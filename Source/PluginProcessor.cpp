#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    bool isMonoOrStereo (const juce::AudioChannelSet& set)
    {
        return set == juce::AudioChannelSet::mono()
            || set == juce::AudioChannelSet::stereo();
    }
}

VoxChordAudioProcessor::VoxChordAudioProcessor()
    : AudioProcessor (BusesProperties()
        #if ! JucePlugin_IsMidiEffect
         #if ! JucePlugin_IsSynth
          .withInput  ("Input",  juce::AudioChannelSet::mono(), true)
         #endif
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
        #endif
      ),
      apvts (*this, nullptr, "VoxChordState", voxchord::createParameterLayout())
{
    dryWetParameter = apvts.getRawParameterValue (voxchord::ParameterIDs::dryWet);
    voiceCountParameter = apvts.getRawParameterValue (voxchord::ParameterIDs::voiceCount);
    tuneParameter = apvts.getRawParameterValue (voxchord::ParameterIDs::tune);
    glideParameter = apvts.getRawParameterValue (voxchord::ParameterIDs::glide);
    characterParameter = apvts.getRawParameterValue (voxchord::ParameterIDs::character);
    spreadParameter = apvts.getRawParameterValue (voxchord::ParameterIDs::spread);
    outputLevelParameter = apvts.getRawParameterValue (voxchord::ParameterIDs::outputLevel);

    for (auto& note : activeMidiNotes)
        note.store (-1, std::memory_order_relaxed);

    jassert (dryWetParameter != nullptr);
    jassert (voiceCountParameter != nullptr);
    jassert (tuneParameter != nullptr);
    jassert (glideParameter != nullptr);
    jassert (characterParameter != nullptr);
    jassert (spreadParameter != nullptr);
    jassert (outputLevelParameter != nullptr);
}

const juce::String VoxChordAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool VoxChordAudioProcessor::acceptsMidi() const
{
    return true;
}

bool VoxChordAudioProcessor::producesMidi() const
{
    return false;
}

bool VoxChordAudioProcessor::isMidiEffect() const
{
    return false;
}

double VoxChordAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int VoxChordAudioProcessor::getNumPrograms()
{
    return 1;
}

int VoxChordAudioProcessor::getCurrentProgram()
{
    return 0;
}

void VoxChordAudioProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String VoxChordAudioProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void VoxChordAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

void VoxChordAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    midiVoices.reset();
    choirEngine.prepare (sampleRate, samplesPerBlock);
    meters.reset();
    dryBuffer.setSize (2, samplesPerBlock, false, false, true);
    wetBuffer.setSize (2, samplesPerBlock, false, false, true);
    dryWetSmoothed.reset (sampleRate, 0.02);
    dryWetSmoothed.setCurrentAndTargetValue (getDryWet());
    outputGainSmoothed.reset (sampleRate, 0.02);
    outputGainSmoothed.setCurrentAndTargetValue (getOutputGain());
    publishMidiSnapshot();
    setLatencySamples (0);
}

void VoxChordAudioProcessor::releaseResources()
{
    dryBuffer.setSize (0, 0);
    wetBuffer.setSize (0, 0);
    choirEngine.reset();
}

bool VoxChordAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto input = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();

    if (! isMonoOrStereo (input))
        return false;

    if (! isMonoOrStereo (output))
        return false;

    return true;
}

void VoxChordAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                           juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    const auto samples = buffer.getNumSamples();
    const auto inputChannels = getTotalNumInputChannels();
    const auto outputChannels = getTotalNumOutputChannels();
    const auto inputPeak = calculatePeak (buffer, inputChannels, samples);

    if (panicRequested.exchange (false, std::memory_order_acq_rel))
    {
        midiVoices.reset();
        publishMidiActivity (MidiActivity::panic);
        publishMidiSnapshot();
    }

    handleMidi (midiMessages);
    copyInputToDryBuffer (buffer);
    choirEngine.render (dryBuffer,
                         wetBuffer,
                         getActiveMidiNotes(),
                         getVoiceLimit(),
                         getSpread(),
                         getTune(),
                         getGlide(),
                         getCharacter());
    const auto pitchState = choirEngine.getPitchState();
    detectedInputPitchHz.store (pitchState.stablePitchHz, std::memory_order_relaxed);
    inputRmsDb.store (pitchState.inputRmsDb, std::memory_order_relaxed);
    rawPitchHz.store (pitchState.rawPitchHz, std::memory_order_relaxed);
    stablePitchHz.store (pitchState.stablePitchHz, std::memory_order_relaxed);
    pitchConfidence.store (pitchState.confidence, std::memory_order_relaxed);
    pitchVoiced.store (pitchState.voiced, std::memory_order_relaxed);
    mixDryWetToOutput (buffer);

    const auto outputPeak = calculatePeak (buffer, outputChannels, samples);
    meters.publish (inputPeak, outputPeak);
}

