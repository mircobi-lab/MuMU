#include "PluginEditor.h"

namespace ui = mbstudio::ui;
namespace C = mbstudio::ui::colours;
using namespace mumu;

namespace
{
constexpr int rowH = 88, chipH = 20, margin = 14, sectionGap = 22, ctlGap = 4;
constexpr int rowY[3] = { 124, 336, 548 };
constexpr int seqY = 772, seqH = 128;
constexpr int keyboardY = 930;

int findIndex (const std::vector<ParamDef>& defs, const juce::String& key)
{
    for (size_t i = 0; i < defs.size(); ++i) if (key == defs[i].key) return (int) i;
    jassertfalse;
    return 0;
}
}

// ================================================================================================ pannello
MuMUPanel::MuMUPanel (MuMUProcessor& p)
    : proc (p), keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard),
      midiButtons (p.midiMap), midiOverlay (p.midiMap), seqStrip (p)
{
    // riga 1: sorgenti e filtri
    addSection ("MOD OSC", true, { { "MOWAVE", "MOTRACK", "MOPITCH", "MORATE" }, { "MOAPITCH", "MOACUT", "MOAWAVE", "MOAAMP" } });
    addSection ("OSC 1", true, { { "O1OCT" }, { "O1WAVE" } });
    addSection ("OSC 2", true, { { "O2OCT", "O2SEMI", "O2FINE" }, { "O2WAVE", "SYNC", "FM" } });
    addSection ("MIXER", true, { { "MIX1", "MIX2", "MIXMO", "MIXRING" }, { "MIXNOISE", "DRIVE", "OFFSET" } });
    addSection ("FILTERS", true, { { "F1MODE", "F1CUT", "F1RES", "F2CUT", "F2RES", "KEYTRK" }, { "FROUTE", "FLINK", "FSPACE", "FENVAMT", "FVELO" } });
    // riga 2: inviluppi, LFO, voce
    addSection ("FILTER ENVELOPE", true, { { "FA", "FD", "FS", "FR" }, { "FCURVE", "FLOOP" } });
    addSection ("AMP ENVELOPE", true, { { "AA", "AD", "AS", "AR" }, { "ACURVE", "AVELO", "ALOOP" } });
    addSection ("LFO 1", true, { { "L1WAVE", "L1RATE" }, { "L1SYNC", "L1KEY", "L1UNI" } });
    addSection ("LFO 2", true, { { "L2WAVE", "L2RATE" }, { "L2SYNC", "L2KEY", "L2UNI" } });
    addSection ("PITCH LFO", true, { { "PLRATE", "PLAMT" }, { "PLWHEEL" } });
    addSection ("VOICE", true, { { "VMODE", "UNIVOX", "UNIDET", "OCT" }, { "MTRIG", "GLIDE", "GLMODE", "BEND" } });
    // riga 3: arpeggiatore, delay, uscita (la matrice è disegnata a parte)
    addSection ("ARPEGGIATOR", false, { { "ARPON", "ARPMODE", "ARPOCT" }, { "ARPRATE", "ARPGATE", "ARPLATCH", "BPM" } });
    addSection ("DIFFUSION DELAY", false, { { "DLON", "DLTIME", "DLSYNC", "DLFB" }, { "DLMIX", "DLDIFF", "DLCHAR", "DLTONE", "DLPING" } });
    addSection ("OUTPUT", true, { { "LEVEL", "PAN", "SPREAD" }, { "G:MACRO", "G:TUNE", "G:MASTER", "G:LOWCUT" } });

    for (int s = 0; s < 8; ++s)
    {
        const juce::String n (s + 1);
        matrix[s][0] = makeCtl ("M" + n + "SRC", true, "MOD " + n);
        matrix[s][1] = makeCtl ("M" + n + "DST", true, "MOD " + n);
        matrix[s][2] = makeCtl ("M" + n + "AMT", true, "MOD " + n);
        matrix[s][2]->small = true;
    }
    // sequencer: prima la striscia, poi i comandi-parametro sopra di lei
    addAndMakeVisible (seqStrip);
    seqStrip.onTouch = [this] (const juce::String& w, const juce::String& v) { touched = w; touchedValue = v; repaint (displayArea); };
    {
        const char* keys[6] = { "SEQON", "SEQREC", "SEQSLOT", "SEQRATE", "SEQKEY", "SEQSWING" };
        for (int i = 0; i < 6; ++i) seqCtl[i] = makeCtl (keys[i], false, "SEQUENCER");
        seqCtl[0]->button.setTooltip ("Avvia e ferma il sequencer. In una DAW segue il tempo e la posizione del progetto");
        seqCtl[1]->button.setTooltip ("Registra: in moto sostituisce il passo acceso (tieni premuto per legare), da fermo scrive passo passo");
        seqCtl[4]->combo.setTooltip ("PLAY: la tastiera suona sopra la sequenza. TRANSPOSE: il tasto suonato traspone la sequenza rispetto a C3");
    }
    keyMode = makeCtl ("KEYMODE", false, "PATCH");
    splitPoint = makeCtl ("SPLIT", false, "PATCH");

    for (int t = 0; t < 2; ++t)
    {
        timbreBtn[t].setButtonText (t == 0 ? "A" : "B");
        timbreBtn[t].getProperties().set ("led", true);
        timbreBtn[t].getProperties().set ("ledColour", t == 0 ? "ff3d8bff" : "ff3ee05c");
        timbreBtn[t].onClick = [this, t] { selTimbre = t; reattach(); repaint(); };
        addAndMakeVisible (timbreBtn[t]);
    }
    prevBtn.onClick = [this] { const int n = proc.getNumPrograms(); proc.setCurrentProgram ((proc.getCurrentProgram() + n - 1) % n); };
    nextBtn.onClick = [this] { const int n = proc.getNumPrograms(); proc.setCurrentProgram ((proc.getCurrentProgram() + 1) % n); };
    addAndMakeVisible (prevBtn);
    addAndMakeVisible (nextBtn);
    panicBtn.setTooltip ("MIDI PANIC: spegne subito tutte le note, l'arpeggiatore, il sustain e la coda del delay");
    panicBtn.getProperties().set ("led", true);
    panicBtn.getProperties().set ("ledColour", "ffff3b30");
    panicBtn.onClick = [this]
    {
        proc.requestPanic();
        proc.keyboardState.allNotesOff (0);
        panicFlash = 15;
        touched = "PANIC"; touchedValue = "TUTTE LE NOTE SPENTE";
    };
    addAndMakeVisible (panicBtn);
    programBox.onChange = [this]
    {
        const int id = programBox.getSelectedId();
        if (id > 0 && id - 1 != proc.getCurrentProgram()) proc.setCurrentProgram (id - 1);
    };
    addAndMakeVisible (programBox);
    addAndMakeVisible (meter);

    keyboard.setColour (juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour (0xffdcdcd8));
    keyboard.setColour (juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour (0xff0d0d0e));
    keyboard.setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colour (0xff3a3c40));
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, C::ledBlue.withAlpha (.55f));
    keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, C::ledBlue.withAlpha (.18f));
    keyboard.setColour (juce::MidiKeyboardComponent::shadowColourId, juce::Colours::black.withAlpha (.5f));
    keyboard.setColour (juce::MidiKeyboardComponent::upDownButtonBackgroundColourId, C::key);
    keyboard.setColour (juce::MidiKeyboardComponent::upDownButtonArrowColourId, C::print);
    keyboard.setAvailableRange (24, 108);
    keyboard.setLowestVisibleKey (24);
    keyboard.setWantsKeyboardFocus (false);
    addAndMakeVisible (keyboard);
    addAndMakeVisible (midiButtons);
    addAndMakeVisible (midiOverlay);     // per ultimo: sopra a tutto

    juce::Component::SafePointer<MuMUPanel> safeThis (this);
    proc.onProgramChanged = [safeThis] { juce::MessageManager::callAsync ([safeThis] { if (safeThis != nullptr) safeThis->refreshPrograms(); }); };
    refreshPrograms();
    reattach();
    setSize (baseW, baseH);
    startTimerHz (30);
}

