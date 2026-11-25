#include "SeqStrip.h"

namespace ui = mbstudio::ui;
namespace C = mbstudio::ui::colours;
using namespace mumu;

namespace
{
void setupKnob (juce::Slider& s, double lo, double hi, double def, double step)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    s.setRange (lo, hi, step);
    s.setValue (def, juce::dontSendNotification);
    s.setDoubleClickReturnValue (true, def);
    s.setMouseDragSensitivity (160);
    s.setColour (juce::Slider::trackColourId, C::ledAmber);
}
const char* timbreNames[4] = { "KEY MODE", "TIMBRE A", "TIMBRE B", "A + B" };
}

SeqStrip::SeqStrip (MuMUProcessor& p) : proc (p)
{
    addAndMakeVisible (grid);
    const char* pages[4] = { "1-16", "17-32", "33-48", "49-64" };
    for (int i = 0; i < 4; ++i)
    {
        pageBtn[i].setButtonText (pages[i]);
        pageBtn[i].getProperties().set ("led", true);
        pageBtn[i].getProperties().set ("ledColour", "ffffb020");
        pageBtn[i].onClick = [this, i] { page = i; followBtn.setToggleState (false, juce::dontSendNotification); grid.repaint(); };
        addAndMakeVisible (pageBtn[i]);
    }
    followBtn.setClickingTogglesState (true);
    followBtn.setToggleState (true, juce::dontSendNotification);
    followBtn.setTooltip ("La pagina segue il passo che suona");
    addAndMakeVisible (followBtn);

    setupKnob (lengthKnob, 1, SeqData::numSteps, 16, 1);
    lengthKnob.setTooltip ("Lunghezza della sequenza (1-64 passi)");
    lengthKnob.onValueChange = [this]
    {
        if (updating) return;
        proc.synth.seq.data.length[slot()] = (int) lengthKnob.getValue();
        touch ("SEQUENCER  LENGTH", juce::String ((int) lengthKnob.getValue()) + " STEPS");
        grid.repaint();
    };
    addAndMakeVisible (lengthKnob);

    clearBtn.setTooltip ("Svuota tutta la sequenza (doppio controllo: tieni premuto SHIFT)");
    clearBtn.onClick = [this]
    {
        if (! juce::ModifierKeys::currentModifiers.isShiftDown()) { touch ("CLEAR", "SHIFT + CLICK PER SVUOTARE LA SEQUENZA"); return; }
        proc.synth.seq.data.clearSlot (slot());
        proc.synth.seq.recCursor = 0;
        touch ("SEQUENCER", "SEQUENZA SVUOTATA");
        refreshEditor(); grid.repaint();
    };
    restBtn.setTooltip ("Registrazione passo passo: lascia il passo vuoto e va avanti");
    restBtn.onClick = [this]
    {
        auto& seq = proc.synth.seq;
        const int c = juce::jlimit (0, SeqData::numSteps - 1, seq.recCursor.load());
        step (c).clear();
        seq.recCursor = (c + 1) % length();
        select (seq.recCursor.load());
        touch ("SEQUENCER  REST", "STEP " + juce::String (c + 1));
    };
    copyBtn.onClick = [this] { copyStep (selected); };
    pasteBtn.onClick = [this] { pasteStep (selected); };
    for (auto* b : { &clearBtn, &restBtn, &copyBtn, &pasteBtn }) addAndMakeVisible (*b);

    // catena di sequenze
    chainAddBtn.setTooltip ("Aggiunge in fondo alla catena la sequenza scelta in SEQUENCE (fino a 16)");
    chainAddBtn.onClick = [this]
    {
        const int sl = juce::jlimit (0, SeqData::numSlots - 1, (int) std::lround (proc.state.getRawParameterValue ("G_SEQSLOT")->load()));
        touch ("CHAIN", proc.synth.seq.data.chainAppend (sl) ? chainText() : juce::String ("PIENA: 16 SEQUENZE"));
    };
    chainDelBtn.setTooltip ("Toglie l'ultima sequenza della catena");
    chainDelBtn.onClick = [this] { auto& d = proc.synth.seq.data; if (d.chainLen.load() > 0) d.chainLen.fetch_sub (1); touch ("CHAIN", chainText()); };
    chainClrBtn.setTooltip ("Svuota la catena");
    chainClrBtn.onClick = [this] { proc.synth.seq.data.chainLen = 0; touch ("CHAIN", "VUOTA"); };
    for (auto* b : { &chainAddBtn, &chainDelBtn, &chainClrBtn }) addAndMakeVisible (*b);

    // editor del passo
    setupKnob (velKnob, 1, 127, 100, 1);
    setupKnob (gateKnob, 5, 100, 60, 1);
    setupKnob (probKnob, 0, 100, 100, 1);
    velKnob.setTooltip ("Velocity del passo");
    gateKnob.setTooltip ("Durata della nota, in percentuale del passo");
    probKnob.setTooltip ("Probabilità che il passo suoni");
    velKnob.onValueChange  = [this] { if (! updating) { step (selected).vel  = (uint8_t) velKnob.getValue();  touch ("STEP " + juce::String (selected + 1) + "  VELOCITY", juce::String ((int) velKnob.getValue())); grid.repaint(); } };
    gateKnob.onValueChange = [this] { if (! updating) { step (selected).gate = (uint8_t) gateKnob.getValue(); touch ("STEP " + juce::String (selected + 1) + "  GATE", juce::String ((int) gateKnob.getValue()) + "%"); grid.repaint(); } };
    probKnob.onValueChange = [this] { if (! updating) { step (selected).prob = (uint8_t) probKnob.getValue(); touch ("STEP " + juce::String (selected + 1) + "  PROBABILITY", juce::String ((int) probKnob.getValue()) + "%"); grid.repaint(); } };
    for (int i = 0; i < 4; ++i) timbreBox.addItem (timbreNames[i], i + 1);
    timbreBox.setTooltip ("Quale timbro suona il passo");
    timbreBox.onChange = [this] { if (! updating) { step (selected).timbre = (uint8_t) (timbreBox.getSelectedId() - 1); touch ("STEP " + juce::String (selected + 1) + "  TIMBRE", timbreBox.getText()); grid.repaint(); } };
    tieBtn.setClickingTogglesState (true);
    tieBtn.getProperties().set ("led", true);
    tieBtn.getProperties().set ("ledColour", "ffffb020");
    tieBtn.setTooltip ("Lega le note al passo dopo, se le ripete");
    tieBtn.onClick = [this] { if (! updating) { step (selected).tie = (uint8_t) (tieBtn.getToggleState() ? 1 : 0); grid.repaint(); } };
    auto shift = [this] (int semis)
    {
        step (selected).transpose (semis);
        touch ("STEP " + juce::String (selected + 1), stepNotes (step (selected)));
        refreshEditor(); grid.repaint();
    };
    noteDn.onClick = [shift] { shift (-1); };
    noteUp.onClick = [shift] { shift (1); };
    octDn.onClick = [shift] { shift (-12); };
    octUp.onClick = [shift] { shift (12); };
    for (auto* s : { &velKnob, &gateKnob, &probKnob }) addAndMakeVisible (*s);
    for (auto* c : std::initializer_list<juce::Component*> { &timbreBox, &tieBtn, &noteDn, &noteUp, &octDn, &octUp }) addAndMakeVisible (*c);

    refreshEditor();
    startTimerHz (30);
}

