// VoxChord D3 experiment: windowed dual-tap shifter vs TD-PSOLA, offline A/B.
//
// Runs SimpleChoirEngine::render() (current windowed shifter, 1 voice, character
// off, spread 0) and two PsolaShifter configurations (Q = quality grains up to
// the output period; LL = low-latency grains fixed at one input period) against
// the same synthetic inputs, driven by the same detected input pitch (the
// engine's own detector output feeds both PSOLA instances).
//
// Inputs:  steady sines (440/220/110 Hz) and a synthetic vowel (impulse train
//          through 700/1200/2600 Hz resonators) at D3/D4.
// Targets: MIDI notes at semitone offsets around the input.
// Metrics: measured output f0 (cents error + stability), sine SNR, amplitude
//          modulation depth, autocorrelation HNR, dominant spectral peak
//          (formant preservation), input->output latency (unison runs), CPU.
//
// Diagnostic tool only; does not touch the plugin build or any DSP behaviour.

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>

#include "../Source/SimpleChoirEngine.h"
#include "../Source/MidiVoiceState.h"
#include "../Source/PsolaShifter.h"

namespace
{
constexpr double kSampleRate = 44100.0;
constexpr int kBlockSize = 512;
constexpr double kRunSeconds = 4.0;
constexpr double kMeasureFromSeconds = 2.0;
constexpr float kInputAmplitude = 0.35f;
constexpr double kPi = 3.14159265358979323846;

double midiNoteToHz (int midiNote) noexcept
{
    return 440.0 * std::pow (2.0, static_cast<double> (midiNote - 69) / 12.0);
}

// ---------------------------------------------------------------------------
// Signal generators
// ---------------------------------------------------------------------------

struct SineGen
{
    void init (double f0Hz)
    {
        phase = 0.0;
        increment = 2.0 * kPi * f0Hz / kSampleRate;
    }

    float next()
    {
        const auto s = kInputAmplitude * static_cast<float> (std::sin (phase));
        phase += increment;

        if (phase > 2.0 * kPi)
            phase -= 2.0 * kPi;

        return s;
    }

    double phase = 0.0;
    double increment = 0.0;
};

struct VowelGen
{
    struct Resonator
    {
        void init (double frequencyHz, double bandwidthHz, double gain)
        {
            const auto r = std::exp (-kPi * bandwidthHz / kSampleRate);
            a1 = 2.0 * r * std::cos (2.0 * kPi * frequencyHz / kSampleRate);
            a2 = -r * r;
            g = gain * (1.0 - r);
            y1 = 0.0;
            y2 = 0.0;
        }

        double tick (double x)
        {
            const auto y = g * x + a1 * y1 + a2 * y2;
            y2 = y1;
            y1 = y;
            return y;
        }

        double a1 = 0.0, a2 = 0.0, g = 1.0, y1 = 0.0, y2 = 0.0;
    };

    void init (double f0Hz, double vibratoDepthCents = 0.0, double vibratoRateHz = 5.0)
    {
        phase = 0.0;
        carry = 0.0;
        baseIncrement = f0Hz / kSampleRate;
        increment = baseIncrement;
        vibratoCents = vibratoDepthCents;
        vibratoRate = vibratoRateHz;
        vibratoPhase = 0.0;
        f1.init (700.0, 130.0, 1.0);
        f2.init (1200.0, 150.0, 0.6);
        f3.init (2600.0, 250.0, 0.35);

        // Normalize peak amplitude over one second of warm-up.
        scale = 1.0f;
        auto peak = 0.0f;
        auto probe = *this;

        for (auto i = 0; i < static_cast<int> (kSampleRate); ++i)
            peak = std::max (peak, std::abs (probe.rawNext()));

        scale = peak > 0.0f ? kInputAmplitude / peak : 1.0f;
    }

    float rawNext()
    {
        if (vibratoCents > 0.0)
        {
            increment = baseIncrement * std::exp2 (vibratoCents * std::sin (vibratoPhase) / 1200.0);
            vibratoPhase += 2.0 * kPi * vibratoRate / kSampleRate;

            if (vibratoPhase > 2.0 * kPi)
                vibratoPhase -= 2.0 * kPi;
        }

        auto x = carry;
        carry = 0.0;
        phase += increment;

        if (phase >= 1.0)
        {
            phase -= 1.0;
            const auto d = phase / increment; // samples since the exact pulse instant
            x += 1.0 - d;
            carry = d;
        }

        return static_cast<float> (f1.tick (x) + f2.tick (x) + f3.tick (x));
    }

    float next() { return scale * rawNext(); }