MuMUPanel::~MuMUPanel()
{
    stopTimer();
    proc.onProgramChanged = nullptr;
    for (auto& c : ctls) { c->sa.reset(); c->ca.reset(); c->ba.reset(); }
}

MuMUPanel::Ctl* MuMUPanel::makeCtl (const juce::String& keyIn, bool timbreSection, const juce::String& section)
{
    auto c = std::make_unique<Ctl>();
    juce::String key = keyIn;
    c->timbre = timbreSection;
    if (key.startsWith ("G:")) { key = key.substring (2); c->timbre = false; }
    const auto& defs = c->timbre ? timbreDefs() : globalDefs();
    c->index = findIndex (defs, key);
    c->def = &defs[(size_t) c->index];
    c->section = section;
    c->label = c->def->label;
    auto* cp = c.get();
    auto show = [this, cp] (float v)
    {
        touched = (cp->timbre ? juce::String ("TIMBRE ") + (selTimbre == 0 ? "A  " : "B  ") : juce::String()) + cp->section + "  " + cp->label;
        touchedValue = formatValue (*cp->def, v);
        repaint (displayArea);
    };
    switch (c->def->type)
    {
        case PType::C:
            c->kind = Kind::combo;
            c->width = 80;
            c->combo.addItemList (c->def->choiceList(), 1);
            c->combo.setTooltip (c->def->name);
            c->combo.onChange = [cp, show] { show ((float) (cp->combo.getSelectedId() - 1)); };
            break;
        case PType::T:
            c->kind = Kind::toggle;
            c->width = 62;
            c->button.setButtonText (c->label);
            c->button.setClickingTogglesState (true);
            c->button.setTooltip (c->def->name);
            c->button.onClick = [cp, show] { show (cp->button.getToggleState() ? 1.f : 0.f); };
            break;
        case PType::R:
        case PType::I:
            c->kind = Kind::knob;
            c->width = 60;
            c->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            c->slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
            c->slider.setColour (juce::Slider::trackColourId, C::ledBlue);
            c->slider.setMouseDragSensitivity (200);
            c->slider.setDoubleClickReturnValue (true, c->def->def);
            c->slider.setTooltip (c->def->name);
            if (c->def->bipolar()) c->slider.getProperties().set ("bipolar", true);
            c->slider.onValueChange = [cp, show] { show ((float) cp->slider.getValue()); };
            break;
    }
    addAndMakeVisible (c->comp());
    ctls.push_back (std::move (c));
    return cp;
}