SeqStrip::~SeqStrip() { stopTimer(); }

int SeqStrip::slot() const
{
    // con la catena in riproduzione si vede (e si modifica) la sequenza che sta suonando
    if (proc.synth.seq.chainPos.load() >= 0) return juce::jlimit (0, SeqData::numSlots - 1, proc.synth.seq.playSlot.load());
    return juce::jlimit (0, SeqData::numSlots - 1, (int) std::lround (proc.state.getRawParameterValue ("G_SEQSLOT")->load()));
}

juce::String SeqStrip::chainText() const
{
    auto& d = proc.synth.seq.data;
    const int n = d.chainLen.load(), cur = proc.synth.seq.chainPos.load();
    if (n == 0) return "CHAIN VUOTA";
    juce::String t ("CHAIN");
    for (int i = 0; i < n; ++i)
    {
        const juce::String v (d.chain[i].load() + 1);
        t << " " << (i == cur ? "[" + v + "]" : v);
    }
    return t;
}
int SeqStrip::length() const { return juce::jlimit (1, SeqData::numSteps, proc.synth.seq.data.length[slot()].load()); }
SeqStep& SeqStrip::step (int index) const { return proc.synth.seq.data.step[slot()][juce::jlimit (0, SeqData::numSteps - 1, index)]; }

