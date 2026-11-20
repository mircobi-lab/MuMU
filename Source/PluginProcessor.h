#pragma once
#include "Engine.h"
#include "Presets.h"

class MuMUProcessor : public juce::AudioProcessor, private juce::AsyncUpdater
{
public:
    MuMUProcessor();
    ~MuMUProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 6.0; }

    int getNumPrograms() override { return (int) mumu::factoryPresets().size(); }
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::String programLabel (int index) const { return juce::String (index + 1).paddedLeft ('0', 2); }

    juce::AudioProcessorValueTreeState state;
    juce::MidiKeyboardState keyboardState;
    mumu::Synth synth;
    std::function<void()> onProgramChanged;           // per l'editor (thread dei messaggi)

private:
    mumu::ParamReader reader;
    mumu::TimbreValues tv[2];
    mumu::GlobalValues gv;
    int currentProgram = 0;
    std::atomic<int> pendingProgram { -1 };
    void handleAsyncUpdate() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MuMUProcessor)
};
