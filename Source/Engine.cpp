#include "Engine.h"

namespace mumu
{
namespace
{
constexpr float twoPi = 6.28318530718f;
constexpr int controlBlock = 16;            // passo di calcolo di matrice, LFO e glide

inline float clamp01 (float x) noexcept { return x < 0.f ? 0.f : (x > 1.f ? 1.f : x); }
inline float clampf (float x, float a, float b) noexcept { return x < a ? a : (x > b ? b : x); }

inline float polyBlep (float t, float dt) noexcept
{
    if (dt <= 0.f) return 0.f;
    if (t < dt)       { t /= dt; return t + t - t * t - 1.f; }
    if (t > 1.f - dt) { t = (t - 1.f) / dt; return t * t + t + t + 1.f; }
    return 0.f;
}

// durata in battute (semiminime) delle divisioni di clock degli LFO
constexpr double lfoBeats[12] = { 0, 16, 8, 4, 2, 1, 2.0 / 3.0, .5, 1.0 / 3.0, .25, 1.0 / 6.0, .125 };

double lfoRate (const TimbreValues& tv, int which, double bpm)
{
    const int sync = tv.i (which == 0 ? L1SYNC : L2SYNC);
    if (sync > 0 && sync < 12) return bpm / 60.0 / lfoBeats[sync];
    return tv[which == 0 ? L1RATE : L2RATE];
}

enum Src { sOff, sLfo1, sLfo2, sModOsc, sFiltEnv, sAmpEnv, sVelocity, sKey, sWheel, sAfter, sBend, sRandom, sMacro, sCount };
enum Dst { dOff, dPitch, dO1Pitch, dO2Pitch, dO1Wave, dO2Wave, dO1Lvl, dO2Lvl, dMoLvl, dNoise, dFM, dDrive, dF1Cut, dF2Cut,
           dF1Res, dF2Res, dAmp, dPan, dL1Rate, dL2Rate, dMoFreq, dVib, dCount };

inline double noteToInc (float semis, double sr) noexcept
{
    const double f = 440.0 * std::exp2 ((semis - 69.0) / 12.0);
    return std::min (f / sr, .45);
}
}

namespace dsp
{
// ------------------------------------------------------------------------------------------------ VCO
float VCO::tick (double inc, float wave, bool& wrapped, double& frac) noexcept
{
    const float t = (float) phase, dt = (float) inc;
    const float w = clamp01 (wave);
    auto saw = [&] { return 2.f * t - 1.f - polyBlep (t, dt); };
    auto pulse = [&] (float pw)
    {
        float v = t < pw ? 1.f : -1.f;
        v += polyBlep (t, dt);
        float t2 = t - pw; if (t2 < 0.f) t2 += 1.f;
        return v - polyBlep (t2, dt);
    };
    float out;
    if (w < 1.f / 3.f)
    {
        const float a = w * 3.f, tri = 1.f - 4.f * std::abs (t - .5f);
        out = tri + (saw() - tri) * a;
    }
    else if (w < 2.f / 3.f)
    {
        const float a = (w - 1.f / 3.f) * 3.f, s = saw();
        out = s + (pulse (.5f) - s) * a;
    }
    else
        out = pulse (.5f - .42f * (w - 2.f / 3.f) * 3.f);

    phase += inc;
    wrapped = false;
    if (phase >= 1.0)
    {
        phase -= std::floor (phase);
        wrapped = true;
        frac = inc > 0 ? phase / inc : 0;
    }
    return out;
}

float ModOsc::tick (double inc, int shape) noexcept
{
    const float t = (float) phase, dt = (float) inc;
    float out = 0;
    switch (shape)
    {
        case 0: out = std::sin (twoPi * t); break;
        case 1: out = 1.f - 4.f * std::abs (t - .5f); break;
        case 2: out = 1.f - 2.f * t + polyBlep (t, dt); break;
        case 3:
        {
            out = t < .5f ? 1.f : -1.f;
            out += polyBlep (t, dt);
            float t2 = t - .5f; if (t2 < 0.f) t2 += 1.f;
            out -= polyBlep (t2, dt);
            break;
        }
        default: out = held; break;
    }
    phase += inc;
    if (phase >= 1.0) { phase -= std::floor (phase); held = rng.next(); }
    return out;
}

// ------------------------------------------------------------------------------------------------ ladder
float Ladder::process (float x, float g, float k, bool highPass) noexcept
{
    const float G = g / (1.f + g), b = 1.f / (1.f + g);
    const float G2 = G * G, G3 = G2 * G, G4 = G3 * G;
    const float sigma = b * (G3 * s[0] + G2 * s[1] + G * s[2] + s[3]);
    const float xin = highPass ? x : x * (1.f + .45f * k);       // il ladder perde bassi con la risonanza: compenso a metà
    const float y4lin = (G4 * xin + sigma) / (1.f + k * G4);
    const float u = std::tanh (xin - k * y4lin);
    const float y1 = G * u + b * s[0];  s[0] = 2.f * y1 - s[0];
    const float y2 = G * y1 + b * s[1]; s[1] = 2.f * y2 - s[1];
    const float y3 = G * y2 + b * s[2]; s[2] = 2.f * y3 - s[2];
    const float y4 = G * y3 + b * s[3]; s[3] = 2.f * y4 - s[3];
    return highPass ? (u - 4.f * y1 + 6.f * y2 - 4.f * y3 + y4) : y4;
}

// ------------------------------------------------------------------------------------------------ inviluppo
void Envelope::set (float a, float d, float s, float r, float curve, bool loop, double sr) noexcept
{
    auto coef = [sr] (float t, float ratio)
    {
        const double rate = std::max (1.0, (double) t * sr);
        return (float) std::exp (-std::log ((1.0 + ratio) / ratio) / rate);
    };
    const float ra = .3f * std::pow (10.f, curve * 1.5f);           // curva -1 esponenziale ... +1 quasi lineare
    const float rd = .0001f * std::pow (10.f, (curve + 1.f) * 2.f);
    aCoef = coef (a, ra); aBase = (1.f + ra) * (1.f - aCoef);
    dCoef = coef (d, rd); dBase = (s - rd) * (1.f - dCoef);
    rCoef = coef (r, rd); rBase = -rd * (1.f - rCoef);
    sus = s;
    looping = loop;
}

float Envelope::tick() noexcept
{
    switch (stage)
    {
        case attack:
            out = aBase + out * aCoef;
            if (out >= 1.f) { out = 1.f; stage = decay; }
            break;
        case decay:
            out = dBase + out * dCoef;
            if (out <= sus) { out = sus; stage = looping ? attack : sustain; }
            break;
        case sustain: out = sus; break;
        case release:
            out = rBase + out * rCoef;
            if (out <= 0.f) { out = 0.f; stage = idle; }
            break;
        case idle: break;
    }
    return out;
}

// ------------------------------------------------------------------------------------------------ LFO
float LFO::advance (double inc, int shape) noexcept
{
    phase += inc;
    if (phase >= 1.0)
    {
        phase -= std::floor (phase);
        held = rng.next();
        a = b; b = rng.next();
    }
    const float t = (float) phase;
    switch (shape)
    {
        case 0: return std::sin (twoPi * t);
        case 1: return 1.f - 4.f * std::abs (t - .5f);
        case 2: return 1.f - 2.f * t;
        case 3: return 2.f * t - 1.f;
        case 4: return t < .5f ? 1.f : -1.f;
        case 5: return held;
        default: { const float s = t * t * (3.f - 2.f * t); return a + (b - a) * s; }
    }
}

// ------------------------------------------------------------------------------------------------ linee di ritardo
void DelayLine::init (int minSize)
{
    int n = 1;
    while (n < minSize + 4) n <<= 1;
    buf.assign ((size_t) n, 0.f);
    mask = n - 1;
    w = 0;
}

float DelayLine::read (float delay) const noexcept
{
    delay = std::max (2.f, delay);
    float rp = (float) w - delay;
    while (rp < 0.f) rp += (float) (mask + 1);
    const int i1 = (int) rp;
    const float f = rp - (float) i1;
    const float y0 = buf[(size_t) ((i1 - 1) & mask)], y1 = buf[(size_t) (i1 & mask)];
    const float y2 = buf[(size_t) ((i1 + 1) & mask)], y3 = buf[(size_t) ((i1 + 2) & mask)];
    const float c1 = .5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.f * y2 - .5f * y3, c3 = .5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * f + c2) * f + c1) * f + y1;
}