    double phase = 0.0;
    double increment = 0.0;
    double baseIncrement = 0.0;
    double carry = 0.0;
    double vibratoCents = 0.0;
    double vibratoRate = 5.0;
    double vibratoPhase = 0.0;
    float scale = 1.0f;
    Resonator f1, f2, f3;
};

// ---------------------------------------------------------------------------
// Metrics
// ---------------------------------------------------------------------------

struct F0Stats
{
    bool valid = false;
    double meanHz = 0.0;
    double meanCents = 0.0;
    double stdCents = 0.0;
};

F0Stats measureF0 (const std::vector<float>& x, int start, int end, double targetHz)
{
    F0Stats stats;

    if (targetHz <= 0.0)
        return stats;

    const auto lagMin = std::max (8, static_cast<int> (kSampleRate / (targetHz * 2.5)));
    const auto lagMax = std::min (static_cast<int> (kSampleRate / std::max (20.0, targetHz / 2.5)),
                                  static_cast<int> (kSampleRate / 20.0));

    if (lagMax <= lagMin + 4)
        return stats;

    const auto frame = std::max (8192, lagMax * 3);
    const auto inner = frame - lagMax;
    const auto hop = 4096;

    std::vector<double> centsValues;
    std::vector<double> hzValues;

    for (auto p = start; p + frame <= end; p += hop)
    {
        auto energy0 = 0.0;

        for (auto i = 0; i < inner; ++i)
            energy0 += static_cast<double> (x[static_cast<size_t> (p + i)]) * x[static_cast<size_t> (p + i)];

        if (energy0 < 1.0e-9)
            continue;

        std::vector<double> corr (static_cast<size_t> (lagMax + 2), 0.0);
        auto globalMax = -1.0;

        for (auto lag = lagMin; lag <= lagMax; ++lag)
        {
            auto sum = 0.0;
            auto energyLag = 0.0;

            for (auto i = 0; i < inner; ++i)
            {
                const auto a = static_cast<double> (x[static_cast<size_t> (p + i)]);
                const auto b = static_cast<double> (x[static_cast<size_t> (p + i + lag)]);
                sum += a * b;
                energyLag += b * b;
            }

            const auto denominator = std::sqrt (energy0 * energyLag);
            corr[static_cast<size_t> (lag)] = denominator > 1.0e-12 ? sum / denominator : 0.0;
            globalMax = std::max (globalMax, corr[static_cast<size_t> (lag)]);
        }

        if (globalMax < 0.3)
            continue;

        // Smallest-lag local peak within 10% of the global maximum (octave guard).
        auto chosenLag = -1;

        for (auto lag = lagMin + 1; lag < lagMax; ++lag)
        {
            const auto c = corr[static_cast<size_t> (lag)];

            if (c >= 0.9 * globalMax
                && c >= corr[static_cast<size_t> (lag - 1)]
                && c >= corr[static_cast<size_t> (lag + 1)])
            {
                chosenLag = lag;
                break;
            }
        }

        if (chosenLag < 0)
            continue;

        const auto cPrev = corr[static_cast<size_t> (chosenLag - 1)];
        const auto cHere = corr[static_cast<size_t> (chosenLag)];
        const auto cNext = corr[static_cast<size_t> (chosenLag + 1)];
        const auto denom = cPrev - 2.0 * cHere + cNext;
        const auto offset = std::abs (denom) > 1.0e-12
                              ? std::clamp (0.5 * (cPrev - cNext) / denom, -0.5, 0.5)
                              : 0.0;
        const auto hz = kSampleRate / (static_cast<double> (chosenLag) + offset);

        hzValues.push_back (hz);
        centsValues.push_back (1200.0 * std::log2 (hz / targetHz));
    }

    if (hzValues.empty())
        return stats;

    auto hzSum = 0.0;
    auto centsSum = 0.0;

    for (size_t i = 0; i < hzValues.size(); ++i)
    {
        hzSum += hzValues[i];
        centsSum += centsValues[i];
    }

    const auto n = static_cast<double> (hzValues.size());
    stats.meanHz = hzSum / n;
    stats.meanCents = centsSum / n;

    auto varianceSum = 0.0;

    for (auto c : centsValues)
        varianceSum += (c - stats.meanCents) * (c - stats.meanCents);

    stats.stdCents = std::sqrt (varianceSum / n);
    stats.valid = true;
    return stats;
}

// Least-squares sine fit at fHz; SNR of the fitted component vs the residual.
double sineSNRdB (const std::vector<float>& x, int start, int end, double fHz)
{
    if (fHz <= 0.0 || end - start < 1024)
        return 0.0;

    const auto w = 2.0 * kPi * fHz / kSampleRate;
    double ss = 0.0, cc = 0.0, sc = 0.0, xs = 0.0, xc = 0.0;

    for (auto i = start; i < end; ++i)
    {
        const auto s = std::sin (w * i);
        const auto c = std::cos (w * i);
        const auto v = static_cast<double> (x[static_cast<size_t> (i)]);
        ss += s * s;
        cc += c * c;
        sc += s * c;
        xs += v * s;
        xc += v * c;
    }

    const auto det = ss * cc - sc * sc;

    if (std::abs (det) < 1.0e-9)
        return 0.0;

    const auto a = (xs * cc - xc * sc) / det;
    const auto b = (xc * ss - xs * sc) / det;

    auto signalPower = 0.0;
    auto residualPower = 0.0;

    for (auto i = start; i < end; ++i)
    {
        const auto fit = a * std::sin (w * i) + b * std::cos (w * i);
        const auto res = static_cast<double> (x[static_cast<size_t> (i)]) - fit;
        signalPower += fit * fit;
        residualPower += res * res;
    }

    if (residualPower < 1.0e-15)
        return 99.0;

    return 10.0 * std::log10 (std::max (signalPower, 1.0e-15) / residualPower);
}

// Amplitude modulation depth: sliding RMS (window = 2 periods, hop = period/4).
double amDepthPercent (const std::vector<float>& x, int start, int end, double fHz)
{
    if (fHz <= 0.0)
        return 0.0;

    const auto period = std::max (16, static_cast<int> (kSampleRate / fHz));
    const auto window = 2 * period;
    const auto hop = std::max (4, period / 4);

    auto minRms = 1.0e30;
    auto maxRms = 0.0;
    auto any = false;

    for (auto p = start; p + window <= end; p += hop)
    {
        auto sum = 0.0;

        for (auto i = 0; i < window; ++i)
            sum += static_cast<double> (x[static_cast<size_t> (p + i)]) * x[static_cast<size_t> (p + i)];

        const auto rms = std::sqrt (sum / window);
        minRms = std::min (minRms, rms);
        maxRms = std::max (maxRms, rms);
        any = true;
    }

    if (! any || maxRms + minRms < 1.0e-12)
        return 0.0;

    return 100.0 * (maxRms - minRms) / (maxRms + minRms);
}

// Autocorrelation HNR at the measured period (rho / (1 - rho)).
double acHnrDb (const std::vector<float>& x, int start, int end, double fHz)
{
    if (fHz <= 0.0)
        return 0.0;

    const auto lag = static_cast<int> (std::lround (kSampleRate / fHz));
    const auto inner = std::min (end - start - lag - 2, 32768);

    if (lag < 8 || inner < 1024)
        return 0.0;

    auto best = -1.0;

    for (auto candidate = lag - 2; candidate <= lag + 2; ++candidate)
    {
        auto sum = 0.0, e0 = 0.0, e1 = 0.0;

        for (auto i = 0; i < inner; ++i)
        {
            const auto a = static_cast<double> (x[static_cast<size_t> (start + i)]);
            const auto b = static_cast<double> (x[static_cast<size_t> (start + i + candidate)]);
            sum += a * b;
            e0 += a * a;
            e1 += b * b;
        }

        const auto denominator = std::sqrt (e0 * e1);

        if (denominator > 1.0e-12)
            best = std::max (best, sum / denominator);
    }

    const auto rho = std::clamp (best, 0.0, 0.9999);

    if (rho <= 0.0)
        return 0.0;

    return 10.0 * std::log10 (rho / (1.0 - rho));
}

// Goertzel power at fHz over a Hann-windowed segment.
double goertzelPower (const std::vector<float>& x, int start, int length, double fHz)
{
    const auto w = 2.0 * kPi * fHz / kSampleRate;
    const auto coefficient = 2.0 * std::cos (w);
    double s0 = 0.0, s1 = 0.0, s2 = 0.0;

    for (auto i = 0; i < length; ++i)
    {
        const auto hann = 0.5 - 0.5 * std::cos (2.0 * kPi * i / (length - 1));
        s0 = hann * static_cast<double> (x[static_cast<size_t> (start + i)]) + coefficient * s1 - s2;
        s2 = s1;
        s1 = s0;
    }

    return s1 * s1 + s2 * s2 - coefficient * s1 * s2;
}

// Band power approximated by summing Goertzel powers on a Hann-windowed segment.
double bandPowerGoertzel (const std::vector<float>& x, int start, int length,
                          double fromHz, double toHz, double stepHz)
{
    auto sum = 0.0;

    for (auto f = fromHz; f <= toHz; f += stepHz)
        sum += goertzelPower (x, start, length, f);

    return sum;
}

// Low-frequency noise indicator: power below lfCutoffHz relative to the broad
// band (25 Hz .. 5 kHz), both computed the same way so the ratio is comparable
// across runs. Mark-jitter artifacts in PSOLA land below the fundamentals.
double lfNoiseDb (const std::vector<float>& x, int start, int end, double lfCutoffHz)
{
    const auto length = std::min (16384, end - start);

    if (length < 4096 || lfCutoffHz <= 30.0)
        return 0.0;

    const auto lf = bandPowerGoertzel (x, start, length, 25.0, lfCutoffHz, 5.0);
    const auto broad = bandPowerGoertzel (x, start, length, 25.0, 5000.0, 25.0);

    if (broad < 1.0e-12)
        return 0.0;

    return 10.0 * std::log10 (std::max (lf, 1.0e-15) / broad);
}

double rmsOf (const std::vector<float>& x, int start, int end)
{
    auto sum = 0.0;

    for (auto i = start; i < end; ++i)
        sum += static_cast<double> (x[static_cast<size_t> (i)]) * x[static_cast<size_t> (i)];

    return std::sqrt (sum / std::max (1, end - start));
}

// Dominant spectral peak between 250 and 3400 Hz (formant preservation probe).
double dominantPeakHz (const std::vector<float>& x, int start, int end)
{
    const auto length = std::min (16384, end - start);

    if (length < 4096)
        return 0.0;

    auto bestHz = 0.0;
    auto bestPower = -1.0;

    for (auto f = 250.0; f <= 3400.0; f += 10.0)
    {
        const auto p = goertzelPower (x, start, length, f);

        if (p > bestPower)
        {
            bestPower = p;
            bestHz = f;
        }
    }

    return bestHz;
}

// Input -> output delay via cross-correlation of the sliding-RMS *envelopes*.
// The waveforms themselves are periodic (correlation peaks at every period);
// the 3 Hz AM imposed on the dedicated latency runs makes the envelope
// unambiguous within the searched lag range.
double measureLatencyMs (const std::vector<float>& input, const std::vector<float>& output,
                         int start, int maxLagSamples)
{
    constexpr auto hop = 32;
    constexpr auto window = 256;
    const auto end = static_cast<int> (std::min (input.size(), output.size())) - window;

    if (start + maxLagSamples + 8192 >= end)
        return -1.0;

    auto envelopeAt = [=] (const std::vector<float>& x, int position)
    {
        auto sum = 0.0;

        for (auto i = 0; i < window; ++i)
            sum += static_cast<double> (x[static_cast<size_t> (position + i)]) * x[static_cast<size_t> (position + i)];

        return std::sqrt (sum / window);
    };

    const auto hops = (end - start - maxLagSamples) / hop;
    std::vector<double> inputEnvelope (static_cast<size_t> (hops));
    std::vector<double> outputEnvelope (static_cast<size_t> (hops + maxLagSamples / hop + 1));

    for (auto i = 0; i < hops; ++i)
        inputEnvelope[static_cast<size_t> (i)] = envelopeAt (input, start + i * hop);

    for (auto i = 0; i < static_cast<int> (outputEnvelope.size()); ++i)
        outputEnvelope[static_cast<size_t> (i)] = envelopeAt (output, start + i * hop);

    auto mean = [] (const std::vector<double>& v, int from, int count)
    {
        auto sum = 0.0;

        for (auto i = from; i < from + count; ++i)
            sum += v[static_cast<size_t> (i)];

        return sum / count;
    };

    const auto inputMean = mean (inputEnvelope, 0, hops);
    const auto maxLagHops = maxLagSamples / hop;
    auto bestLag = 0;
    auto bestCorr = -1.0e30;

    for (auto lag = 0; lag <= maxLagHops; ++lag)
    {
        const auto outputMean = mean (outputEnvelope, lag, hops);
        auto sum = 0.0;
        auto outputVariance = 0.0;

        for (auto i = 0; i < hops; ++i)
        {
            const auto a = inputEnvelope[static_cast<size_t> (i)] - inputMean;
            const auto b = outputEnvelope[static_cast<size_t> (i + lag)] - outputMean;
            sum += a * b;
            outputVariance += b * b;
        }

        const auto corr = outputVariance > 1.0e-12 ? sum / std::sqrt (outputVariance) : 0.0;

        if (corr > bestCorr)
        {
            bestCorr = corr;
            bestLag = lag;
        }
    }

    return 1000.0 * bestLag * hop / kSampleRate;
}

// Wraps a generator with slow AM so the envelope correlator has a feature to lock onto.
template <typename Generator>
struct AmWrap
{
    float next()
    {
        const auto m = 0.75 + 0.25 * std::sin (2.0 * kPi * 3.0 * t / kSampleRate);
        t += 1.0;
        return static_cast<float> (m) * generator.next();
    }