void MuMUPanel::addSection (const juce::String& title, bool timbre, std::initializer_list<std::initializer_list<const char*>> rows)
{
    Section s { title, timbre, {}, {} };
    for (auto& r : rows)
    {
        std::vector<Ctl*> row;
        for (auto* k : r) row.push_back (makeCtl (k, timbre, title));
        s.rows.push_back (std::move (row));
    }
    sections.push_back (std::move (s));
}

void MuMUPanel::reattach()
{
    const juce::String tag = selTimbre == 0 ? "ff3d8bff" : "ff3ee05c";
    for (auto& c : ctls)
    {
        c->sa.reset(); c->ca.reset(); c->ba.reset();
        const auto id = paramID (c->timbre ? selTimbre : -1, c->index);
        mbstudio::midi::tag (c->kind == Kind::knob ? (juce::Component&) c->slider
                             : c->kind == Kind::combo ? (juce::Component&) c->combo : (juce::Component&) c->button, id);
        switch (c->kind)
        {
            case Kind::knob:
                c->slider.setColour (juce::Slider::trackColourId, c->timbre ? juce::Colour::fromString (tag) : C::ledAmber);
                c->sa = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.state, id, c->slider);
                break;
            case Kind::combo:
                c->ca = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.state, id, c->combo);
                break;
            case Kind::toggle:
                c->button.getProperties().set ("ledColour", c->timbre ? tag : "ffffb020");
                c->ba = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.state, id, c->button);
                c->button.repaint();
                break;
        }
    }
    if (seqCtl[1] != nullptr) seqCtl[1]->button.getProperties().set ("ledColour", "ffff3b30");
    if (seqCtl[0] != nullptr) seqCtl[0]->button.getProperties().set ("ledColour", "ff3ee05c");
    for (int t = 0; t < 2; ++t) timbreBtn[t].setToggleState (selTimbre == t, juce::dontSendNotification);
    touched.clear();
    repaint();
}

