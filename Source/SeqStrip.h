#pragma once
#include "PluginProcessor.h"
#include "MBStudioLookAndFeel.h"

// Striscia del sequencer: 16 passi della pagina, pagine, lunghezza, comandi e editor del passo scelto.
// I comandi che sono parametri (PLAY, REC, SEQUENCE, RATE, KEYS, SWING) li mette il pannello.
class SeqStrip : public juce::Component, private juce::Timer
{
public:
    explicit SeqStrip (MuMUProcessor&);
    ~SeqStrip() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    std::function<void (const juce::String&, const juce::String&)> onTouch;   // per il display del pannello

    static constexpr int gridX = 506, cellW = 56, cellGap = 3, gridY = 20, cellH = 58;

private:
    class Grid : public juce::Component
    {
    public:
        explicit Grid (SeqStrip& o) : owner (o) {}
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseDoubleClick (const juce::MouseEvent&) override;
        void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    private:
        SeqStrip& owner;
        int dragStep = -1, dragStartVel = 0;
        float wheelAcc = 0;
        int stepAt (juce::Point<int>) const;
    };

    MuMUProcessor& proc;
    Grid grid { *this };
    int page = 0, selected = 0, lastRevision = -1, lastSel = -1, lastSlot = -1, lastNote = 48;
    bool copyValid = false;
    struct Clip { int notes[mumu::SeqStep::maxNotes]; int vel, gate, prob, timbre, tie; } clip {};

    juce::TextButton pageBtn[4], followBtn { "FOLLOW" }, clearBtn { "CLEAR" }, restBtn { "REST" },
                     copyBtn { "COPY" }, pasteBtn { "PASTE" }, tieBtn { "TIE" },
                     noteDn { "-1" }, noteUp { "+1" }, octDn { "-OCT" }, octUp { "+OCT" },
                     chainAddBtn { "+ SEQ" }, chainDelBtn { "DEL" }, chainClrBtn { "CLR" };
    juce::String chainText() const;
    juce::Slider lengthKnob, velKnob, gateKnob, probKnob;
    juce::ComboBox timbreBox;
    bool updating = false;

    int slot() const;
    int length() const;
    mumu::SeqStep& step (int index) const;
    void select (int index);
    void refreshEditor();
    void touch (const juce::String& what, const juce::String& value) { if (onTouch) onTouch (what, value); }
    void showMenu (int index);
    void copyStep (int index);
    void pasteStep (int index);
    void timerCallback() override;
    static juce::String noteName (int n) { return juce::MidiMessage::getMidiNoteName (n, true, true, 3); }
    static juce::String stepNotes (const mumu::SeqStep&);
};