bool VoxChordAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* VoxChordAudioProcessor::createEditor()
{
    return new VoxChordAudioProcessorEditor (*this);
}

void VoxChordAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void VoxChordAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
    }
}

void VoxChordAudioProcessor::panic() noexcept
{
    panicRequested.store (true, std::memory_order_release);
    publishMidiActivity (MidiActivity::panic);
}

void VoxChordAudioProcessor::clearClipFlags() noexcept
{
    meters.clearClipFlags();
}

voxchord::MidiVoiceState::NoteSnapshot VoxChordAudioProcessor::getActiveMidiNotes() const noexcept
{
    voxchord::MidiVoiceState::NoteSnapshot notes {};

    for (auto index = 0; index < voxchord::MidiVoiceState::maxVoices; ++index)
        notes[static_cast<size_t> (index)] = activeMidiNotes[static_cast<size_t> (index)].load (std::memory_order_relaxed);

    return notes;
}

VoxChordAudioProcessor::MidiActivitySnapshot VoxChordAudioProcessor::getMidiActivitySnapshot() const noexcept
{
    return {
        static_cast<MidiActivity> (lastMidiActivity.load (std::memory_order_relaxed)),
        midiActivityCounter.load (std::memory_order_relaxed)
    };
}

int VoxChordAudioProcessor::getCurrentVoiceLimit() const noexcept
{
    return getVoiceLimit();
}

float VoxChordAudioProcessor::getDetectedInputPitchHz() const noexcept
{
    return detectedInputPitchHz.load (std::memory_order_relaxed);
}

voxchord::PitchState VoxChordAudioProcessor::getPitchState() const noexcept
{
    return {
        inputRmsDb.load (std::memory_order_relaxed),
        rawPitchHz.load (std::memory_order_relaxed),
        stablePitchHz.load (std::memory_order_relaxed),
        pitchConfidence.load (std::memory_order_relaxed),
        pitchVoiced.load (std::memory_order_relaxed)
    };
}

float VoxChordAudioProcessor::calculatePeak (const juce::AudioBuffer<float>& buffer, int channels, int samples) noexcept
{
    auto peak = 0.0f;
    const auto safeChannels = juce::jlimit (0, buffer.getNumChannels(), channels);

    for (auto channel = 0; channel < safeChannels; ++channel)
        peak = juce::jmax (peak, buffer.getMagnitude (channel, 0, samples));

    return peak;
}

int VoxChordAudioProcessor::getVoiceLimit() const noexcept
{
    if (voiceCountParameter == nullptr)
        return voxchord::MidiVoiceState::maxVoices;

    return juce::jlimit (1,
                         voxchord::MidiVoiceState::maxVoices,
                         juce::roundToInt (voiceCountParameter->load (std::memory_order_relaxed)));
}

float VoxChordAudioProcessor::getDryWet() const noexcept
{
    if (dryWetParameter == nullptr)
        return 0.0f;

    return juce::jlimit (0.0f, 1.0f, dryWetParameter->load (std::memory_order_relaxed));
}

float VoxChordAudioProcessor::getOutputGain() const noexcept
{
    if (outputLevelParameter == nullptr)
        return 1.0f;

    return juce::Decibels::decibelsToGain (outputLevelParameter->load (std::memory_order_relaxed));
}

float VoxChordAudioProcessor::getSpread() const noexcept
{
    if (spreadParameter == nullptr)
        return 0.0f;

    return juce::jlimit (0.0f, 1.0f, spreadParameter->load (std::memory_order_relaxed));
}