namespace
{
constexpr float preMs[2][4]  = { { 4.77f, 3.59f, 12.73f, 9.31f }, { 5.13f, 3.91f, 11.97f, 8.83f } };
constexpr float loopMs[2][3] = { { 22.6f, 30.1f, 17.3f }, { 24.3f, 28.7f, 19.1f } };
constexpr double chorusHz[6] = { .31, .43, .57, .71, 1.13, 1.7 };
constexpr double delayBeats[9] = { 0, 2, 1, 1.5, 2.0 / 3.0, .5, .75, 1.0 / 3.0, .25 };
}

void DiffusionDelay::prepare (double sampleRate)
{
    sr = sampleRate;
    for (int c = 0; c < 2; ++c)
    {
        for (int j = 0; j < 4; ++j) pre[c][j].line.init ((int) (sr * .02));
        for (int j = 0; j < 3; ++j) loop[c][j].line.init ((int) (sr * .04));
        main[c].init ((int) (sr * 2.2));
    }
    reset();
}

void DiffusionDelay::reset()
{
    for (int c = 0; c < 2; ++c)
    {
        for (auto& a : pre[c]) a.line.clear();
        for (auto& a : loop[c]) a.line.clear();
        main[c].clear();
        lp[c] = fb[c] = 0;
    }
    timeSm = 0;
    mixSm = 0;
}

