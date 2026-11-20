#pragma once
#include "PluginProcessor.h"
#include "MBStudioLookAndFeel.h"
#include <juce_audio_utils/juce_audio_utils.h>

// Pannello frontale di MuMU: una manopola per funzione, sezioni incise con titolo-chip, grafica MBStudio91.
class MuMUPanel : public juce::Component, private juce::Timer
{
public:
    explicit MuMUPanel (MuMUProcessor&);
    ~MuMUPanel() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void refreshPrograms();

    static constexpr int baseW = 1500, baseH = 890;

private:
    enum class Kind { knob, combo, toggle };
    struct Ctl
    {
        Kind kind = Kind::knob;
        bool timbre = true;
        int index = 0;
        const mumu::ParamDef* def = nullptr;
        juce::String section, label;
        int width = 60;
        bool small = false;          // manopola piccola della matrice
        juce::Slider slider;
        juce::ComboBox combo;
        juce::TextButton button;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sa;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> ca;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> ba;
        juce::Rectangle<int> cell;
        juce::Component& comp() { return kind == Kind::knob ? (juce::Component&) slider : kind == Kind::combo ? (juce::Component&) combo : (juce::Component&) button; }
    };
    struct Section
    {
        juce::String title;
        bool timbre;
        juce::Rectangle<int> area;
        std::vector<std::vector<Ctl*>> rows;
    };

    MuMUProcessor& proc;
    std::vector<std::unique_ptr<Ctl>> ctls;
    std::vector<Section> sections;
    Ctl* matrix[8][3] {};
    juce::Rectangle<int> matrixArea;
    int selTimbre = 0;
    juce::String touched, touchedValue;

    juce::TextButton timbreBtn[2], prevBtn { "<" }, nextBtn { ">" }, panicBtn { "PANIC" };
    int panicFlash = 0;
    juce::ComboBox programBox;
    mbstudio::ui::LedMeter meter { 12 };
    juce::MidiKeyboardComponent keyboard;
    mbstudio::midi::MidiControls midiButtons;
    mbstudio::midi::MidiLearnOverlay midiOverlay;
    juce::Rectangle<int> displayArea;
    Ctl* keyMode = nullptr;
    Ctl* splitPoint = nullptr;

    Ctl* makeCtl (const juce::String& key, bool timbre, const juce::String& section);
    void addSection (const juce::String& title, bool timbre, std::initializer_list<std::initializer_list<const char*>> rows);
    void reattach();
    void layoutSection (Section&, int x, int y, int w);
    void timerCallback() override;
};

class MuMUEditor : public juce::AudioProcessorEditor
{
public:
    explicit MuMUEditor (MuMUProcessor&);
    ~MuMUEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    mbstudio::ui::HardwareLookAndFeel lnf;
    MuMUProcessor& proc;
    MuMUPanel panel;
};