void MuMUPanel::refreshPrograms()
{
    programBox.clear (juce::dontSendNotification);
    programBox.addSectionHeading ("FACTORY MBSTUDIO91");
    for (int i = 0; i < proc.getNumPrograms(); ++i) programBox.addItem (proc.getProgramName (i), i + 1);
    programBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    touched.clear();
    repaint();
}

void MuMUPanel::timerCallback()
{
    meter.setLevel (std::pow (proc.synth.getOutputLevel(), .5f));
    if (panicFlash > 0) --panicFlash;
    panicBtn.setToggleState (panicFlash > 0 && (panicFlash / 3) % 2 == 0, juce::dontSendNotification);
    repaint (displayArea);
}

// ------------------------------------------------------------------------------------------------ layout
void MuMUPanel::layoutSection (Section& s, int x, int y, int w)
{
    s.area = { x, y, w, chipH + rowH * (int) s.rows.size() };
    for (size_t r = 0; r < s.rows.size(); ++r)
    {
        auto& row = s.rows[r];
        int natural = 0;
        for (auto* c : row) natural += c->width;
        const int gap = row.size() > 1 ? juce::jlimit (ctlGap, 30, (w - natural) / (int) (row.size() - 1)) : 0;
        int cx = x;
        const int cy = y + chipH + rowH * (int) r;
        for (auto* c : row)
        {
            c->cell = { cx, cy, c->width, rowH };
            switch (c->kind)
            {
                case Kind::knob:   c->slider.setBounds (cx + (c->width - 54) / 2, cy + 22, 54, 54); break;
                case Kind::combo:  c->combo.setBounds (cx, cy + 34, c->width, 26); break;
                case Kind::toggle: c->button.setBounds (cx, cy + 33, c->width, 28); break;
            }
            cx += c->width + gap;
        }
    }
}