void DiffusionDelay::process (float* L, float* R, int n, const GlobalValues& gv, double bpm) noexcept
{
    const bool on = gv.b (DLON);
    if (! on)
    {
        if (wasOn) reset();
        wasOn = false;
        return;
    }
    const int sync = gv.i (DLSYNC);
    const double ms = (sync > 0 && sync < 9) ? delayBeats[sync] * 60000.0 / std::max (20.0, bpm) : (double) gv[DLTIME];
    const float preTotal = (float) ((preMs[0][0] + preMs[0][1] + preMs[0][2] + preMs[0][3]) * sr / 1000.0);
    const float target = clampf ((float) (ms * sr / 1000.0) - preTotal, 2.f, (float) (sr * 2.1));
    if (! wasOn) { timeSm = target; mixSm = 0; }
    wasOn = true;

    const float fbk = clampf (gv[DLFB], 0.f, .95f), mix = clamp01 (gv[DLMIX]), diff = clamp01 (gv[DLDIFF]), ch = clampf (gv[DLCHAR], -1.f, 1.f);
    const bool ping = gv.b (DLPING);
    const float fc = 800.f * std::exp2 (gv[DLTONE] * 4.3f);
    const float lpA = 1.f - std::exp (-twoPi * fc / (float) sr);
    const float preG = diff * .7f;
    const float loopG = clampf (diff * .55f * (1.f + std::max (0.f, ch) * .6f), 0.f, .78f);
    const float modD = std::max (0.f, -ch) * .0015f * (float) sr;
    const float mainMod = std::max (0.f, -ch) * .003f * (float) sr;
    const float msToS = (float) sr / 1000.f;

    for (int i = 0; i < n; ++i)
    {
        timeSm += (target - timeSm) * .0005f;
        mixSm += (mix - mixSm) * .001f;
        float lv[6];
        for (int k = 0; k < 6; ++k)
        {
            lfoPh[k] += chorusHz[k] / sr;
            if (lfoPh[k] >= 1.0) lfoPh[k] -= 1.0;
            lv[k] = std::sin (twoPi * (float) lfoPh[k]);
        }
        const float inL = L[i], inR = R[i];
        float x[2] = { ping ? (inL + inR) * .5f : inL, ping ? 0.f : inR };
        for (int c = 0; c < 2; ++c)
            for (int j = 0; j < 4; ++j)
                x[c] = pre[c][j].process (x[c], preG, preMs[c][j] * msToS + modD * lv[(j + 3 * c) % 6] + 2.f);

        const float d0 = main[0].read (timeSm + mainMod * (1.f + lv[0])), d1 = main[1].read (timeSm + mainMod * (1.f + lv[3]));
        float f[2] = { d0, d1 };
        for (int c = 0; c < 2; ++c)
        {
            lp[c] += (f[c] - lp[c]) * lpA;
            f[c] = lp[c];
            for (int j = 0; j < 3; ++j)
                f[c] = loop[c][j].process (f[c], loopG, loopMs[c][j] * msToS + modD * lv[(j * 2 + c) % 6] + 2.f);
            f[c] *= fbk;
            f[c] = f[c] / std::sqrt (1.f + f[c] * f[c] * .25f);      // limita dolcemente il ritorno
        }
        if (ping) { main[0].write (x[0] + f[1]); main[1].write (f[0]); }
        else      { main[0].write (x[0] + f[0]); main[1].write (x[1] + f[1]); }

        L[i] = inL * (1.f - mixSm * .5f) + d0 * mixSm;
        R[i] = inR * (1.f - mixSm * .5f) + d1 * mixSm;
    }
}
}

// ================================================================================================ voce
void Voice::updateEnvelopes (const TimbreValues& tv, double sampleRate)
{
    sr = sampleRate;
    fenv.set (tv[FA], tv[FD], tv[FS], tv[FR], tv[FCURVE], tv.b (FLOOP), sr);
    aenv.set (tv[AA], tv[AD], tv[AS], tv[AR], tv[ACURVE], tv.b (ALOOP), sr);
}

void Voice::setGlideTarget (float target, const TimbreValues& tv)
{
    pitchTarget = target;
    const float g = tv[GLIDE];
    if (g <= .0005f) { pitchCur = target; glideStep = 0; return; }
    switch (tv.i (GLMODE))
    {
        case 1:  glideStep = (float) (12.0 / (g * sr)); break;                              // velocità costante: GLIDE = secondi per ottava
        case 2:  glideStep = (float) (std::abs (target - pitchCur) / (g * sr)); break;      // tempo costante
        default: glideStep = 0; break;                                                      // esponenziale
    }
}

void Voice::start (int tm, int nt, float v, const TimbreValues& tv, const VoiceContext& c, double ph0, double ph1, float glideFrom)
{
    const bool wasSounding = active && aenv.active();
    timbre = tm; note = nt; vel = v;
    rnd = noise.uni();
    active = true; gate = true; sustained = false;
    updateEnvelopes (tv, c.sr);
    fenv.gateOn (! wasSounding);
    aenv.gateOn (! wasSounding);
    lfo[0].phase = tv.b (L1KEY) ? 0.0 : ph0;
    lfo[1].phase = tv.b (L2KEY) ? 0.0 : ph1;
    if (! wasSounding) { f1.reset(); f2.reset(); dcX = dcY = 0; }
    pitchCur = (glideFrom >= 0.f && tv[GLIDE] > .0005f) ? glideFrom : (float) nt;
    setGlideTarget ((float) nt, tv);
}

void Voice::moveTo (int nt, float v, const TimbreValues& tv, bool retrig)
{
    note = nt; vel = v; gate = true; sustained = false;
    if (retrig) { fenv.gateOn (false); aenv.gateOn (false); }
    setGlideTarget ((float) nt, tv);
}