    Generator generator;
    double t = 0.0;
};

// ---------------------------------------------------------------------------
// Run driver
// ---------------------------------------------------------------------------

struct RunOutputs
{
    std::vector<float> dry;
    std::vector<float> engine;
    std::vector<float> psolaA;   // symmetric grains (factor 1.0) = 0.3.4 baseline
    std::vector<float> psolaB75; // right half 0.75 * left
    std::vector<float> psolaB50; // right half 0.50 * left
    double detectedInputHz = 0.0;
    int psolaALatency = 0;
    int psolaB75Latency = 0;
    int psolaB50Latency = 0;
};

// Mirrors SimpleChoirEngine's tracking highpass on the PSOLA voice path
// (cutoff 0.6 * target f0, RBJ highpass, Q 0.707): sub-fundamental output is
// artifact energy by construction. Kept in the harness so the measurements
// match what the plugin actually plays (directions/0708_5.md item 2).
struct TrackingHighpass
{
    void configure (double cutoffHz)
    {
        const auto safeFrequency = std::clamp (cutoffHz, 30.0, 1000.0);
        const auto omega = 2.0 * kPi * safeFrequency / kSampleRate;
        const auto sinOmega = std::sin (omega);
        const auto cosOmega = std::cos (omega);
        const auto alpha = sinOmega / (2.0 * 0.707);
        const auto a0 = 1.0 + alpha;
        b0 = ((1.0 + cosOmega) * 0.5) / a0;
        b1 = (-(1.0 + cosOmega)) / a0;
        b2 = b0;
        a1 = (-2.0 * cosOmega) / a0;
        a2 = (1.0 - alpha) / a0;
    }

