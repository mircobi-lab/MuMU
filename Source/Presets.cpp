#include "Presets.h"

/*  Preset di fabbrica MBStudio91, originali (non copiati da banchi di altri strumenti).
    Sorgenti matrice: 1 LFO1 2 LFO2 3 MODOSC 4 FENV 5 AENV 6 VEL 7 KEY 8 WHEEL 9 AFTER 10 BEND 11 RND 12 MACRO
    Destinazioni:     1 PITCH 2 O1P 3 O2P 4 O1W 5 O2W 6 O1L 7 O2L 8 MOL 9 NOISE 10 FM 11 DRIVE 12 F1C 13 F2C
                      14 F1R 15 F2R 16 AMP 17 PAN 18 L1R 19 L2R 20 MOF 21 VIB                                     */
namespace mumu
{
const std::vector<Preset>& factoryPresets()
{
    static const std::vector<Preset> presets
    {
        { "BASIC SAW", { { "G_MASTER", -9.6f } } },

        { "FAT BRASS", { { "G_MASTER", -10.4f },
            { "A_O1WAVE", .34f }, { "A_O2WAVE", .34f }, { "A_O2FINE", 6.f }, { "A_MIX2", .75f },
            { "A_F1CUT", 520.f }, { "A_F1RES", .12f }, { "A_FENVAMT", .55f }, { "A_FVELO", .45f },
            { "A_FA", .09f }, { "A_FD", .9f }, { "A_FS", .5f }, { "A_FR", .35f },
            { "A_AA", .04f }, { "A_AD", .6f }, { "A_AS", .9f }, { "A_AR", .3f }, { "A_DRIVE", .45f },
            { "A_M1SRC", 9.f }, { "A_M1DST", 12.f }, { "A_M1AMT", .25f }, { "A_PLWHEEL", .4f },
            { "G_DLMIX", .14f }, { "G_DLDIFF", .5f } } },

        { "LADDER BASS", { { "G_MASTER", -3.4f },
            { "A_VMODE", 1.f }, { "A_GLIDE", .05f }, { "A_O1OCT", 1.f }, { "A_O2OCT", 1.f }, { "A_O2WAVE", .6f }, { "A_O2FINE", -5.f },
            { "A_F1CUT", 210.f }, { "A_F1RES", .45f }, { "A_FENVAMT", .6f }, { "A_FVELO", .5f }, { "A_KEYTRK", .3f },
            { "A_FA", .001f }, { "A_FD", .32f }, { "A_FS", .1f }, { "A_FR", .15f },
            { "A_AA", .001f }, { "A_AD", .5f }, { "A_AS", .8f }, { "A_AR", .08f }, { "A_DRIVE", .6f }, { "A_SPREAD", 0.f },
            { "G_DLON", 0.f } } },

        { "WIDE STRINGS", { { "G_MASTER", -8.9f },
            { "A_O1WAVE", .34f }, { "A_O2WAVE", .3f }, { "A_O2FINE", 9.f }, { "A_MIX2", .7f },
            { "A_F1CUT", 2600.f }, { "A_F1RES", .05f }, { "A_FENVAMT", .12f }, { "A_FA", .5f }, { "A_FD", 1.5f }, { "A_FS", .7f },
            { "A_AA", .6f }, { "A_AD", 1.f }, { "A_AS", .9f }, { "A_AR", 1.3f }, { "A_DRIVE", .15f },
            { "A_L1RATE", .35f }, { "A_L1WAVE", 1.f }, { "A_M1SRC", 1.f }, { "A_M1DST", 3.f }, { "A_M1AMT", .005f },
            { "A_L2RATE", 5.2f }, { "A_M2SRC", 2.f }, { "A_M2DST", 4.f }, { "A_M2AMT", .06f }, { "A_SPREAD", .9f },
            { "G_DLMIX", .32f }, { "G_DLDIFF", .75f }, { "G_DLCHAR", -.65f }, { "G_DLFB", .45f }, { "G_DLTIME", 520.f } } },

        { "STEREO DRIFT PAD", { { "G_MASTER", -5.5f },
            { "A_FROUTE", 2.f }, { "A_F1CUT", 900.f }, { "A_F2CUT", 1400.f }, { "A_F1RES", .35f }, { "A_F2RES", .35f },
            { "A_O1WAVE", .5f }, { "A_O2WAVE", .2f }, { "A_O2FINE", -7.f }, { "A_FENVAMT", .2f },
            { "A_FA", 1.2f }, { "A_FD", 2.f }, { "A_FS", .6f }, { "A_FR", 2.5f },
            { "A_AA", 1.1f }, { "A_AD", 2.f }, { "A_AS", .85f }, { "A_AR", 2.8f },
            { "A_L1RATE", .13f }, { "A_L2RATE", .19f }, { "A_L2WAVE", 6.f },
            { "A_M1SRC", 1.f }, { "A_M1DST", 12.f }, { "A_M1AMT", .25f }, { "A_M2SRC", 2.f }, { "A_M2DST", 13.f }, { "A_M2AMT", .3f },
            { "A_M3SRC", 12.f }, { "A_M3DST", 14.f }, { "A_M3AMT", .4f },
            { "G_DLMIX", .38f }, { "G_DLDIFF", .85f }, { "G_DLCHAR", -.8f }, { "G_DLFB", .55f }, { "G_DLTIME", 740.f } } },

        { "SYNC LEAD", { { "G_MASTER", -5.6f },
            { "A_VMODE", 1.f }, { "A_GLIDE", .07f }, { "A_SYNC", 1.f }, { "A_O2SEMI", 7.f }, { "A_O2WAVE", .34f }, { "A_MIX1", .3f }, { "A_MIX2", .9f },
            { "A_F1CUT", 3200.f }, { "A_F1RES", .2f }, { "A_FENVAMT", .2f },
            { "A_FA", .001f }, { "A_FD", .7f }, { "A_FS", .3f },
            { "A_M1SRC", 4.f }, { "A_M1DST", 3.f }, { "A_M1AMT", .5f }, { "A_M2SRC", 8.f }, { "A_M2DST", 3.f }, { "A_M2AMT", .35f },
            { "A_PLWHEEL", .0f }, { "A_PLAMT", .15f }, { "A_PLRATE", 5.8f },
            { "G_DLMIX", .24f }, { "G_DLSYNC", 6.f }, { "G_DLFB", .4f }, { "G_DLDIFF", .2f } } },

        { "VELVET CLAV", { { "G_MASTER", -5.8f },
            { "A_O1WAVE", .86f }, { "A_MIX2", 0.f }, { "A_F1CUT", 900.f }, { "A_F1RES", .25f }, { "A_FENVAMT", .55f }, { "A_FVELO", .7f },
            { "A_FA", .001f }, { "A_FD", .18f }, { "A_FS", 0.f }, { "A_FR", .1f },
            { "A_AA", .001f }, { "A_AD", .5f }, { "A_AS", 0.f }, { "A_AR", .08f }, { "A_AVELO", .8f }, { "A_DRIVE", .3f },
            { "A_M1SRC", 6.f }, { "A_M1DST", 4.f }, { "A_M1AMT", -.1f },
            { "G_DLMIX", .1f }, { "G_DLDIFF", .3f } } },

        { "DRAWBAR ORGAN", { { "G_MASTER", -10.7f },
            { "A_O1WAVE", 0.f }, { "A_O2WAVE", 0.f }, { "A_O2OCT", 3.f }, { "A_MIX1", .8f }, { "A_MIX2", .5f },
            { "A_MOTRACK", 0.f }, { "A_MOWAVE", 0.f }, { "A_MOPITCH", 19.f }, { "A_MIXMO", .35f },
            { "A_F1CUT", 9000.f }, { "A_FENVAMT", 0.f }, { "A_KEYTRK", 0.f },
            { "A_AA", .002f }, { "A_AD", .1f }, { "A_AS", 1.f }, { "A_AR", .05f }, { "A_AVELO", 0.f }, { "A_DRIVE", .2f },
            { "G_DLTIME", 7.f }, { "G_DLFB", .1f }, { "G_DLMIX", .4f }, { "G_DLCHAR", -1.f }, { "G_DLDIFF", .15f } } },

        { "RING BELLS", { { "G_MASTER", 0.4f },
            { "A_O1WAVE", 0.f }, { "A_O2WAVE", 0.f }, { "A_O2OCT", 4.f }, { "A_O2SEMI", -5.f }, { "A_O2FINE", 0.f },
            { "A_MIX1", .15f }, { "A_MIX2", 0.f }, { "A_MIXRING", .85f },
            { "A_F1CUT", 7000.f }, { "A_FENVAMT", -.2f }, { "A_FA", .001f }, { "A_FD", 1.5f }, { "A_FS", 0.f },
            { "A_AA", .001f }, { "A_AD", 2.6f }, { "A_AS", 0.f }, { "A_AR", 2.2f }, { "A_DRIVE", .1f },
            { "G_DLMIX", .36f }, { "G_DLDIFF", .85f }, { "G_DLFB", .5f }, { "G_DLTIME", 430.f } } },

        { "SPLIT BASS + KEYS", { { "G_MASTER", -7.4f },
            { "G_KEYMODE", 3.f }, { "G_SPLIT", 55.f },
            { "A_O1WAVE", .75f }, { "A_O2WAVE", .2f }, { "A_O2OCT", 3.f }, { "A_MIX2", .35f },
            { "A_F1CUT", 1600.f }, { "A_FENVAMT", .3f }, { "A_FD", .5f }, { "A_FS", .3f }, { "A_AD", 1.2f }, { "A_AS", .4f }, { "A_AR", .5f },
            { "B_VMODE", 1.f }, { "B_O1OCT", 1.f }, { "B_O2OCT", 1.f }, { "B_O2WAVE", .6f }, { "B_F1CUT", 260.f }, { "B_F1RES", .35f },
            { "B_FENVAMT", .5f }, { "B_FD", .3f }, { "B_FS", .15f }, { "B_AR", .08f }, { "B_DRIVE", .55f }, { "B_SPREAD", 0.f },
            { "G_DLMIX", .15f } } },

        { "LAYER BRASS + AIR", { { "G_MASTER", -9.7f },
            { "G_KEYMODE", 2.f },
            { "A_O2FINE", 5.f }, { "A_F1CUT", 600.f }, { "A_FENVAMT", .5f }, { "A_FA", .07f }, { "A_FD", .8f }, { "A_FS", .5f },
            { "A_AA", .05f }, { "A_AR", .4f }, { "A_DRIVE", .4f },
            { "B_OCT", 1.f }, { "B_O1WAVE", .0f }, { "B_O2WAVE", .1f }, { "B_O2FINE", -8.f }, { "B_MIXNOISE", .15f },
            { "B_F1MODE", 1.f }, { "B_F1CUT", 700.f }, { "B_F2CUT", 6000.f }, { "B_FENVAMT", 0.f },
            { "B_AA", .9f }, { "B_AR", 1.6f }, { "B_LEVEL", .5f }, { "B_SPREAD", 1.f },
            { "G_DLMIX", .3f }, { "G_DLDIFF", .8f }, { "G_DLCHAR", -.5f } } },

        { "METAL FM", { { "G_MASTER", -2.7f },
            { "A_MOTRACK", 0.f }, { "A_MOWAVE", 0.f }, { "A_MOPITCH", 12.f }, { "A_MOAPITCH", .38f },
            { "A_O1WAVE", 0.f }, { "A_MIX2", 0.f }, { "A_F1CUT", 5000.f }, { "A_FENVAMT", -.15f },
            { "A_AA", .001f }, { "A_AD", 1.4f }, { "A_AS", .15f }, { "A_AR", .9f },
            { "A_M1SRC", 5.f }, { "A_M1DST", 20.f }, { "A_M1AMT", .1f },
            { "G_DLMIX", .25f }, { "G_DLDIFF", .6f } } },

        { "LOOPING PULSE", { { "G_MASTER", -7.4f },
            { "A_O1WAVE", .6f }, { "A_O2WAVE", .6f }, { "A_O2FINE", 3.f },
            { "A_F1CUT", 300.f }, { "A_F1RES", .55f }, { "A_FENVAMT", .55f },
            { "A_FA", .001f }, { "A_FD", .14f }, { "A_FS", 0.f }, { "A_FLOOP", 1.f }, { "A_FCURVE", -.5f },
            { "A_AA", .01f }, { "A_AS", 1.f }, { "A_AR", .4f },
            { "A_M1SRC", 12.f }, { "A_M1DST", 12.f }, { "A_M1AMT", .4f },
            { "G_DLSYNC", 5.f }, { "G_DLMIX", .3f }, { "G_DLPING", 1.f }, { "G_DLFB", .45f }, { "G_DLDIFF", .2f } } },

        { "UNISON SWARM", { { "G_MASTER", -9.8f },
            { "A_VMODE", 2.f }, { "A_UNIVOX", 6.f }, { "A_UNIDET", .4f }, { "A_GLIDE", .04f },
            { "A_O1WAVE", .34f }, { "A_O2WAVE", .34f }, { "A_O2OCT", 1.f },
            { "A_F1CUT", 2200.f }, { "A_F1RES", .25f }, { "A_FENVAMT", .25f }, { "A_FD", .6f }, { "A_FS", .6f },
            { "A_DRIVE", .5f }, { "A_SPREAD", 1.f }, { "A_LEVEL", .55f },
            { "G_DLMIX", .22f }, { "G_DLSYNC", 6.f } } },

        { "ARP CASCADE", { { "G_MASTER", 0.1f },
            { "G_ARPON", 1.f }, { "G_ARPMODE", 2.f }, { "G_ARPOCT", 2.f }, { "G_ARPRATE", 3.f }, { "G_ARPGATE", .45f },
            { "A_O1WAVE", .45f }, { "A_O2WAVE", .7f }, { "A_O2OCT", 3.f }, { "A_MIX2", .35f },
            { "A_F1CUT", 700.f }, { "A_F1RES", .4f }, { "A_FENVAMT", .5f }, { "A_FA", .001f }, { "A_FD", .2f }, { "A_FS", 0.f },
            { "A_AA", .001f }, { "A_AD", .35f }, { "A_AS", .2f }, { "A_AR", .2f },
            { "A_M1SRC", 1.f }, { "A_M1DST", 12.f }, { "A_M1AMT", .2f }, { "A_L1RATE", .08f },
            { "G_DLSYNC", 6.f }, { "G_DLMIX", .3f }, { "G_DLFB", .5f }, { "G_DLPING", 1.f }, { "G_DLDIFF", .35f } } },

        { "NOTCH SWEEP", { { "G_MASTER", -1.7f },
            { "A_FLINK", 1.f }, { "A_F1MODE", 1.f }, { "A_FROUTE", 1.f }, { "A_FSPACE", 1.3f }, { "A_F2CUT", 900.f },
            { "A_F1RES", .3f }, { "A_F2RES", .3f }, { "A_FENVAMT", 0.f },
            { "A_O1WAVE", .34f }, { "A_O2WAVE", .34f }, { "A_O2FINE", 8.f }, { "A_MIXNOISE", .1f },
            { "A_AA", .4f }, { "A_AR", 1.5f },
            { "A_L1RATE", .09f }, { "A_M1SRC", 1.f }, { "A_M1DST", 13.f }, { "A_M1AMT", .35f },
            { "G_DLMIX", .3f }, { "G_DLDIFF", .7f }, { "G_DLCHAR", .5f } } },

        { "NOISE WIND", { { "G_MASTER", 2.5f },
            { "A_MIX1", 0.f }, { "A_MIX2", 0.f }, { "A_MIXNOISE", .9f },
            { "A_F1CUT", 800.f }, { "A_F1RES", .72f }, { "A_FENVAMT", 0.f }, { "A_KEYTRK", 1.f },
            { "A_AA", 1.5f }, { "A_AR", 2.5f },
            { "A_L2WAVE", 6.f }, { "A_L2RATE", .25f }, { "A_M1SRC", 2.f }, { "A_M1DST", 12.f }, { "A_M1AMT", .3f },
            { "A_M2SRC", 1.f }, { "A_M2DST", 17.f }, { "A_M2AMT", .7f }, { "A_L1RATE", .17f },
            { "G_DLMIX", .4f }, { "G_DLDIFF", .9f }, { "G_DLFB", .6f } } },

        { "CRUNCH SQUARE", { { "G_MASTER", -5.7f },
            { "A_O1WAVE", .62f }, { "A_O2WAVE", .62f }, { "A_O2OCT", 1.f }, { "A_MIX2", .5f },
            { "A_DRIVE", .9f }, { "A_OFFSET", .6f },
            { "A_F1CUT", 1500.f }, { "A_F1RES", .3f }, { "A_FENVAMT", .3f }, { "A_FD", .4f }, { "A_FS", .4f },
            { "A_LEVEL", .6f }, { "G_DLMIX", .12f } } },
    };
    return presets;
}

void applyPreset (juce::AudioProcessorValueTreeState& state, const Preset& p)
{
    for (auto* rp : state.processor.getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (rp))
            r->setValueNotifyingHost (r->getDefaultValue());
    for (auto& kv : p.values)
        if (auto* r = state.getParameter (kv.first))
            r->setValueNotifyingHost (r->convertTo0to1 (kv.second));
        else
            jassertfalse;
}
}