void Voice::render (float* L, float* R, int n, const TimbreValues& tv, const VoiceContext& c) noexcept
{
    if (! active) return;
    sr = c.sr;
    const double invSr = 1.0 / sr;
    const float glide = tv[GLIDE];
    const int glMode = tv.i (GLMODE);
    const float level = tv[LEVEL] * 1.f;
    const int route = tv.i (FROUTE);
    const bool hp1 = tv.i (F1MODE) == 1, link = tv.b (FLINK), sync = tv.b (SYNC);
    const int moShape = tv.i (MOWAVE);
    const float moP = tv[MOAPITCH], moPitchOct = moP * moP * 2.f, moCut = tv[MOACUT] * 4.f, moWave = tv[MOAWAVE] * .5f, moAmp = tv[MOAAMP];
    const float offset = tv[OFFSET] * .7f, offTanh = std::tanh (offset);
    const float velGain = 1.f - tv[AVELO] + tv[AVELO] * vel;
    const float envAmt = tv[FENVAMT] * (1.f - tv[FVELO] + tv[FVELO] * vel) * 7.f;

    int pos = 0;
    while (pos < n)
    {
        const int m = std::min (controlBlock, n - pos);

        // glide
        if (std::abs (pitchCur - pitchTarget) > 1.0e-6f)
        {
            if (glide <= .0005f) pitchCur = pitchTarget;
            else if (glMode == 0)
            {
                const float k = std::exp (-(float) m / (glide * .25f * (float) sr));
                pitchCur = pitchTarget + (pitchCur - pitchTarget) * k;
                if (std::abs (pitchCur - pitchTarget) < .001f) pitchCur = pitchTarget;
            }
            else
            {
                const float step = glideStep * (float) m;
                if (std::abs (pitchTarget - pitchCur) <= step) pitchCur = pitchTarget;
                else pitchCur += pitchTarget > pitchCur ? step : -step;
            }
        }

        // LFO (avanzano a passo di controllo)
        for (int k = 0; k < 2; ++k)
        {
            const double hz = lfoRate (tv, k, c.bpm) * lfoRateMul[k];
            float val = lfo[k].advance (hz * m * invSr, tv.i (k == 0 ? L1WAVE : L2WAVE));
            if (tv.b (k == 0 ? L1UNI : L2UNI)) val = val * .5f + .5f;
            lfoVal[k] = val;
        }

        // matrice di modulazione
        const float src[sCount] = { 0.f, lfoVal[0], lfoVal[1], moLast, fenv.out, aenv.out, vel, ((float) note - 60.f) / 60.f,
                                    c.wheel, c.aftertouch, c.bend, rnd * 2.f - 1.f, c.macro };
        float dst[dCount] {};
        for (int k = 0; k < 8; ++k)
        {
            const int s = tv.i ((TP) (M1SRC + 3 * k)), d = tv.i ((TP) (M1DST + 3 * k));
            if (s > 0 && s < sCount && d > 0 && d < dCount) dst[d] += src[s] * tv[(TP) (M1AMT + 3 * k)];
        }
        lfoRateMul[0] = std::exp2 (dst[dL1Rate] * 4.f);
        lfoRateMul[1] = std::exp2 (dst[dL2Rate] * 4.f);

        // altezze
        plPhase += tv[PLRATE] * m * invSr;
        if (plPhase >= 1.0) plPhase -= std::floor (plPhase);
        const float vibDepth = clampf (tv[PLAMT] + c.wheel * tv[PLWHEEL] + dst[dVib], 0.f, 2.f) * .5f;   // 1 = mezzo semitono
        const float vib = std::sin (twoPi * (float) plPhase) * vibDepth;
        const float base = pitchCur + 12.f * (float) tv.i (OCT) + c.bend * (float) tv.i (BEND) + c.tuneSemis + detune * .01f + vib + dst[dPitch] * 24.f;
        const double inc1 = noteToInc (base + 12.f * (float) (tv.i (O1OCT) - 2) + dst[dO1Pitch] * 24.f, sr);
        const double inc2 = noteToInc (base + 12.f * (float) (tv.i (O2OCT) - 2) + (float) tv.i (O2SEMI) + tv[O2FINE] * .01f + dst[dO2Pitch] * 24.f, sr);
        double incMo;
        if (tv.i (MOTRACK) == 0) incMo = noteToInc (base + tv[MOPITCH], sr) * std::exp2 (dst[dMoFreq] * 4.f);
        else incMo = std::min (.45, tv[MORATE] * std::exp2 (dst[dMoFreq] * 4.f) * invSr);

        const float w1 = tv[O1WAVE] + dst[dO1Wave], w2 = tv[O2WAVE] + dst[dO2Wave];
        const float l1 = clamp01 (tv[MIX1] + dst[dO1Lvl]), l2 = clamp01 (tv[MIX2] + dst[dO2Lvl]);
        const float lmo = clamp01 (tv[MIXMO] + dst[dMoLvl]), ln = clamp01 (tv[MIXNOISE] + dst[dNoise]) * .7f, lr = tv[MIXRING];
        const float fm = clampf (tv[FM] + dst[dFM], -1.f, 1.f), fmD = fm * std::abs (fm) * 3.f;
        const float drive = clamp01 (tv[DRIVE] + dst[dDrive]);
        const float mixGain = 1.f + drive * 7.f, makeup = 1.f / (1.f + drive * .9f);
        const float kt = tv[KEYTRK] * (pitchCur - 60.f) / 12.f;
        const float oct2 = std::log2 (tv[F2CUT]) + kt + dst[dF2Cut] * 6.f;
        const float oct1 = link ? oct2 + tv[FSPACE] + dst[dF1Cut] * 6.f : std::log2 (tv[F1CUT]) + kt + dst[dF1Cut] * 6.f;
        const float k1 = 4.4f * clamp01 (tv[F1RES] + dst[dF1Res]), k2 = 4.4f * clamp01 (tv[F2RES] + dst[dF2Res]);
        const float ampMod = std::max (0.f, 1.f + dst[dAmp]) * velGain * level;
        const float pan = clampf (tv[PAN] + panPos * tv[SPREAD] + dst[dPan], -1.f, 1.f);
        const float pl = std::cos ((pan + 1.f) * .25f * 3.14159265f) * 1.41421356f;
        const float pr = std::sin ((pan + 1.f) * .25f * 3.14159265f) * 1.41421356f;
        const float maxOct = std::log2 ((float) sr * .45f);
        const float piOverSr = 3.14159265f / (float) sr;

        for (int i = 0; i < m; ++i)
        {
            const float mov = mo.tick (incMo, moShape);
            const double pmul = moPitchOct > 0.f ? std::exp2 (mov * moPitchOct) : 1.0;
            double i1 = inc1 * pmul, i2 = inc2 * pmul;
            if (fmD > 0.f)      i2 *= std::max (0.f, 1.f + fmD * prevO1);
            else if (fmD < 0.f) i1 *= std::max (0.f, 1.f - fmD * prevO2);
            i1 = std::min (i1, .45); i2 = std::min (i2, .45);

            bool wr = false; double fr = 0;
            const float a = o1.tick (i1, w1 + mov * moWave, wr, fr);
            if (sync && wr) o2.phase = fr * i2;
            bool wr2 = false; double fr2 = 0;
            const float b = o2.tick (i2, w2 + mov * moWave, wr2, fr2);
            prevO1 = a; prevO2 = b;

            // mixer saturante con offset (clip asimmetrico) e blocco della continua
            const float sum = l1 * a + l2 * b + lmo * mov + lr * a * b + ln * noise.next();
            float y = (std::tanh (mixGain * sum * .55f + offset) - offTanh) * makeup;
            const float yd = y - dcX + .9995f * dcY; dcX = y; dcY = yd; y = yd + noise.next() * 1.0e-5f;   // rumore di fondo: innesca l'autooscillazione

            // filtri
            const float fe = fenv.tick(), ae = aenv.tick();
            const float em = envAmt * fe + mov * moCut;
            const float g1 = std::tan (std::exp2 (clampf (oct1 + em, 3.3f, maxOct)) * piOverSr);
            const float g2 = std::tan (std::exp2 (clampf (oct2 + em, 3.3f, maxOct)) * piOverSr);
            float outL, outR;
            switch (route)
            {
                case 1:  outL = outR = .5f * (f1.process (y, g1, k1, hp1) + f2.process (y, g2, k2, false)); break;
                case 2:  outL = f1.process (y, g1, k1, hp1); outR = f2.process (y, g2, k2, false); break;
                default: outL = outR = f2.process (f1.process (y, g1, k1, hp1), g2, k2, false); break;
            }
            const float amp = ae * ampMod * (1.f - moAmp + moAmp * (mov * .5f + .5f));
            L[pos + i] += outL * amp * pl;
            R[pos + i] += outR * amp * pr;
            moLast = mov;
        }
        pos += m;
    }
    if (! aenv.active()) { active = false; gate = false; }
}

