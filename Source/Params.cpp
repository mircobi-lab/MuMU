#include "Params.h"

namespace mumu
{
#define MUMU_DEF(e, key, name, label, type, mn, mx, df, ctr, ch, unit) \
    ParamDef { key, name, label, PType::type, (float) (mn), (float) (mx), (float) (df), (float) (ctr), ch, unit },

const std::vector<ParamDef>& timbreDefs()
{
    static const std::vector<ParamDef> defs { MUMU_TIMBRE_PARAMS (MUMU_DEF) };
    return defs;
}

const std::vector<ParamDef>& globalDefs()
{
    static const std::vector<ParamDef> defs { MUMU_GLOBAL_PARAMS (MUMU_DEF) };
    return defs;
}
#undef MUMU_DEF

juce::String paramID (int timbre, int index)
{
    if (timbre < 0) return juce::String ("G_") + globalDefs()[(size_t) index].key;
    return juce::String (timbre == 0 ? "A_" : "B_") + timbreDefs()[(size_t) index].key;
}

juce::String formatValue (const ParamDef& d, float v)
{
    switch (d.type)
    {
        case PType::C: { auto c = d.choiceList(); return c[juce::jlimit (0, c.size() - 1, (int) std::lround (v))]; }
        case PType::T: return v > .5f ? "ON" : "OFF";
        case PType::I:
        {
            const int n = (int) std::lround (v);
            if (juce::String (d.unit) == "note") return juce::MidiMessage::getMidiNoteName (n, true, true, 4);
            juce::String s = (d.min < 0 && n > 0 ? "+" : "") + juce::String (n);
            return juce::String (d.unit).isEmpty() ? s : s + " " + juce::String (d.unit).toUpperCase();
        }
        case PType::R: break;
    }
    const juce::String u (d.unit);
    if (u == "Hz")   return v >= 1000.f ? juce::String (v / 1000.f, 2) + " KHZ" : juce::String (v, v < 10.f ? 2 : (v < 100.f ? 1 : 0)) + " HZ";
    if (u == "s")    return v < 1.f ? juce::String (v * 1000.f, v < .1f ? 1 : 0) + " MS" : juce::String (v, 2) + " S";
    if (u == "ms")   return juce::String (v, v < 100.f ? 1 : 0) + " MS";
    if (u == "dB")   return v <= -59.9f ? "-INF" : juce::String (v, 1) + " DB";
    if (u == "ct")   return (v > 0 ? "+" : "") + juce::String (v, 1) + " CT";
    if (u == "st")   return (v > 0 ? "+" : "") + juce::String (v, 2) + " ST";
    if (u == "oct")  return (v > 0 ? "+" : "") + juce::String (v, 2) + " OCT";
    if (u == "bpm")  return juce::String (v, 1) + " BPM";
    if (u == "wave")
    {
        if (v < .17f) return "TRIANGLE";
        if (v < .5f)  return v < .3f ? "TRI > SAW" : (v < .4f ? "SAW" : "SAW > SQR");
        if (v < .69f) return "SQUARE";
        return "PULSE " + juce::String ((int) std::lround (50.f - 42.f * (v - 2.f / 3.f) * 3.f)) + "%";
    }
    if (u == "%")    return (d.bipolar() && v > 0 ? "+" : "") + juce::String ((int) std::lround (v * 100.f)) + "%";
    return juce::String (v, 2);
}

static std::unique_ptr<juce::RangedAudioParameter> makeParam (const juce::String& id, const juce::String& name, const ParamDef& d)
{
    const juce::ParameterID pid { id, 1 };
    switch (d.type)
    {
        case PType::C: return std::make_unique<juce::AudioParameterChoice> (pid, name, d.choiceList(), (int) d.def);
        case PType::T: return std::make_unique<juce::AudioParameterChoice> (pid, name, juce::StringArray { "OFF", "ON" }, d.def > .5f ? 1 : 0);
        case PType::I:
            return std::make_unique<juce::AudioParameterInt> (pid, name, (int) d.min, (int) d.max, (int) d.def,
                       juce::AudioParameterIntAttributes().withStringFromValueFunction ([d] (int v, int) { return formatValue (d, (float) v); }));
        case PType::R: break;
    }
    juce::NormalisableRange<float> r (d.min, d.max);
    if (d.centre > 0.f && d.centre > d.min && d.centre < d.max) r.setSkewForCentre (d.centre);
    return std::make_unique<juce::AudioParameterFloat> (pid, name, r, d.def,
               juce::AudioParameterFloatAttributes().withStringFromValueFunction ([d] (float v, int) { return formatValue (d, v); }));
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    const auto& g = globalDefs();
    for (size_t i = 0; i < g.size(); ++i)
        layout.add (makeParam (paramID (-1, (int) i), juce::String ("Global ") + g[i].name, g[i]));
    const auto& t = timbreDefs();
    for (int tm = 0; tm < 2; ++tm)
        for (size_t i = 0; i < t.size(); ++i)
            layout.add (makeParam (paramID (tm, (int) i), juce::String (tm == 0 ? "A " : "B ") + t[i].name, t[i]));
    return layout;
}

void ParamReader::attach (juce::AudioProcessorValueTreeState& s)
{
    for (int t = 0; t < 2; ++t)
        for (int i = 0; i < TP_COUNT; ++i)
        {
            tp[t][i] = s.getRawParameterValue (paramID (t, i));
            jassert (tp[t][i] != nullptr);
        }
    for (int i = 0; i < GP_COUNT; ++i)
    {
        gp[i] = s.getRawParameterValue (paramID (-1, i));
        jassert (gp[i] != nullptr);
    }
}

void ParamReader::read (TimbreValues (&t)[2], GlobalValues& g) const
{
    for (int k = 0; k < 2; ++k)
        for (int i = 0; i < TP_COUNT; ++i)
            t[k].v[i] = tp[k][i] != nullptr ? tp[k][i]->load (std::memory_order_relaxed) : timbreDefs()[(size_t) i].def;
    for (int i = 0; i < GP_COUNT; ++i)
        g.v[i] = gp[i] != nullptr ? gp[i]->load (std::memory_order_relaxed) : globalDefs()[(size_t) i].def;
}
}
