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
    voiceCountParameter = apvts.getRawParameterValue (voxchord::ParameterIDs::voiceCount);
    outputLevelParameter = apvts.getRawParameterValue (voxchord::ParameterIDs::outputLevel);

    for (auto& note : activeMidiNotes)
        note.store (-1, std::memory_order_relaxed);

    jassert (voiceCountParameter != nullptr);
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
    juce::ignoreUnused (samplesPerBlock);

    midiVoices.reset();
    meters.reset();
    outputGainSmoothed.reset (sampleRate, 0.02);
    outputGainSmoothed.setCurrentAndTargetValue (getOutputGain());
    publishMidiSnapshot();
    setLatencySamples (0);
}

void VoxChordAudioProcessor::releaseResources()
{
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
    processAudioPassThrough (buffer);

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

float VoxChordAudioProcessor::getOutputGain() const noexcept
{
    if (outputLevelParameter == nullptr)
        return 1.0f;

    return juce::Decibels::decibelsToGain (outputLevelParameter->load (std::memory_order_relaxed));
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

void VoxChordAudioProcessor::processAudioPassThrough (juce::AudioBuffer<float>& buffer) noexcept
{
    const auto samples = buffer.getNumSamples();
    const auto inputChannels = getTotalNumInputChannels();
    const auto outputChannels = getTotalNumOutputChannels();

    if (outputChannels == 0)
        return;

    if (inputChannels == 0)
    {
        for (auto channel = 0; channel < outputChannels; ++channel)
            buffer.clear (channel, 0, samples);

        return;
    }

    if (inputChannels > 1 && outputChannels == 1)
    {
        buffer.applyGain (0, 0, samples, 0.5f);
        buffer.addFrom (0, 0, buffer, 1, 0, samples, 0.5f);
    }
    else if (inputChannels == 1 && outputChannels > 1)
    {
        for (auto channel = 1; channel < outputChannels; ++channel)
            buffer.copyFrom (channel, 0, buffer, 0, 0, samples);
    }

    for (auto channel = outputChannels; channel < buffer.getNumChannels(); ++channel)
        buffer.clear (channel, 0, samples);

    outputGainSmoothed.setTargetValue (getOutputGain());
    outputGainSmoothed.applyGain (buffer, samples);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VoxChordAudioProcessor();
}
