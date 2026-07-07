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

    void init (double f0Hz)
    {
        phase = 0.0;
        carry = 0.0;
        increment = f0Hz / kSampleRate;
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
    double carry = 0.0;
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
    std::vector<float> psolaQ;
    std::vector<float> psolaLL;
    double detectedInputHz = 0.0;
    int psolaQLatency = 0;
    int psolaLLLatency = 0;
};

template <typename Generator>
void runOnce (Generator& generator, double inputF0Hz, int midiNote, RunOutputs& outputs)
{
    const auto totalSamples = static_cast<int> (kRunSeconds * kSampleRate);
    const auto totalBlocks = totalSamples / kBlockSize;

    outputs.dry.assign (static_cast<size_t> (totalBlocks) * kBlockSize, 0.0f);
    outputs.engine.assign (outputs.dry.size(), 0.0f);
    outputs.psolaQ.assign (outputs.dry.size(), 0.0f);
    outputs.psolaLL.assign (outputs.dry.size(), 0.0f);

    voxchord::SimpleChoirEngine engine;
    engine.prepare (kSampleRate, kBlockSize);

    const auto targetHz = midiNoteToHz (midiNote);
    const auto nominalRatio = targetHz / inputF0Hz;

    voxchord::PsolaShifter psolaQ;
    voxchord::PsolaShifter psolaLL;
    const auto minF0 = static_cast<float> (inputF0Hz * 0.8);
    const auto minRatio = static_cast<float> (std::min (nominalRatio * 0.9, 1.0));
    psolaQ.prepare (kSampleRate, minF0, minRatio, 2.0f);
    psolaLL.prepare (kSampleRate, minF0, minRatio, 1.0f);
    outputs.psolaQLatency = psolaQ.getLatencySamples();
    outputs.psolaLLLatency = psolaLL.getLatencySamples();

    juce::AudioBuffer<float> dry (2, kBlockSize);
    juce::AudioBuffer<float> wet (2, kBlockSize);
    juce::AudioBuffer<float> lead (2, kBlockSize);

    voxchord::MidiVoiceState::NoteSnapshot notes {};
    notes.fill (-1);
    notes[0] = midiNote;

    std::array<float, kBlockSize> mono {};
    std::array<float, kBlockSize> psolaOut {};

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
            psolaQ.setInputPitchHz (detected);
            psolaLL.setInputPitchHz (detected);
            psolaQ.setTargetPitchRatio (ratio);
            psolaLL.setTargetPitchRatio (ratio);
        }

        for (auto n = 0; n < kBlockSize; ++n)
            outputs.engine[static_cast<size_t> (block * kBlockSize + n)] = wet.getSample (0, n);

        psolaQ.processBlock (mono.data(), psolaOut.data(), kBlockSize);

        for (auto n = 0; n < kBlockSize; ++n)
            outputs.psolaQ[static_cast<size_t> (block * kBlockSize + n)] = psolaOut[static_cast<size_t> (n)];

        psolaLL.processBlock (mono.data(), psolaOut.data(), kBlockSize);

        for (auto n = 0; n < kBlockSize; ++n)
            outputs.psolaLL[static_cast<size_t> (block * kBlockSize + n)] = psolaOut[static_cast<size_t> (n)];
    }
}

struct MetricsRow
{
    F0Stats f0;
    double snrDb = 0.0;
    double amPercent = 0.0;
    double hnrDb = 0.0;
    double peakHz = 0.0;
};

MetricsRow analyse (const std::vector<float>& x, double targetHz, bool isSine)
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
    }

    return row;
}