juce::String SeqStrip::stepNotes (const SeqStep& s)
{
    std::vector<int> ns;
    for (auto& n : s.note) if (n.load() >= 0) ns.push_back (n.load());
    if (ns.empty()) return "REST";
    std::sort (ns.begin(), ns.end());
    juce::StringArray out;
    for (int n : ns) out.add (noteName (n));
    return out.joinIntoString (" ");
}

void SeqStrip::select (int index)
{
    selected = juce::jlimit (0, SeqData::numSteps - 1, index);
    page = selected / 16;
    refreshEditor();
    grid.repaint();
    repaint();
}

void SeqStrip::refreshEditor()
{
    const juce::ScopedValueSetter<bool> svs (updating, true);
    auto& s = step (selected);
    velKnob.setValue (s.vel.load(), juce::dontSendNotification);
    gateKnob.setValue (s.gate.load(), juce::dontSendNotification);
    probKnob.setValue (s.prob.load(), juce::dontSendNotification);
    timbreBox.setSelectedId (s.timbre.load() + 1, juce::dontSendNotification);
    tieBtn.setToggleState (s.tie.load() != 0, juce::dontSendNotification);
    lengthKnob.setValue (length(), juce::dontSendNotification);
    lastSel = selected;
    repaint (0, 86, getWidth(), 42);
}

void SeqStrip::copyStep (int index)
{
    auto& s = step (index);
    for (int i = 0; i < SeqStep::maxNotes; ++i) clip.notes[i] = s.note[i].load();
    clip.vel = s.vel; clip.gate = s.gate; clip.prob = s.prob; clip.timbre = s.timbre; clip.tie = s.tie;
    copyValid = true;
    touch ("STEP " + juce::String (index + 1) + "  COPY", stepNotes (s));
}

void SeqStrip::pasteStep (int index)
{
    if (! copyValid) return;
    auto& s = step (index);
    s.clear();
    for (int n : clip.notes) if (n >= 0) s.add (n);
    s.vel = (uint8_t) clip.vel; s.gate = (uint8_t) clip.gate; s.prob = (uint8_t) clip.prob; s.timbre = (uint8_t) clip.timbre; s.tie = (uint8_t) clip.tie;
    touch ("STEP " + juce::String (index + 1) + "  PASTE", stepNotes (s));
    refreshEditor(); grid.repaint();
}

void SeqStrip::showMenu (int index)
{
    select (index);
    juce::PopupMenu m;
    m.addSectionHeader ("STEP " + juce::String (index + 1));
    m.addItem (1, "CLEAR STEP");
    m.addItem (2, "COPY");
    m.addItem (3, "PASTE", copyValid);
    m.addItem (4, "TIE", true, step (index).tie.load() != 0);
    m.addSeparator();
    m.addItem (5, "REC CURSOR HERE");
    m.addItem (6, "LENGTH ENDS HERE");
    m.showMenuAsync (juce::PopupMenu::Options(), [this, index] (int r)
    {
        auto& s = step (index);
        switch (r)
        {
            case 1: s.clear(); break;
            case 2: copyStep (index); break;
            case 3: pasteStep (index); break;
            case 4: s.tie = (uint8_t) (s.tie.load() ? 0 : 1); break;
            case 5: proc.synth.seq.recCursor = index; break;
            case 6: proc.synth.seq.data.length[slot()] = index + 1; break;
            default: return;
        }
        refreshEditor(); grid.repaint();
    });
}

