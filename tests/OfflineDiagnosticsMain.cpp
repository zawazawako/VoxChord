// VoxChord offline D1 low-pitch diagnostics harness.
//
// Drives SimpleChoirEngine::render() headlessly with a steady sine input and a
// single held MIDI note, then prints the D1 diagnostic PitchState for each
// target note. This automates experiment #2 from directions/0703_1.md
// (sine wave + descending MIDI notes) so the shifter-side numbers can be read
// without a DAW/Standalone. Real-voice behaviour (experiment #3) still requires
// user testing; a pure sine only exercises the detection + shifter path.
//
// This target is a diagnostic tool only. It links the engine sources directly
// and does not touch the plugin build or any DSP behaviour.

#include <array>
#include <cmath>
#include <cstdio>
#include <string>

#include <juce_audio_basics/juce_audio_basics.h>

#include "../Source/SimpleChoirEngine.h"
#include "../Source/MidiVoiceState.h"

namespace
{
constexpr double kSampleRate = 44100.0;
constexpr int kBlockSize = 512;
constexpr float kInputFrequencyHz = 440.0f; // steady sine, as in experiment #2
constexpr float kInputAmplitude = 0.35f;
constexpr double kSettleSeconds = 2.0; // let detection + shifter + zero-crossing settle

float midiNoteToHz (int midiNote) noexcept
{
    return 440.0f * std::pow (2.0f, static_cast<float> (midiNote - 69) / 12.0f);
}

void runForNote (int midiNote)
{
    voxchord::SimpleChoirEngine engine;
    engine.prepare (kSampleRate, kBlockSize);

    juce::AudioBuffer<float> dry (2, kBlockSize);
    juce::AudioBuffer<float> wet (2, kBlockSize);
    juce::AudioBuffer<float> lead (2, kBlockSize);

    voxchord::MidiVoiceState::NoteSnapshot notes {};
    notes.fill (-1);
    notes[0] = midiNote; // one held voice on the target note

    double phase = 0.0;
    const double phaseInc = 2.0 * juce::MathConstants<double>::pi
                          * static_cast<double> (kInputFrequencyHz) / kSampleRate;

    const int totalBlocks = static_cast<int> (kSettleSeconds * kSampleRate / kBlockSize);

    for (int block = 0; block < totalBlocks; ++block)
    {
        for (int n = 0; n < kBlockSize; ++n)
        {
            const auto s = kInputAmplitude * static_cast<float> (std::sin (phase));
            dry.setSample (0, n, s);
            dry.setSample (1, n, s);
            phase += phaseInc;
            if (phase > 2.0 * juce::MathConstants<double>::pi)
                phase -= 2.0 * juce::MathConstants<double>::pi;
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
    }

    const auto state = engine.getPitchState();
    const auto targetHz = midiNoteToHz (midiNote);
    const auto noteName = juce::MidiMessage::getMidiNoteName (midiNote, true, true, 3);

    std::printf ("%-4s (MIDI %3d, target %7.2f Hz)  detIn=%7.2f Hz  ratio %6.3f -> %6.3f  grain=%4d smp  Per/Win=%5.2f  clamp#=%u  wetHz=%7.2f (%+.0f c)\n",
                 noteName.toRawUTF8(),
                 midiNote,
                 targetHz,
                 state.correctionInputPitchHz,
                 state.representativePitchRatioRaw,
                 state.representativePitchRatioClamped,
                 state.representativeGrainWindowSamples,
                 state.outputPeriodToWindowRatio,
                 static_cast<unsigned> (state.ratioClampHitCount),
                 state.wetZeroCrossingHz,
                 state.wetZeroCrossingCentsDeviation);
}
} // namespace

int main()
{
    std::printf ("VoxChord D1 offline diagnostics - input sine %.1f Hz @ %.0f Hz SR, block %d, settle %.1fs\n",
                 kInputFrequencyHz, kSampleRate, kBlockSize, kSettleSeconds);
    std::printf ("Detection floor (SimplePitchDetector::minFrequencyHz) = 80 Hz; shifter ratio clamp = 0.25..8.0\n\n");

    // Descending targets around the 440 Hz input, per experiment #2 (A5..A1)
    // plus the octave above, to show where the shifter/ratio starts to break.
    for (int note : { 81, 69, 57, 45, 33 }) // A5, A4, A3, A2, A1
        runForNote (note);

    std::printf ("\nNotes:\n");
    std::printf ("  detIn   = correctionInputPitchHz (detected input pitch feeding the ratio)\n");
    std::printf ("  ratio   = representative voice input->target pitch ratio, raw -> clamped (clamp 0.25..8.0)\n");
    std::printf ("  Per/Win = (SR/targetHz)/grainWindowSamples; approaching/exceeding 1.0 implicates the shifter\n");
    std::printf ("  wetHz   = zero-crossing estimate of the representative wet voice (accurate for sine input)\n");
    return 0;
}