// ================================================================================================ arpeggiatore
namespace
{
constexpr double arpBeats[6] = { 1, .5, 1.0 / 3.0, .25, 1.0 / 6.0, .125 };
}

void Arp::reset()
{
    held.clear(); down.clear(); seq.clear();
    step = -1; dir = 1; playing = -1;
    samplesToNext = 0; samplesToOff = -1; lastStepIndex = -1;
}

void Arp::noteOn (int note, float vel, bool latch)
{
    if (latch && down.empty()) held.clear();           // nuovo accordo dopo aver lasciato tutti i tasti
    down.push_back (note);
    for (auto& h : held) if (h.first == note) return;
    held.push_back ({ note, vel });
    lastVel = vel;
}

void Arp::noteOff (int note, bool latch)
{
    down.erase (std::remove (down.begin(), down.end(), note), down.end());
    if (! latch) held.erase (std::remove_if (held.begin(), held.end(), [note] (auto& h) { return h.first == note; }), held.end());
}

void Arp::latchReleased()
{
    held.erase (std::remove_if (held.begin(), held.end(), [this] (auto& h)
                { return std::find (down.begin(), down.end(), h.first) == down.end(); }), held.end());
}

void Arp::buildSequence (const GlobalValues& gv)
{
    std::vector<int> base;
    for (auto& h : held) base.push_back (h.first);
    if (gv.i (ARPMODE) != 4) std::sort (base.begin(), base.end());
    seq.clear();
    const int oct = juce::jlimit (1, 4, gv.i (ARPOCT));
    for (int o = 0; o < oct; ++o)
        for (int nn : base) if (nn + 12 * o < 128) seq.push_back (nn + 12 * o);
}