float VoxChordAudioProcessor::getTune() const noexcept
{
    if (tuneParameter == nullptr)
        return 1.0f;

    return juce::jlimit (0.0f, 1.0f, tuneParameter->load (std::memory_order_relaxed));
}

float VoxChordAudioProcessor::getGlide() const noexcept
{
    if (glideParameter == nullptr)
        return 0.0f;

    return juce::jlimit (0.0f, 1.0f, glideParameter->load (std::memory_order_relaxed));
}

float VoxChordAudioProcessor::getCharacter() const noexcept
{
    if (characterParameter == nullptr)
        return 0.0f;

    return juce::jlimit (0.0f, 1.0f, characterParameter->load (std::memory_order_relaxed));
}

void VoxChordAudioProcessor::handleMidi (const juce::MidiBuffer& midiMessages) noexcept
{
    const auto voiceLimit = getVoiceLimit();
    midiVoices.enforceVoiceLimit (voiceLimit);

    for (const auto metadata : midiMessages)
    {
        const auto message = metadata.getMessage();

        if (message.isNoteOn())
            publishMidiActivity (MidiActivity::noteOn);
        else if (message.isNoteOff())
            publishMidiActivity (MidiActivity::noteOff);
        else if (message.isAllNotesOff() || message.isAllSoundOff() || message.isResetAllControllers())
            publishMidiActivity (MidiActivity::allNotesOff);

        midiVoices.handleMidiMessage (message, voiceLimit);
    }

    publishMidiSnapshot();
}

void VoxChordAudioProcessor::publishMidiActivity (MidiActivity activity) noexcept
{
    lastMidiActivity.store (static_cast<int> (activity), std::memory_order_relaxed);
    midiActivityCounter.fetch_add (1, std::memory_order_relaxed);
}

void VoxChordAudioProcessor::publishMidiSnapshot() noexcept
{
    const auto notes = midiVoices.getActiveNotes();

    for (auto index = 0; index < voxchord::MidiVoiceState::maxVoices; ++index)
        activeMidiNotes[static_cast<size_t> (index)].store (notes[static_cast<size_t> (index)], std::memory_order_relaxed);
}

void VoxChordAudioProcessor::copyInputToDryBuffer (const juce::AudioBuffer<float>& buffer) noexcept
{
    const auto samples = buffer.getNumSamples();
    const auto inputChannels = getTotalNumInputChannels();

    dryBuffer.clear();

    if (samples > dryBuffer.getNumSamples())
        return;

    if (inputChannels == 0)
        return;

    if (inputChannels == 1)
    {
        dryBuffer.copyFrom (0, 0, buffer, 0, 0, samples);
        dryBuffer.copyFrom (1, 0, buffer, 0, 0, samples);
        return;
    }

    dryBuffer.copyFrom (0, 0, buffer, 0, 0, samples);
    dryBuffer.copyFrom (1, 0, buffer, 1, 0, samples);
}

void VoxChordAudioProcessor::mixDryWetToOutput (juce::AudioBuffer<float>& buffer) noexcept
{
    const auto samples = buffer.getNumSamples();
    const auto outputChannels = getTotalNumOutputChannels();

    buffer.clear();

    if (samples > dryBuffer.getNumSamples() || samples > wetBuffer.getNumSamples())
        return;

    dryWetSmoothed.setTargetValue (getDryWet());

    for (auto sample = 0; sample < samples; ++sample)
    {
        const auto wetAmount = dryWetSmoothed.getNextValue();
        const auto dryAmount = 1.0f - wetAmount;

        if (outputChannels > 0)
        {
            buffer.setSample (0,
                              sample,
                              dryBuffer.getSample (0, sample) * dryAmount
                                  + wetBuffer.getSample (0, sample) * wetAmount);
        }

        if (outputChannels > 1)
        {
            buffer.setSample (1,
                              sample,
                              dryBuffer.getSample (1, sample) * dryAmount
                                  + wetBuffer.getSample (1, sample) * wetAmount);
        }
    }

    outputGainSmoothed.setTargetValue (getOutputGain());
    outputGainSmoothed.applyGain (buffer, samples);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VoxChordAudioProcessor();
}