    float process (float x)
    {
        const auto y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return static_cast<float> (y);
    }

    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0, z1 = 0.0, z2 = 0.0;
};

template <typename Generator>
void runOnce (Generator& generator, double inputF0Hz, int midiNote, RunOutputs& outputs)
{
    const auto totalSamples = static_cast<int> (kRunSeconds * kSampleRate);
    const auto totalBlocks = totalSamples / kBlockSize;

    outputs.dry.assign (static_cast<size_t> (totalBlocks) * kBlockSize, 0.0f);
    outputs.engine.assign (outputs.dry.size(), 0.0f);
    outputs.psolaA.assign (outputs.dry.size(), 0.0f);
    outputs.psolaB75.assign (outputs.dry.size(), 0.0f);
    outputs.psolaB50.assign (outputs.dry.size(), 0.0f);

    voxchord::SimpleChoirEngine engine;
    engine.prepare (kSampleRate, kBlockSize);

    const auto targetHz = midiNoteToHz (midiNote);
    const auto nominalRatio = targetHz / inputF0Hz;

    voxchord::PsolaShifter psolaA;
    voxchord::PsolaShifter psolaB75;
    voxchord::PsolaShifter psolaB50;
    const auto minF0 = static_cast<float> (inputF0Hz * 0.8);
    const auto minRatio = static_cast<float> (std::min (nominalRatio * 0.9, 1.0));
    psolaA.prepare (kSampleRate, minF0, minRatio, 1.0f, 1.0f);
    psolaB75.prepare (kSampleRate, minF0, minRatio, 1.0f, 0.75f);
    psolaB50.prepare (kSampleRate, minF0, minRatio, 1.0f, 0.5f);
    outputs.psolaALatency = psolaA.getLatencySamples();
    outputs.psolaB75Latency = psolaB75.getLatencySamples();
    outputs.psolaB50Latency = psolaB50.getLatencySamples();

    juce::AudioBuffer<float> dry (2, kBlockSize);
    juce::AudioBuffer<float> wet (2, kBlockSize);
    juce::AudioBuffer<float> lead (2, kBlockSize);

    voxchord::MidiVoiceState::NoteSnapshot notes {};
    notes.fill (-1);
    notes[0] = midiNote;

    std::array<float, kBlockSize> mono {};
    std::array<float, kBlockSize> psolaOut {};
    TrackingHighpass highpassA, highpassB75, highpassB50;

    for (auto block = 0; block < totalBlocks; ++block)
    {
        for (auto n = 0; n < kBlockSize; ++n)
        {
            const auto s = generator.next();
            mono[static_cast<size_t> (n)] = s;
            dry.setSample (0, n, s);
            dry.setSample (1, n, s);
            outputs.dry[static_cast<size_t> (block * kBlockSize + n)] = s;
        }

        engine.render (dry, wet, lead, notes,
                       /*voiceLimit*/ 4,
                       /*spread*/ 0.0f,
                       /*tune*/ 0.8f,
                       /*glide*/ 0.0f,
                       /*characterMode*/ 0,
                       /*characterAmountRaw*/ 0.0f,
                       /*characterAmountSmoothed*/ 0.0f,
                       /*leadTuneEnabled*/ false);

        const auto detected = engine.getLastDetectedInputFrequencyHz();

        if (detected > 0.0f)
        {
            outputs.detectedInputHz = detected;
            const auto ratio = static_cast<float> (targetHz / detected);

            for (auto* shifter : { &psolaA, &psolaB75, &psolaB50 })
            {
                shifter->setInputPitchHz (detected);
                shifter->setTargetPitchRatio (ratio);
            }

            for (auto* highpass : { &highpassA, &highpassB75, &highpassB50 })
                highpass->configure (0.6 * targetHz);
        }

        for (auto n = 0; n < kBlockSize; ++n)
            outputs.engine[static_cast<size_t> (block * kBlockSize + n)] = wet.getSample (0, n);

        const std::tuple<voxchord::PsolaShifter*, TrackingHighpass*, std::vector<float>*> shifterOutputs[] = {
            { &psolaA, &highpassA, &outputs.psolaA },
            { &psolaB75, &highpassB75, &outputs.psolaB75 },
            { &psolaB50, &highpassB50, &outputs.psolaB50 },
        };

        for (const auto& [shifter, highpass, destination] : shifterOutputs)
        {
            shifter->processBlock (mono.data(), psolaOut.data(), kBlockSize);

            for (auto n = 0; n < kBlockSize; ++n)
                (*destination)[static_cast<size_t> (block * kBlockSize + n)] = highpass->process (psolaOut[static_cast<size_t> (n)]);
        }
    }
}

struct MetricsRow
{
    F0Stats f0;
    double snrDb = 0.0;
    double amPercent = 0.0;
    double hnrDb = 0.0;
    double peakHz = 0.0;
    double rmsRelDryDb = 0.0;
    double lfDb = 0.0;
    bool hasVowelExtras = false;
};

MetricsRow analyse (const std::vector<float>& x, double targetHz, bool isSine,
                    double dryRms = 0.0, double lfCutoffHz = 0.0)
{
    MetricsRow row;
    const auto start = static_cast<int> (kMeasureFromSeconds * kSampleRate);
    const auto end = static_cast<int> (x.size());

    row.f0 = measureF0 (x, start, end, targetHz);
    const auto fitHz = row.f0.valid ? row.f0.meanHz : targetHz;
    row.amPercent = amDepthPercent (x, start, end, fitHz);

    if (isSine)
        row.snrDb = sineSNRdB (x, start, end, fitHz);
    else
    {
        row.hnrDb = acHnrDb (x, start, end, fitHz);
        row.peakHz = dominantPeakHz (x, start, end);

        if (dryRms > 1.0e-9)
        {
            row.rmsRelDryDb = 20.0 * std::log10 (std::max (rmsOf (x, start, end), 1.0e-9) / dryRms);
            row.lfDb = lfNoiseDb (x, start, end, lfCutoffHz);
            row.hasVowelExtras = true;
        }
    }

    return row;
}

void printRow (const char* label, const MetricsRow& row, bool isSine, int latencySamples, double measuredLatencyMs)
{
    std::printf ("    %-8s ", label);

    if (row.f0.valid)
        std::printf ("f0 %8.2f Hz (%+7.1f c, sd %6.1f c)", row.f0.meanHz, row.f0.meanCents, row.f0.stdCents);
    else
        std::printf ("f0   ------ Hz (   ----  ,     ---- )");

    std::printf ("  AM %5.1f%%", row.amPercent);

    if (isSine)
        std::printf ("  SNR %6.1f dB", row.snrDb);
    else
        std::printf ("  HNR %6.1f dB  peak %6.0f Hz", row.hnrDb, row.peakHz);

    if (row.hasVowelExtras)
        std::printf ("  RMS %+6.1f dB  LF %6.1f dB", row.rmsRelDryDb, row.lfDb);

    if (latencySamples > 0)
        std::printf ("  lat %5.1f ms", 1000.0 * latencySamples / kSampleRate);

    if (measuredLatencyMs >= 0.0)
        std::printf ("  (meas %5.1f ms)", measuredLatencyMs);

    std::printf ("\n");
}

// ---------------------------------------------------------------------------
// Character probe (directions/0708_6.md): vowel input + 4-note chord through
// the Classic engine, one run per character mode at amount 1.0 vs a baseline
// at amount 0. Band-power deltas show each mode's spectral signature.
// ---------------------------------------------------------------------------

struct CharacterProbeResult
{
    double bandDb[4] = {};   // 200-500 / 900-1800 / 2500-5500 / 6500-11000 Hz, rel. baseline
    double hnrDb = 0.0;
    double rmsRelBaselineDb = 0.0;
};

void runCharacterProbe (int characterMode, float amount, std::vector<float>& wetOut)
{
    voxchord::SimpleChoirEngine engine;
    engine.prepare (kSampleRate, kBlockSize);

    VowelGen generator;
    generator.init (146.83);

    juce::AudioBuffer<float> dry (2, kBlockSize);
    juce::AudioBuffer<float> wet (2, kBlockSize);
    juce::AudioBuffer<float> lead (2, kBlockSize);

    voxchord::MidiVoiceState::NoteSnapshot notes {};
    notes.fill (-1);
    notes[0] = 50; // D3
    notes[1] = 54;
    notes[2] = 57;
    notes[3] = 62;

    const auto totalBlocks = static_cast<int> (kRunSeconds * kSampleRate) / kBlockSize;
    wetOut.assign (static_cast<size_t> (totalBlocks) * kBlockSize, 0.0f);

    for (auto block = 0; block < totalBlocks; ++block)
    {
        for (auto n = 0; n < kBlockSize; ++n)
        {
            const auto s = generator.next();
            dry.setSample (0, n, s);
            dry.setSample (1, n, s);
        }

        engine.render (dry, wet, lead, notes, 4, 0.0f, 0.8f, 0.0f,
                       characterMode, amount, amount, false);

        for (auto n = 0; n < kBlockSize; ++n)
            wetOut[static_cast<size_t> (block * kBlockSize + n)] = wet.getSample (0, n);
    }
}

void printCharacterProbe()
{
    static constexpr double bandEdges[4][2] = {
        { 200.0, 500.0 }, { 900.0, 1800.0 }, { 2500.0, 5500.0 }, { 6500.0, 11000.0 }
    };
    static const char* modeNames[4] = { "Warm", "Bright", "Vowel", "Digital" };

    std::printf ("=== Character probe (vowel 146.83 Hz, chord MIDI 50/54/57/62, Classic engine) ===\n");
    std::printf ("    band deltas vs amount-0 baseline [dB]: lowmid 200-500 | formant 900-1800 | presence 2.5-5.5k | air 6.5-11k\n");

    std::vector<float> baseline;
    runCharacterProbe (1, 0.0f, baseline);

    const auto start = static_cast<int> (kMeasureFromSeconds * kSampleRate);
    const auto length = std::min (16384, static_cast<int> (baseline.size()) - start);

    double baselineBands[4];

    for (auto b = 0; b < 4; ++b)
        baselineBands[b] = bandPowerGoertzel (baseline, start, length, bandEdges[b][0], bandEdges[b][1], 25.0);

    const auto baselineRms = rmsOf (baseline, start, static_cast<int> (baseline.size()));
    const auto baselineHnr = acHnrDb (baseline, start, static_cast<int> (baseline.size()), 146.83);
    std::printf ("    baseline (amount 0): HNR %5.1f dB\n", baselineHnr);

    for (auto mode = 1; mode <= 4; ++mode)
    {
        std::vector<float> wet;
        runCharacterProbe (mode, 1.0f, wet);

        CharacterProbeResult result;

        for (auto b = 0; b < 4; ++b)
        {
            const auto power = bandPowerGoertzel (wet, start, length, bandEdges[b][0], bandEdges[b][1], 25.0);
            result.bandDb[b] = 10.0 * std::log10 (std::max (power, 1.0e-15) / std::max (baselineBands[b], 1.0e-15));
        }

        result.hnrDb = acHnrDb (wet, start, static_cast<int> (wet.size()), 146.83);
        result.rmsRelBaselineDb = 20.0 * std::log10 (std::max (rmsOf (wet, start, static_cast<int> (wet.size())), 1.0e-9)
                                                     / std::max (baselineRms, 1.0e-9));

        std::printf ("    %-8s lowmid %+6.1f | formant %+6.1f | presence %+6.1f | air %+6.1f | HNR %5.1f dB | RMS %+5.1f dB\n",
                     modeNames[mode - 1],
                     result.bandDb[0], result.bandDb[1], result.bandDb[2], result.bandDb[3],
                     result.hnrDb, result.rmsRelBaselineDb);
    }

    std::printf ("\n");
}

// ---------------------------------------------------------------------------
// CPU benchmark
// ---------------------------------------------------------------------------

double benchEngineMsPerSecond (int activeNotes)
{
    voxchord::SimpleChoirEngine engine;
    engine.prepare (kSampleRate, kBlockSize);

    VowelGen generator;
    generator.init (146.83);

    juce::AudioBuffer<float> dry (2, kBlockSize);
    juce::AudioBuffer<float> wet (2, kBlockSize);
    juce::AudioBuffer<float> lead (2, kBlockSize);

    voxchord::MidiVoiceState::NoteSnapshot notes {};
    notes.fill (-1);

    const int noteChoices[4] = { 62, 57, 53, 45 };

    for (auto i = 0; i < activeNotes && i < 4; ++i)
        notes[static_cast<size_t> (i)] = noteChoices[i];

    const auto seconds = 10.0;
    const auto totalBlocks = static_cast<int> (seconds * kSampleRate / kBlockSize);

    const auto begin = std::chrono::steady_clock::now();

    for (auto block = 0; block < totalBlocks; ++block)
    {
        for (auto n = 0; n < kBlockSize; ++n)
        {
            const auto s = generator.next();
            dry.setSample (0, n, s);
            dry.setSample (1, n, s);
        }

        engine.render (dry, wet, lead, notes, 4, 0.5f, 0.8f, 0.0f, 0, 0.0f, 0.0f, false);
    }

    const auto elapsed = std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - begin).count();
    return elapsed / seconds;
}