int Arp::nextNote (const GlobalValues& gv)
{
    buildSequence (gv);
    if (seq.empty()) return -1;
    const int sz = (int) seq.size(), mode = gv.i (ARPMODE);
    switch (mode)
    {
        case 1:  step = (step - 1 + sz * 4) % sz; if (step < 0) step = sz - 1; break;           // DOWN
        case 2:                                                                                  // UP-DOWN
            if (sz == 1) { step = 0; break; }
            step += dir;
            if (step >= sz) { dir = -1; step = sz - 2; }
            if (step < 0)   { dir = 1;  step = std::min (1, sz - 1); }
            break;
        case 3:  rnd = rnd * 1664525u + 1013904223u; step = (int) ((rnd >> 8) % (uint32_t) sz); break;   // RANDOM
        default: step = (step + 1) % sz; break;                                                  // UP / ORDER
    }
    step = juce::jlimit (0, sz - 1, step);
    return seq[(size_t) step];
}

void Arp::stopAll (std::vector<Ev>& out, int offset)
{
    if (playing >= 0) out.push_back ({ offset, playing, 0.f });
    playing = -1;
    samplesToOff = -1;
}

void Arp::process (int n, const GlobalValues& gv, double sr, double bpm, double ppqStart, bool hostPlaying, std::vector<Ev>& out)
{
    if (held.empty())
    {
        stopAll (out, 0);
        step = -1; dir = 1; samplesToNext = 0; lastStepIndex = -1;
        return;
    }
    const double beats = arpBeats[juce::jlimit (0, 5, gv.i (ARPRATE))];
    const double stepSamples = std::max (1.0, beats * 60.0 / std::max (20.0, bpm) * sr);
    const double gateSamples = std::max (1.0, stepSamples * juce::jlimit (.05, 1.0, (double) gv[ARPGATE]));

    std::vector<int> triggers;
    if (hostPlaying)
    {
        const double pps = bpm / 60.0 / sr;
        double idx = std::floor (ppqStart / beats);
        if ((long long) idx != (long long) lastStepIndex && playing < 0) { triggers.push_back (0); lastStepIndex = idx; }
        double nextPpq = (idx + 1) * beats;
        int off = (int) std::ceil ((nextPpq - ppqStart) / pps);
        while (off < n)
        {
            triggers.push_back (std::max (0, off));
            lastStepIndex = idx + 1;
            idx += 1; nextPpq += beats;
            off = (int) std::ceil ((nextPpq - ppqStart) / pps);
        }
    }
    else
    {
        while (samplesToNext < n)
        {
            triggers.push_back (std::max (0, (int) samplesToNext));
            samplesToNext += stepSamples;
        }
        samplesToNext -= n;
    }

    for (int t : triggers)
    {
        if (playing >= 0)
        {
            const int offAt = (samplesToOff >= 0 && samplesToOff < t) ? (int) samplesToOff : t;
            out.push_back ({ offAt, playing, 0.f });
            playing = -1;
        }
        const int nn = nextNote (gv);
        if (nn < 0) continue;
        float v = lastVel;
        for (auto& h : held) if (h.first % 12 == nn % 12) { v = h.second; break; }
        out.push_back ({ t, nn, v });
        playing = nn;
        samplesToOff = t + gateSamples;
    }
    if (playing >= 0 && samplesToOff >= 0 && samplesToOff < n)
    {
        out.push_back ({ (int) samplesToOff, playing, 0.f });
        playing = -1;
        samplesToOff = -1;
    }
    else if (samplesToOff >= 0) samplesToOff -= n;
}

// ================================================================================================ sintetizzatore
void Synth::prepare (double sampleRate, int maxBlock)
{
    sr = sampleRate;
    ctx.sr = sr;
    busL.assign ((size_t) std::max (maxBlock, 64), 0.f);
    busR.assign ((size_t) std::max (maxBlock, 64), 0.f);
    for (int i = 0; i < numVoices; ++i) { voices[i].kill(); voices[i].index = i; }
    delay.prepare (sr);
    arp.reset();
    for (auto& t : ts) { t.stack.clear(); t.mode = -1; t.lastPitch = -1; }
    std::fill (std::begin (noteMask), std::end (noteMask), 0);
    hpPrevL = hpPrevR = hpL = hpR = 0;
    arpEvents.reserve (64);
}

int Synth::activeVoices() const
{
    int n = 0;
    for (auto& v : voices) if (v.active) ++n;
    return n;
}

void Synth::allNotesOff()
{
    for (auto& v : voices) v.release();
    for (auto& t : ts) t.stack.clear();
    arp.reset();
}

Voice* Synth::findVoice (uint32_t exclude)
{
    Voice* best = nullptr;
    for (auto& v : voices)
        if (! (exclude & (1u << v.index)) && ! v.active) return &v;
    float lowest = 2.f;
    for (auto& v : voices)
        if (! (exclude & (1u << v.index)) && ! v.gate && v.ampLevel() < lowest) { lowest = v.ampLevel(); best = &v; }
    if (best != nullptr) return best;
    uint64_t oldest = ~0ull;
    for (auto& v : voices)
        if (! (exclude & (1u << v.index)) && v.age < oldest) { oldest = v.age; best = &v; }
    return best;
}