void MuMUPanel::resized()
{
    // intestazione
    displayArea = { 360, 14, 470, 86 };
    timbreBtn[0].setBounds (850, 38, 62, 30);
    timbreBtn[1].setBounds (918, 38, 62, 30);
    keyMode->combo.setBounds (996, 38, 120, 28);
    splitPoint->slider.setBounds (1128, 28, 50, 50);
    programBox.setBounds (1196, 38, 254, 28);
    prevBtn.setBounds (1196, 74, 46, 26);
    nextBtn.setBounds (1248, 74, 46, 26);
    panicBtn.setBounds (1370, 74, 80, 26);
    midiButtons.setBounds (850, 74, 250, 26);
    midiOverlay.setBounds (getLocalBounds());
    meter.setBounds (1468, 18, 14, 82);

    // righe di sezioni: larghezza naturale + spazio residuo distribuito
    auto naturalWidth = [] (const Section& s)
    {
        int w = 0;
        for (auto& row : s.rows) { int rw = 0; for (auto* c : row) rw += c->width + ctlGap; w = std::max (w, rw - ctlGap); }
        return w;
    };
    auto layoutRow = [&] (int first, int count, int y, int reserved)
    {
        int total = 0;
        for (int i = 0; i < count; ++i) total += naturalWidth (sections[(size_t) (first + i)]);
        const int avail = baseW - 2 * margin - reserved - sectionGap * (count - 1 + (reserved > 0 ? 1 : 0));
        const int extra = std::max (0, (avail - total) / count);
        int x = margin + (reserved > 0 ? reserved + sectionGap : 0);
        for (int i = 0; i < count; ++i)
        {
            auto& s = sections[(size_t) (first + i)];
            const int w = naturalWidth (s) + extra;
            layoutSection (s, x, y, w);
            x += w + sectionGap;
        }
    };
    layoutRow (0, 5, rowY[0], 0);
    layoutRow (5, 6, rowY[1], 0);

    // riga 3: matrice di modulazione a sinistra, poi arpeggiatore, delay, uscita
    matrixArea = { margin, rowY[2], 560, chipH + 2 * rowH };
    for (int s = 0; s < 8; ++s)
    {
        const int col = s / 4, r = s % 4;
        const int x = matrixArea.getX() + col * 286 + 22, y = matrixArea.getY() + chipH + 20 + r * 39;
        matrix[s][0]->combo.setBounds (x, y, 96, 28);
        matrix[s][1]->combo.setBounds (x + 101, y, 114, 28);
        matrix[s][2]->slider.setBounds (x + 216, y - 4, 36, 36);
        for (int k = 0; k < 3; ++k) matrix[s][k]->cell = {};
    }
    layoutRow (11, 3, rowY[2], matrixArea.getWidth());

    // sequencer
    seqStrip.setBounds (margin, seqY, baseW - 2 * margin, seqH);
    {
        const int x0 = margin, y0 = seqY;
        seqCtl[0]->button.setBounds (x0, y0 + 34, 62, 28);
        seqCtl[1]->button.setBounds (x0 + 66, y0 + 34, 62, 28);
        seqCtl[2]->combo.setBounds (x0 + 132, y0 + 35, 88, 26);
        seqCtl[3]->combo.setBounds (x0 + 224, y0 + 35, 70, 26);
        seqCtl[4]->combo.setBounds (x0 + 298, y0 + 35, 100, 26);
        seqCtl[5]->slider.setBounds (x0 + 402, y0 + 30, 40, 40);
        for (auto* c : seqCtl) c->cell = {};
    }

    keyboard.setBounds (margin, keyboardY, baseW - 2 * margin, baseH - keyboardY - 12);
    keyboard.setKeyWidth ((float) (baseW - 2 * margin) / 50.f);
}