double benchPsolaMsPerSecond (float ratio, float rightHalfFactor)
{
    voxchord::PsolaShifter shifter;
    shifter.prepare (kSampleRate, 100.0f, std::min (ratio, 1.0f), 1.0f, rightHalfFactor);
    shifter.setInputPitchHz (146.83f);
    shifter.setTargetPitchRatio (ratio);

    VowelGen generator;
    generator.init (146.83);

    std::array<float, kBlockSize> in {};
    std::array<float, kBlockSize> out {};

    const auto seconds = 10.0;
    const auto totalBlocks = static_cast<int> (seconds * kSampleRate / kBlockSize);

    const auto begin = std::chrono::steady_clock::now();

    for (auto block = 0; block < totalBlocks; ++block)
    {
        for (auto n = 0; n < kBlockSize; ++n)
            in[static_cast<size_t> (n)] = generator.next();

        shifter.processBlock (in.data(), out.data(), kBlockSize);
    }

    const auto elapsed = std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - begin).count();
    return elapsed / seconds;
}

} // namespace

// ---------------------------------------------------------------------------

int main()
{
    std::printf ("VoxChord D3 shifter comparison - SR %.0f Hz, block %d, run %.1f s, measure from %.1f s\n",
                 kSampleRate, kBlockSize, kRunSeconds, kMeasureFromSeconds);
    std::printf ("engine   = SimpleChoirEngine windowed dual-tap shifter (ratio clamp 0.25..8)\n");
    std::printf ("psolaA   = TD-PSOLA, symmetric 1-period grains (0.3.4 plan A' baseline)\n");
    std::printf ("psolaB75 = TD-PSOLA, asymmetric grains, right half = 0.75 * left (plan B)\n");
    std::printf ("psolaB50 = TD-PSOLA, asymmetric grains, right half = 0.50 * left (plan B)\n\n");

    struct Signal
    {
        const char* name;
        bool vowel;
        double f0;
        int baseMidi;
        std::vector<int> offsets;
        double vibratoCents = 0.0; // > 0: +/- cents at 5 Hz (realistic voice movement)
    };

    const std::vector<Signal> signals = {
        { "sine 440", false, 440.00, 69, { 12, 7, 0, -5, -12, -24, -36 } },
        { "sine 220", false, 220.00, 57, { 12, 7, 0, -5, -12, -24 } },
        { "sine 110", false, 110.00, 45, { 12, 7, 0, -5, -12 } },
        { "vowel 147", true, 146.83, 50, { 12, 7, 0, -5, -12 } },
        { "vowel 294", true, 293.66, 62, { 12, 0, -5, -12, -24 } },
        // High-MIDI upshift stress with vocal-like pitch movement: reproduces
        // the reported low-frequency noise case (directions/0708_5.md item 2).
        { "vowel 196 vib", true, 196.00, 55, { 19, 16, 12, 7, 0 }, 30.0 },
    };

    RunOutputs outputs;

    for (const auto& signal : signals)
    {
        std::printf ("=== %s Hz (base MIDI %d) ===\n", signal.name, signal.baseMidi);

        if (signal.vowel)
        {
            VowelGen probe;
            probe.init (signal.f0);
            std::vector<float> dryProbe (static_cast<size_t> (kSampleRate * 2.0), 0.0f);

            for (auto& v : dryProbe)
                v = probe.next();

            std::printf ("    (dry input dominant peak: %.0f Hz)\n",
                         dominantPeakHz (dryProbe, static_cast<int> (kSampleRate), static_cast<int> (dryProbe.size())));
        }

        for (const auto offset : signal.offsets)
        {
            const auto midiNote = signal.baseMidi + offset;
            const auto targetHz = midiNoteToHz (midiNote);

            if (signal.vowel)
            {
                VowelGen generator;
                generator.init (signal.f0, signal.vibratoCents);
                runOnce (generator, signal.f0, midiNote, outputs);
            }
            else
            {
                SineGen generator;
                generator.init (signal.f0);
                runOnce (generator, signal.f0, midiNote, outputs);
            }

            std::printf ("  note %+3d (MIDI %d, target %7.2f Hz, ratio %.4f, detIn %.2f Hz)\n",
                         offset, midiNote, targetHz, targetHz / signal.f0, outputs.detectedInputHz);

            const auto measureStart = static_cast<int> (kMeasureFromSeconds * kSampleRate);
            const auto dryRms = signal.vowel
                                    ? rmsOf (outputs.dry, measureStart, static_cast<int> (outputs.dry.size()))
                                    : 0.0;
            const auto lfCutoffHz = 0.6 * std::min (signal.f0, targetHz);

            const auto engineRow = analyse (outputs.engine, targetHz, ! signal.vowel, dryRms, lfCutoffHz);
            const auto psolaARow = analyse (outputs.psolaA, targetHz, ! signal.vowel, dryRms, lfCutoffHz);
            const auto psolaB75Row = analyse (outputs.psolaB75, targetHz, ! signal.vowel, dryRms, lfCutoffHz);
            const auto psolaB50Row = analyse (outputs.psolaB50, targetHz, ! signal.vowel, dryRms, lfCutoffHz);

            auto engineLatencyMs = -1.0;
            auto psolaALatencyMs = -1.0;
            auto psolaB75LatencyMs = -1.0;
            auto psolaB50LatencyMs = -1.0;

            if (offset == 0)
            {
                // Dedicated latency pass: same run with 3 Hz AM on the input,
                // measured on the RMS envelopes (period-ambiguity free).
                if (signal.vowel)
                {
                    AmWrap<VowelGen> generator;
                    generator.generator.init (signal.f0);
                    runOnce (generator, signal.f0, midiNote, outputs);
                }
                else
                {
                    AmWrap<SineGen> generator;
                    generator.generator.init (signal.f0);
                    runOnce (generator, signal.f0, midiNote, outputs);
                }

                const auto start = static_cast<int> (1.0 * kSampleRate);
                const auto maxLag = std::max (6144, outputs.psolaALatency + 1024);
                engineLatencyMs = measureLatencyMs (outputs.dry, outputs.engine, start, 6144);
                psolaALatencyMs = measureLatencyMs (outputs.dry, outputs.psolaA, start, maxLag);
                psolaB75LatencyMs = measureLatencyMs (outputs.dry, outputs.psolaB75, start, maxLag);
                psolaB50LatencyMs = measureLatencyMs (outputs.dry, outputs.psolaB50, start, maxLag);
            }

            printRow ("engine", engineRow, ! signal.vowel, 0, engineLatencyMs);
            printRow ("psolaA", psolaARow, ! signal.vowel, outputs.psolaALatency, psolaALatencyMs);
            printRow ("psolaB75", psolaB75Row, ! signal.vowel, outputs.psolaB75Latency, psolaB75LatencyMs);
            printRow ("psolaB50", psolaB50Row, ! signal.vowel, outputs.psolaB50Latency, psolaB50LatencyMs);
        }

        std::printf ("\n");
    }

    printCharacterProbe();

    std::printf ("=== CPU (vowel 146.83 Hz input, 10 s each, ms of processing per second of audio) ===\n");
    std::printf ("  engine, 1 note         : %7.3f ms/s (includes YIN pitch detection)\n", benchEngineMsPerSecond (1));
    std::printf ("  engine, 4 notes        : %7.3f ms/s (includes YIN pitch detection)\n", benchEngineMsPerSecond (4));
    std::printf ("  psola x1, ratio 0.5, A  : %7.3f ms/s (shifter only, no detection)\n", benchPsolaMsPerSecond (0.5f, 1.0f));
    std::printf ("  psola x1, ratio 0.5, B50: %7.3f ms/s (shifter only, no detection)\n", benchPsolaMsPerSecond (0.5f, 0.5f));
    std::printf ("  psola x1, ratio 2.0, A  : %7.3f ms/s (shifter only, no detection)\n", benchPsolaMsPerSecond (2.0f, 1.0f));

    std::printf ("\nNotes:\n");
    std::printf ("  f0     = autocorrelation estimate of the output pitch (mean over 4096-hop frames, cents vs target, sd = stability)\n");
    std::printf ("  AM     = amplitude modulation depth of the sliding 2-period RMS envelope\n");
    std::printf ("  SNR    = sine runs: least-squares sine fit at the measured f0 vs residual (distortion + AM sidebands + noise)\n");
    std::printf ("  HNR    = vowel runs: normalized autocorrelation at the measured period, rho/(1-rho)\n");
    std::printf ("  peak   = vowel runs: dominant spectral peak 250..3400 Hz; input has formants 700/1200/2600 Hz.\n");
    std::printf ("           A formant-preserving shifter keeps the peak near the input value; a resampling shifter scales it by the ratio.\n");
    std::printf ("  lat    = PSOLA configured constant latency; (meas) = cross-correlation delay measured on the unison run\n");
    return 0;
}