void Synth::timbreNoteOn (int tm, int note, float vel, const TimbreValues& tv)
{
    auto& st = ts[tm];
    st.stack.erase (std::remove_if (st.stack.begin(), st.stack.end(), [note] (auto& p) { return p.first == note; }), st.stack.end());
    st.stack.push_back ({ note, vel });
    const int mode = tv.i (VMODE);
    const float from = st.lastPitch;

    if (mode == 0)   // POLY
    {
        if (auto* v = findVoice (0))
        {
            static const float panTable[8] = { -.15f, .15f, -.45f, .45f, -.75f, .75f, -1.f, 1.f };
            v->detune = 0; v->panPos = panTable[v->index];
            v->start (tm, note, vel, tv, ctx, lfoGlobal[tm][0], lfoGlobal[tm][1], from);
            v->age = ++counter;
        }
        st.lastPitch = (float) note;
        return;
    }

    // MONO / UNISON
    const int want = mode == 1 ? 1 : juce::jlimit (2, numVoices, tv.i (UNIVOX));
    std::vector<Voice*> group;
    uint32_t mask = 0;
    bool anyGate = false;
    for (auto& v : voices)
        if (v.active && v.timbre == tm) { group.push_back (&v); mask |= 1u << v.index; anyGate = anyGate || v.gate; }

    if (! group.empty() && anyGate && tv.i (MTRIG) == 0 && (int) group.size() == want)
    {
        for (auto* v : group) v->moveTo (note, vel, tv, false);
    }
    else
    {
        while ((int) group.size() > want) { group.back()->kill(); group.pop_back(); }
        while ((int) group.size() < want)
        {
            auto* v = findVoice (mask);
            if (v == nullptr) break;
            group.push_back (v); mask |= 1u << v->index;
        }
        const int n = (int) group.size();
        for (int k = 0; k < n; ++k)
        {
            auto* v = group[(size_t) k];
            const float spreadPos = n > 1 ? (float) k / (float) (n - 1) * 2.f - 1.f : 0.f;
            v->detune = spreadPos * tv[UNIDET] * 30.f;
            v->panPos = spreadPos;
            const float glideFrom = v->active ? v->currentPitch() : from;
            v->start (tm, note, vel, tv, ctx, lfoGlobal[tm][0], lfoGlobal[tm][1], glideFrom);
            v->age = ++counter;
        }
    }
    st.lastPitch = (float) note;
}

void Synth::timbreNoteOff (int tm, int note, const TimbreValues& tv)
{
    auto& st = ts[tm];
    st.stack.erase (std::remove_if (st.stack.begin(), st.stack.end(), [note] (auto& p) { return p.first == note; }), st.stack.end());
    const int mode = tv.i (VMODE);
    if (mode == 0)
    {
        for (auto& v : voices)
            if (v.active && v.gate && v.timbre == tm && v.note == note)
            {
                if (sustainPedal) v.sustained = true;
                else v.release();
            }
        return;
    }
    bool playingThis = false;
    for (auto& v : voices) if (v.active && v.gate && v.timbre == tm && v.note == note) playingThis = true;
    if (! playingThis) return;
    if (! st.stack.empty())
    {
        const auto top = st.stack.back();
        for (auto& v : voices)
            if (v.active && v.timbre == tm) v.moveTo (top.first, top.second, tv, tv.i (MTRIG) == 1);
        st.lastPitch = (float) top.first;
    }
    else
        for (auto& v : voices)
            if (v.active && v.timbre == tm && v.gate)
            {
                if (sustainPedal) v.sustained = true;
                else v.release();
            }
}

void Synth::noteOn (int note, float vel, const TimbreValues (&t)[2], const GlobalValues& gv)
{
    uint8_t mask = 1;
    switch (gv.i (KEYMODE))
    {
        case 1: mask = 2; break;
        case 2: mask = 3; break;
        case 3: mask = note >= gv.i (SPLIT) ? 1 : 2; break;
        default: mask = 1; break;
    }
    noteMask[note & 127] = mask;
    for (int tm = 0; tm < 2; ++tm)
        if (mask & (1 << tm)) timbreNoteOn (tm, note, vel, t[tm]);
}

void Synth::noteOff (int note, const TimbreValues (&t)[2])
{
    const uint8_t mask = noteMask[note & 127];
    noteMask[note & 127] = 0;
    for (int tm = 0; tm < 2; ++tm)
        if (mask & (1 << tm)) timbreNoteOff (tm, note, t[tm]);
}

void Synth::sustainOff()
{
    for (auto& v : voices)
        if (v.sustained) { v.sustained = false; v.release(); }
}

void Synth::handleMidi (const juce::MidiMessage& m, const TimbreValues (&t)[2], const GlobalValues& gv)
{
    if (m.isNoteOn()) noteOn (m.getNoteNumber(), m.getFloatVelocity(), t, gv);
    else if (m.isNoteOff()) noteOff (m.getNoteNumber(), t);
    else if (m.isPitchWheel()) ctx.bend = juce::jlimit (-1.f, 1.f, (float) (m.getPitchWheelValue() - 8192) / 8191.f);
    else if (m.isChannelPressure()) ctx.aftertouch = (float) m.getChannelPressureValue() / 127.f;
    else if (m.isAftertouch()) ctx.aftertouch = std::max (ctx.aftertouch, (float) m.getAfterTouchValue() / 127.f);
    else if (m.isController())
    {
        const int cc = m.getControllerNumber(), val = m.getControllerValue();
        if (cc == 1) ctx.wheel = (float) val / 127.f;
        else if (cc == 64) { const bool on = val >= 64; if (sustainPedal && ! on) { sustainPedal = false; sustainOff(); } sustainPedal = on; }
        else if (cc == 120 || cc == 123) allNotesOff();
    }
    else if (m.isAllNotesOff() || m.isAllSoundOff()) allNotesOff();
}