void SeqStrip::timerCallback()
{
    auto& seq = proc.synth.seq;
    const int ps = seq.playStep.load();
    if (followBtn.getToggleState() && ps >= 0 && ps / 16 != page) page = ps / 16;
    else if (followBtn.getToggleState() && ps < 0 && proc.state.getRawParameterValue ("G_SEQREC")->load() > .5f)
        page = juce::jlimit (0, 3, seq.recCursor.load() / 16);
    for (int i = 0; i < 4; ++i)
        pageBtn[i].setToggleState (i == page, juce::dontSendNotification);
    const int rev = seq.editRevision.load(), sl = slot();
    if (rev != lastRevision || sl != lastSlot)
    {
        lastRevision = rev; lastSlot = sl;
        if (! velKnob.isMouseButtonDown() && ! gateKnob.isMouseButtonDown() && ! probKnob.isMouseButtonDown()) refreshEditor();
    }
    grid.repaint();
}

void SeqStrip::resized()
{
    // a sinistra, sotto i comandi del pannello: lunghezza, pagine, comandi di modifica
    lengthKnob.setBounds (456, 30, 40, 40);
    int x = 0;
    for (int i = 0; i < 4; ++i) { pageBtn[i].setBounds (x, 76, 62, 26); x += 66; }
    followBtn.setBounds (x, 76, 70, 26);
    clearBtn.setBounds (0, 104, 62, 22);
    restBtn.setBounds (66, 104, 62, 22);
    copyBtn.setBounds (132, 104, 62, 22);
    pasteBtn.setBounds (198, 104, 62, 22);
    // CHAIN (parametro del pannello) sta a x 270
    chainAddBtn.setBounds (334, 104, 54, 22);
    chainDelBtn.setBounds (392, 104, 46, 22);
    chainClrBtn.setBounds (442, 104, 46, 22);

    grid.setBounds (gridX, gridY, 16 * cellW + 15 * cellGap, cellH);

    // editor del passo sotto la griglia
    const int y = 84;
    int ex = gridX + 210;
    for (auto* k : { &velKnob, &gateKnob, &probKnob }) { k->setBounds (ex + 40, y + 2, 38, 38); ex += 86; }
    timbreBox.setBounds (ex + 4, y + 10, 96, 24); ex += 106;
    tieBtn.setBounds (ex, y + 10, 52, 24); ex += 58;
    noteDn.setBounds (ex, y + 10, 34, 24); ex += 36;
    noteUp.setBounds (ex, y + 10, 34, 24); ex += 40;
    octDn.setBounds (ex, y + 10, 44, 24); ex += 46;
    octUp.setBounds (ex, y + 10, 44, 24);
}