void printRow (const char* label, const MetricsRow& row, bool isSine, int latencySamples, double measuredLatencyMs)
{
    std::printf ("    %-7s ", label);

    if (row.f0.valid)
        std::printf ("f0 %8.2f Hz (%+7.1f c, sd %6.1f c)", row.f0.meanHz, row.f0.meanCents, row.f0.stdCents);
    else
        std::printf ("f0   ------ Hz (   ----  ,     ---- )");

    std::printf ("  AM %5.1f%%", row.amPercent);

    if (isSine)
        std::printf ("  SNR %6.1f dB", row.snrDb);
    else
        std::printf ("  HNR %6.1f dB  peak %6.0f Hz", row.hnrDb, row.peakHz);

    if (latencySamples > 0)
        std::printf ("  lat %5.1f ms", 1000.0 * latencySamples / kSampleRate);

    if (measuredLatencyMs >= 0.0)
        std::printf ("  (meas %5.1f ms)", measuredLatencyMs);

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

double benchPsolaMsPerSecond (float ratio, float grainCap)
{
    voxchord::PsolaShifter shifter;
    shifter.prepare (kSampleRate, 100.0f, std::min (ratio, 1.0f), grainCap);
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
    std::printf ("engine  = SimpleChoirEngine windowed dual-tap shifter (ratio clamp 0.25..8)\n");
    std::printf ("psolaQ  = TD-PSOLA, grain half-width up to output period (quality mode)\n");
    std::printf ("psolaLL = TD-PSOLA, grain half-width = 1 input period (low-latency mode)\n\n");

    struct Signal
    {
        const char* name;
        bool vowel;
        double f0;
        int baseMidi;
        std::vector<int> offsets;
    };

    const std::vector<Signal> signals = {
        { "sine 440", false, 440.00, 69, { 12, 7, 0, -5, -12, -24, -36 } },
        { "sine 220", false, 220.00, 57, { 12, 7, 0, -5, -12, -24 } },
        { "sine 110", false, 110.00, 45, { 12, 7, 0, -5, -12 } },
        { "vowel 147", true, 146.83, 50, { 12, 7, 0, -5, -12 } },
        { "vowel 294", true, 293.66, 62, { 12, 0, -5, -12, -24 } },
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
                generator.init (signal.f0);
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

            const auto engineRow = analyse (outputs.engine, targetHz, ! signal.vowel);
            const auto psolaQRow = analyse (outputs.psolaQ, targetHz, ! signal.vowel);
            const auto psolaLLRow = analyse (outputs.psolaLL, targetHz, ! signal.vowel);

            auto engineLatencyMs = -1.0;
            auto psolaQLatencyMs = -1.0;
            auto psolaLLLatencyMs = -1.0;

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
                const auto maxLag = std::max (6144, outputs.psolaQLatency + 1024);
                engineLatencyMs = measureLatencyMs (outputs.dry, outputs.engine, start, 6144);
                psolaQLatencyMs = measureLatencyMs (outputs.dry, outputs.psolaQ, start, maxLag);
                psolaLLLatencyMs = measureLatencyMs (outputs.dry, outputs.psolaLL, start, maxLag);
            }

            printRow ("engine", engineRow, ! signal.vowel, 0, engineLatencyMs);
            printRow ("psolaQ", psolaQRow, ! signal.vowel, outputs.psolaQLatency, psolaQLatencyMs);
            printRow ("psolaLL", psolaLLRow, ! signal.vowel, outputs.psolaLLLatency, psolaLLLatencyMs);
        }

        std::printf ("\n");
    }

    std::printf ("=== CPU (vowel 146.83 Hz input, 10 s each, ms of processing per second of audio) ===\n");
    std::printf ("  engine, 1 note         : %7.3f ms/s (includes YIN pitch detection)\n", benchEngineMsPerSecond (1));
    std::printf ("  engine, 4 notes        : %7.3f ms/s (includes YIN pitch detection)\n", benchEngineMsPerSecond (4));
    std::printf ("  psola x1, ratio 0.5, Q : %7.3f ms/s (shifter only, no detection)\n", benchPsolaMsPerSecond (0.5f, 16.0f));
    std::printf ("  psola x1, ratio 0.5, LL: %7.3f ms/s (shifter only, no detection)\n", benchPsolaMsPerSecond (0.5f, 1.0f));
    std::printf ("  psola x1, ratio 2.0    : %7.3f ms/s (shifter only, no detection)\n", benchPsolaMsPerSecond (2.0f, 16.0f));

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