void Synth::renderVoices (int start, int n, const TimbreValues (&t)[2])
{
    if (n <= 0) return;
    for (auto& v : voices)
        if (v.active) v.render (busL.data() + start, busR.data() + start, n, t[v.timbre], ctx);
    for (int tm = 0; tm < 2; ++tm)
        for (int k = 0; k < 2; ++k)
        {
            lfoGlobal[tm][k] += lfoRate (t[tm], k, ctx.bpm) * n / sr;
            lfoGlobal[tm][k] -= std::floor (lfoGlobal[tm][k]);
        }
}

double Synth::lfoRateHz (const TimbreValues& tv, int which) const { return lfoRate (tv, which, ctx.bpm); }

void Synth::process (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi, const TimbreValues (&t)[2], const GlobalValues& gv,
                     double hostBpm, double ppq, bool hostPlaying)
{
    const int n = buffer.getNumSamples();
    if ((int) busL.size() < n) { busL.resize ((size_t) n); busR.resize ((size_t) n); }
    std::fill (busL.begin(), busL.begin() + n, 0.f);
    std::fill (busR.begin(), busR.begin() + n, 0.f);

    ctx.sr = sr;
    ctx.bpm = hostBpm > 0 ? hostBpm : (double) gv[BPM];
    ctx.macro = gv[MACRO];
    ctx.tuneSemis = gv[TUNE] * .01f;

    // cambio di modo voce: chiude le voci del timbro
    for (int tm = 0; tm < 2; ++tm)
        if (ts[tm].mode != t[tm].i (VMODE))
        {
            if (ts[tm].mode >= 0) for (auto& v : voices) if (v.active && v.timbre == tm) v.release();
            ts[tm].mode = t[tm].i (VMODE);
            ts[tm].stack.clear();
        }
    for (auto& v : voices) if (v.active) v.updateEnvelopes (t[v.timbre], sr);

    // eventi: MIDI diretto o passato dall'arpeggiatore
    const bool arpOn = gv.b (ARPON), latch = gv.b (ARPLATCH);
    std::vector<std::pair<int, juce::MidiMessage>> events;
    events.reserve (64);
    arpEvents.clear();
    if (arpWasOn && ! arpOn) { arp.stopAll (arpEvents, 0); arp.reset(); }
    if (latchWasOn && ! latch) arp.latchReleased();
    latchWasOn = latch;

    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        const int off = juce::jlimit (0, n - 1, meta.samplePosition);
        if (arpOn && (m.isNoteOn() || m.isNoteOff()))
        {
            if (! arpWasOn) { for (auto& v : voices) v.release(); }
            if (m.isNoteOn()) arp.noteOn (m.getNoteNumber(), m.getFloatVelocity(), latch);
            else arp.noteOff (m.getNoteNumber(), latch);
        }
        else events.push_back ({ off, m });
    }
    if (arpOn) arp.process (n, gv, sr, ctx.bpm, ppq, hostPlaying, arpEvents);
    arpWasOn = arpOn;
    for (auto& e : arpEvents)
    {
        const int off = juce::jlimit (0, n - 1, e.offset);
        events.push_back ({ off, e.vel > 0.f ? juce::MidiMessage::noteOn (1, e.note, juce::jlimit (0.01f, 1.f, e.vel))
                                             : juce::MidiMessage::noteOff (1, e.note) });
    }
    std::stable_sort (events.begin(), events.end(), [] (auto& a, auto& b)
    {
        if (a.first != b.first) return a.first < b.first;
        return a.second.isNoteOff() && ! b.second.isNoteOff();     // prima i note-off
    });

    int pos = 0;
    for (auto& e : events)
    {
        renderVoices (pos, e.first - pos, t);
        pos = std::max (pos, e.first);
        handleMidi (e.second, t, gv);
    }
    renderVoices (pos, n - pos, t);

    // delay, taglio dei bassi, volume
    delay.process (busL.data(), busR.data(), n, gv, ctx.bpm);
    const bool lowCut = gv.b (LOWCUT);
    const float hpA = 1.f / (1.f + 2.f * 3.14159265f * 80.f / (float) sr);
    const float targetGain = gv[MASTER] <= -59.9f ? 0.f : juce::Decibels::decibelsToGain (gv[MASTER]);
    float peak = 0;
    auto* outL = buffer.getWritePointer (0);
    auto* outR = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    for (int i = 0; i < n; ++i)
    {
        float l = busL[(size_t) i], r = busR[(size_t) i];
        if (lowCut)
        {
            hpL = hpA * (hpL + l - hpPrevL); hpPrevL = l; l = hpL;
            hpR = hpA * (hpR + r - hpPrevR); hpPrevR = r; r = hpR;
        }
        else { hpPrevL = l; hpPrevR = r; hpL = hpR = 0; }
        gainSm += (targetGain - gainSm) * .002f;
        l *= gainSm; r *= gainSm;
        if (! std::isfinite (l)) l = 0;
        if (! std::isfinite (r)) r = 0;
        peak = std::max (peak, std::max (std::abs (l), std::abs (r)));
        if (outR != nullptr) { outL[i] = l; outR[i] = r; }
        else outL[i] = .5f * (l + r);
    }
    outLevel.store (std::max (peak, outLevel.load() * .8f));
}
}