void SeqStrip::paint (juce::Graphics& g)
{
    // intestazione: chip, linea incisa, LED di stato
    const juce::String title = "SEQUENCER";
    const float cw = ui::chipWidth (title, 16.f);
    ui::paintChip (g, { 0.f, 0.f, cw, 16.f }, title, 14.f);
    auto& seq = proc.synth.seq;
    const bool playing = seq.playStep.load() >= 0;
    const bool rec = proc.state.getRawParameterValue ("G_SEQREC")->load() > .5f;
    juce::String tag = "GLOBAL  -  " + juce::String ("SEQ ") + juce::String (slot() + 1) + "  -  " + juce::String (length()) + " STEPS";
    if (proc.state.getRawParameterValue ("G_SEQCHAIN")->load() > .5f) tag << "  -  " << chainText();
    if (seq.transposeShown.load() != 0) tag << "  -  TRANSPOSE " << (seq.transposeShown.load() > 0 ? "+" : "") << seq.transposeShown.load();
    g.setFont (ui::Fonts::label (11.f));
    const float tw = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), tag) + 6.f;
    ui::paintEngravedLine (g, { cw + 4.f, 8.f }, { (float) getWidth() - tw - 14.f, 8.f });
    g.setColour (C::printDim);
    g.drawText (tag, juce::Rectangle<float> ((float) getWidth() - tw, 1.f, tw, 14.f), juce::Justification::centredRight, false);
    ui::paintLed (g, { (float) getWidth() - tw - 6.f, 8.f }, 2.5f, rec ? C::ledRed : C::ledAmber, rec || playing ? 1.f : .25f);

    // etichette dei comandi della riga in alto (i comandi sono del pannello, le posizioni sono queste)
    g.setColour (C::print);
    g.setFont (ui::Fonts::label (12.f));
    g.drawText ("SEQUENCE", juce::Rectangle<int> (132, 18, 88, 14), juce::Justification::centredLeft, false);
    g.drawText ("RATE",     juce::Rectangle<int> (224, 18, 70, 14), juce::Justification::centredLeft, false);
    g.drawText ("KEYS",     juce::Rectangle<int> (298, 18, 100, 14), juce::Justification::centredLeft, false);
    g.drawText ("SWING",    juce::Rectangle<int> (396, 18, 52, 14), juce::Justification::centred, false);
    g.drawText ("LENGTH",   juce::Rectangle<int> (446, 18, 60, 14), juce::Justification::centred, false);

    // editor del passo
    const int y = 84;
    const auto& s = step (selected);
    ui::paintRecess (g, { (float) gridX, (float) y + 6.f, 200.f, 32.f });
    g.setColour (C::print);
    g.setFont (ui::Fonts::display (17.f));
    g.drawText ("STEP " + juce::String (selected + 1).paddedLeft ('0', 2), juce::Rectangle<int> (gridX + 8, y + 6, 62, 32), juce::Justification::centredLeft, false);
    g.setFont (ui::Fonts::label (12.f));
    g.setColour (s.count() > 0 ? C::print : C::printDim);
    g.drawFittedText (stepNotes (s), juce::Rectangle<int> (gridX + 70, y + 6, 126, 32), juce::Justification::centredLeft, 2);
    g.setColour (C::print);
    int ex = gridX + 210;
    for (auto* l : { "VELO", "GATE", "PROB" })
    {
        g.drawText (l, juce::Rectangle<int> (ex, y + 12, 38, 20), juce::Justification::centredRight, false);
        ex += 86;
    }
}

// ------------------------------------------------------------------------------------------------ griglia
int SeqStrip::Grid::stepAt (juce::Point<int> p) const
{
    const int col = p.x / (cellW + cellGap);
    if (col < 0 || col > 15) return -1;
    return owner.page * 16 + col;
}

void SeqStrip::Grid::paint (juce::Graphics& g)
{
    auto& seq = owner.proc.synth.seq;
    const int len = owner.length(), ps = seq.playStep.load(), rc = seq.recCursor.load();
    const bool rec = owner.proc.state.getRawParameterValue ("G_SEQREC")->load() > .5f;
    const bool running = owner.proc.state.getRawParameterValue ("G_SEQON")->load() > .5f;
    for (int c = 0; c < 16; ++c)
    {
        const int idx = owner.page * 16 + c;
        const auto& s = owner.step (idx);
        const juce::Rectangle<float> r ((float) (c * (cellW + cellGap)), 0.f, (float) cellW, (float) cellH);
        const bool inside = idx < len;
        ui::paintRecess (g, r);
        if (idx == owner.selected)
        {
            g.setColour (C::ledBlue.withAlpha (.85f));
            g.drawRoundedRectangle (r.reduced (.5f), 3.f, 1.4f);
        }
        const float a = inside ? 1.f : .3f;
        // numero e LED (verde: passo che suona; rosso: cursore della registrazione passo passo)
        g.setFont (ui::Fonts::label (10.f));
        g.setColour (C::printDim.withAlpha (a));
        g.drawText (juce::String (idx + 1), r.withHeight (14.f).withTrimmedLeft (5.f), juce::Justification::centredLeft, false);
        juce::Colour led = C::ledGreen;
        float lv = 0.f;
        if (idx == ps) lv = 1.f;
        else if (rec && ! running && idx == rc) { led = C::ledRed; lv = 1.f; }
        ui::paintLed (g, { r.getRight() - 8.f, r.getY() + 7.f }, 3.f, led, lv);

        const int cnt = s.count();
        if (cnt > 0)
        {
            int lo = 127;
            for (auto& n : s.note) if (n.load() >= 0) lo = std::min (lo, (int) n.load());
            g.setColour (C::print.withAlpha (a));
            g.setFont (ui::Fonts::display (17.f));
            g.drawText (noteName (lo), r.withTrimmedTop (12.f).withHeight (20.f), juce::Justification::centred, false);
            g.setFont (ui::Fonts::label (10.f));
            g.setColour (C::printDim.withAlpha (a));
            juce::String sub;
            if (cnt > 1) sub << "+" << (cnt - 1);
            if (s.prob.load() < 100) sub << (sub.isEmpty() ? "" : " ") << (int) s.prob.load() << "%";
            if (s.timbre.load() > 0) sub << (sub.isEmpty() ? "" : " ") << (s.timbre.load() == 1 ? "A" : s.timbre.load() == 2 ? "B" : "AB");
            g.drawText (sub, r.withTrimmedTop (31.f).withHeight (12.f), juce::Justification::centred, false);
            // barre: velocity (bianca) e gate (ambra, piena e con freccia se legato)
            const auto bars = r.withTrimmedTop (r.getHeight() - 12.f).reduced (5.f, 2.f);
            g.setColour (C::print.withAlpha (.75f * a));
            g.fillRect (bars.withHeight (3.f).withWidth (bars.getWidth() * (float) s.vel.load() / 127.f));
            const bool tie = s.tie.load() != 0;
            g.setColour (C::ledAmber.withAlpha (a));
            g.fillRect (bars.withTrimmedTop (5.f).withHeight (3.f).withWidth (tie ? bars.getWidth() + 8.f : bars.getWidth() * (float) s.gate.load() / 100.f));
        }
        else
        {
            g.setColour (C::printDim.withAlpha (.5f * a));
            g.setFont (ui::Fonts::display (16.f));
            g.drawText ("-", r.withTrimmedTop (12.f).withHeight (20.f), juce::Justification::centred, false);
        }
    }
}

