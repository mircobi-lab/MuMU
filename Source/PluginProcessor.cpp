#include "PluginProcessor.h"
#include "PluginEditor.h"

MuMUProcessor::MuMUProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, "MuMU", mumu::createLayout())
{
    reader.attach (state);
    setCurrentProgram (1);
}

bool MuMUProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void MuMUProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    synth.prepare (sampleRate, juce::jmax (samplesPerBlock, 32));
    keyboardState.reset();
}

void MuMUProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    keyboardState.processNextMidiBuffer (midi, 0, buffer.getNumSamples(), true);
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isProgramChange()) { pendingProgram.store (m.getProgramChangeNumber()); triggerAsyncUpdate(); }
    }
    double bpm = 0, ppq = 0;
    bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            if (auto p = pos->getPpqPosition()) ppq = *p;
            playing = pos->getIsPlaying() && pos->getPpqPosition().hasValue() && bpm > 0;
        }
    reader.read (tv, gv);
    buffer.clear();
    synth.process (buffer, midi, tv, gv, bpm, ppq, playing);
}

void MuMUProcessor::handleAsyncUpdate()
{
    const int p = pendingProgram.exchange (-1);
    if (p >= 0 && p < getNumPrograms()) setCurrentProgram (p);
}

const juce::String MuMUProcessor::getProgramName (int index)
{
    const auto& ps = mumu::factoryPresets();
    if (index < 0 || index >= (int) ps.size()) return {};
    return programLabel (index) + " " + ps[(size_t) index].name;
}

void MuMUProcessor::setCurrentProgram (int index)
{
    const auto& ps = mumu::factoryPresets();
    if (index < 0 || index >= (int) ps.size()) return;
    currentProgram = index;
    mumu::applyPreset (state, ps[(size_t) index]);
    if (onProgramChanged) onProgramChanged();
    updateHostDisplay (ChangeDetails().withProgramChanged (true));
}

void MuMUProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto st = state.copyState();
    st.setProperty ("program", currentProgram, nullptr);
    if (auto xml = st.createXml()) copyXmlToBinary (*xml, dest);
}

void MuMUProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (! xml || ! xml->hasTagName (state.state.getType())) return;
    auto st = juce::ValueTree::fromXml (*xml);
    currentProgram = juce::jlimit (0, getNumPrograms() - 1, (int) st.getProperty ("program", 0));
    state.replaceState (st);
    if (onProgramChanged) onProgramChanged();
}

juce::AudioProcessorEditor* MuMUProcessor::createEditor() { return new MuMUEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MuMUProcessor(); }