// ------------------------------------------------------------------------------------------------ disegno
void MuMUPanel::paint (juce::Graphics& g)
{
    ui::paintChassis (g, getLocalBounds());
    const auto timbreLed = selTimbre == 0 ? C::ledBlue : C::ledGreen;

    // marchio e sottotitolo
    ui::paintBrand (g, { 18.f, 16.f, 320.f, 34.f }, "MUMU");
    ui::paintPrinted (g, { 18.f, 56.f, 330.f, 16.f }, "POLYPHONIC ANALOG MODEL  -  8 VOICES  -  BITIMBRAL", 14.f, juce::Justification::centredLeft, C::printDim);
    ui::paintPrinted (g, { 18.f, 76.f, 330.f, 14.f }, "LADDER FILTERS  -  MOD MATRIX  -  DIFFUSION DELAY", 11.f, juce::Justification::centredLeft, C::printDim.withAlpha (.7f));

    // display
    const auto d = displayArea.toFloat();
    ui::paintRecess (g, d);
    {
        const auto in = d.reduced (12.f, 8.f);
        const int cur = proc.getCurrentProgram();
        g.setColour (C::print);
        g.setFont (ui::Fonts::display (28.f));
        g.drawText (proc.programLabel (cur) + "  " + juce::String (factoryPresets()[(size_t) cur].name), in.withHeight (30.f), juce::Justification::centredLeft, true);
        g.setFont (ui::Fonts::label (14.f));
        g.setColour (C::printDim);
        const auto km = formatValue (globalDefs()[(size_t) KEYMODE], *proc.state.getRawParameterValue (paramID (-1, KEYMODE)));
        g.drawText ("EDIT TIMBRE " + juce::String (selTimbre == 0 ? "A" : "B") + "     KEY MODE " + km, in.withY (in.getY() + 32.f).withHeight (18.f),
                    juce::Justification::centredLeft, true);
        const juce::String line3 = touched.isNotEmpty() ? touched + "  =  " + touchedValue
                                                        : "VOICES " + juce::String (proc.synth.activeVoices()) + " / 8";
        g.setColour (C::print);
        g.drawText (line3.toUpperCase(), in.withY (in.getY() + 52.f).withHeight (18.f), juce::Justification::centredLeft, true);
        ui::paintLed (g, { d.getRight() - 14.f, d.getY() + 12.f }, 3.5f, C::ledGreen, juce::jmin (1.f, proc.synth.getOutputLevel() * 3.f));
    }
    auto chipAt = [&] (float x, float y, const juce::String& t) { ui::paintChip (g, { x, y, ui::chipWidth (t, 16.f), 16.f }, t, 14.f); };
    chipAt (850.f, 16.f, "TIMBRE SELECT");
    ui::paintPrinted (g, { 996.f, 16.f, 120.f, 16.f }, "KEY MODE", 12.f, juce::Justification::centredLeft, C::print);
    ui::paintPrinted (g, { 1124.f, 14.f, 58.f, 14.f }, "SPLIT", 12.f, juce::Justification::centred, C::print);
    {
        const auto sp = formatValue (globalDefs()[(size_t) SPLIT], *proc.state.getRawParameterValue (paramID (-1, SPLIT)));
        ui::paintPrinted (g, { 1124.f, 80.f, 58.f, 14.f }, sp, 11.f, juce::Justification::centred, C::printDim);
    }
    chipAt (1196.f, 16.f, "PROGRAM");
    ui::paintPrinted (g, { 1302.f, 76.f, 64.f, 22.f }, "FACTORY", 11.f, juce::Justification::centredLeft, C::printDim);
    ui::paintEngravedLine (g, { (float) margin, 112.f }, { (float) baseW - margin, 112.f });

    // sezioni
    auto paintHeader = [&] (juce::Rectangle<float> a, const juce::String& title, bool timbre, bool mixed)
    {
        const float cw = ui::chipWidth (title, 16.f);
        ui::paintChip (g, { a.getX(), a.getY(), cw, 16.f }, title, 14.f);
        const juce::String tag = timbre ? juce::String ("TIMBRE ") + (selTimbre == 0 ? "A" : "B") + (mixed ? " + GLOBAL" : "") : "GLOBAL";
        g.setFont (ui::Fonts::label (11.f));
        const float tw = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), tag) + 6.f;
        const float lineEnd = a.getRight() - tw - 14.f;
        if (lineEnd > a.getX() + cw + 6.f) ui::paintEngravedLine (g, { a.getX() + cw + 4.f, a.getY() + 8.f }, { lineEnd, a.getY() + 8.f });
        g.setColour (C::printDim);
        g.drawText (tag, juce::Rectangle<float> (a.getRight() - tw, a.getY() + 1.f, tw, 14.f), juce::Justification::centredRight, false);
        ui::paintLed (g, { a.getRight() - tw - 6.f, a.getY() + 8.f }, 2.5f, timbre ? timbreLed : C::ledAmber, 1.f);
    };
    for (auto& s : sections)
    {
        paintHeader (s.area.toFloat(), s.title, s.timbre, s.title == "OUTPUT");
        for (auto& row : s.rows)
            for (auto* c : row)
            {
                if (c->kind == Kind::toggle) continue;
                const auto cell = c->cell.toFloat();
                g.setColour (C::print);
                g.setFont (ui::Fonts::label (12.f));
                g.drawText (c->label, cell.withHeight (16.f).expanded (6.f, 0.f), juce::Justification::centred, false);
                if (! c->timbre && s.timbre)
                    ui::paintLed (g, { cell.getCentreX(), cell.getBottom() - 6.f }, 2.f, C::ledAmber, 1.f);
            }
    }
    // matrice
    {
        const auto a = matrixArea.toFloat();
        paintHeader (a, "MOD MATRIX", true, false);
        for (int col = 0; col < 2; ++col)
        {
            const float x = a.getX() + (float) col * 286.f + 22.f, y = a.getY() + (float) chipH;
            g.setColour (C::printDim);
            g.setFont (ui::Fonts::label (11.f));
            g.drawText ("SOURCE", juce::Rectangle<float> (x, y, 96.f, 16.f), juce::Justification::centredLeft, false);
            g.drawText ("DESTINATION", juce::Rectangle<float> (x + 101.f, y, 114.f, 16.f), juce::Justification::centredLeft, false);
            g.drawText ("AMT", juce::Rectangle<float> (x + 210.f, y, 48.f, 16.f), juce::Justification::centred, false);
        }
        for (int s = 0; s < 8; ++s)
        {
            const auto b = matrix[s][0]->combo.getBounds().toFloat();
            const bool live = matrix[s][0]->combo.getSelectedId() > 1 && matrix[s][1]->combo.getSelectedId() > 1 && std::abs (matrix[s][2]->slider.getValue()) > .001;
            g.setColour (C::print);
            g.setFont (ui::Fonts::display (17.f));
            g.drawText (juce::String (s + 1), juce::Rectangle<float> (b.getX() - 22.f, b.getY(), 12.f, b.getHeight()), juce::Justification::centred, false);
            ui::paintLed (g, { b.getX() - 6.f, b.getCentreY() }, 2.5f, timbreLed, live ? 1.f : 0.f);
        }
        ui::paintEngravedLine (g, { a.getX() + 280.f, a.getY() + 26.f }, { a.getX() + 280.f, a.getBottom() - 10.f });
    }

    // logo impresso in basso a destra (sopra la tastiera)
    ui::paintLogo (g, { (float) baseW - 260.f, (float) keyboardY - 22.f, 246.f, 16.f }, false, ui::Imprint::debossed, juce::Justification::centredRight);
    ui::paintPrinted (g, { (float) margin, (float) keyboardY - 22.f, 1180.f, 16.f },
                      "MOD WHEEL = VIBRATO DEPTH   -   DOUBLE CLICK A KNOB TO RESET   -   AMBER = GLOBAL   -   SEQUENCER: DOUBLE CLICK = NOTE ON/OFF, DRAG = VELOCITY, WHEEL = TRANSPOSE, RIGHT CLICK = MENU", 11.f, juce::Justification::centredLeft, C::printDim);
}

// ================================================================================================ editor
MuMUEditor::MuMUEditor (MuMUProcessor& p) : AudioProcessorEditor (p), proc (p), panel (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (panel);
    setResizable (true, true);
    getConstrainer()->setFixedAspectRatio ((double) MuMUPanel::baseW / MuMUPanel::baseH);
    setResizeLimits (MuMUPanel::baseW / 2, MuMUPanel::baseH / 2, MuMUPanel::baseW * 2, MuMUPanel::baseH * 2);
    setSize ((int) (MuMUPanel::baseW * .9), (int) (MuMUPanel::baseH * .9));
}

MuMUEditor::~MuMUEditor() { setLookAndFeel (nullptr); }

void MuMUEditor::paint (juce::Graphics& g) { ui::paintChassis (g, getLocalBounds()); }

void MuMUEditor::resized()
{
    const float s = (float) getWidth() / (float) MuMUPanel::baseW;
    panel.setBounds (0, 0, MuMUPanel::baseW, MuMUPanel::baseH);
    panel.setTransform (juce::AffineTransform::scale (s));
}