void SeqStrip::Grid::mouseDown (const juce::MouseEvent& e)
{
    const int idx = stepAt (e.getPosition());
    if (idx < 0) return;
    if (e.mods.isPopupMenu()) { owner.showMenu (idx); return; }
    if (e.mods.isShiftDown()) owner.proc.synth.seq.recCursor = idx;
    owner.select (idx);
    dragStep = idx;
    dragStartVel = owner.step (idx).vel.load();
    const auto& s = owner.step (idx);
    owner.touch ("STEP " + juce::String (idx + 1), stepNotes (s) + "   VEL " + juce::String ((int) s.vel.load()));
}

void SeqStrip::Grid::mouseDrag (const juce::MouseEvent& e)
{
    if (dragStep < 0 || owner.step (dragStep).count() == 0) return;
    const int v = juce::jlimit (1, 127, dragStartVel - e.getDistanceFromDragStartY());
    owner.step (dragStep).vel = (uint8_t) v;
    owner.touch ("STEP " + juce::String (dragStep + 1) + "  VELOCITY", juce::String (v));
    owner.refreshEditor();
    repaint();
}

void SeqStrip::Grid::mouseUp (const juce::MouseEvent&) { dragStep = -1; }

void SeqStrip::Grid::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int idx = stepAt (e.getPosition());
    if (idx < 0) return;
    auto& s = owner.step (idx);
    if (s.count() > 0)
    {
        for (auto& n : s.note) if (n.load() >= 0) { owner.lastNote = n.load(); break; }
        s.clear();
    }
    else s.add (owner.lastNote);
    owner.touch ("STEP " + juce::String (idx + 1), stepNotes (s));
    owner.refreshEditor();
    repaint();
}

void SeqStrip::Grid::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const int idx = stepAt (e.getPosition());
    if (idx < 0 || owner.step (idx).count() == 0) return;
    wheelAcc += w.deltaY * 8.f;
    const int steps = (int) wheelAcc;
    if (steps == 0) return;
    wheelAcc -= (float) steps;
    owner.step (idx).transpose (steps * (e.mods.isShiftDown() ? 12 : 1));
    owner.select (idx);
    owner.touch ("STEP " + juce::String (idx + 1), stepNotes (owner.step (idx)));
}
