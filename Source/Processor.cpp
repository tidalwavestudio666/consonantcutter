#include "Processor.h"

bool ConsonantCutterProcessor::loadAudioFile(const juce::File& file, juce::AudioBuffer<float>& out, double& sr, juce::String& err)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> r(fm.createReaderFor(file));
    if (!r) { err = "Unsupported audio file."; return false; }

    sr = r->sampleRate;
    out.setSize((int)r->numChannels, (int)r->lengthInSamples);
    out.clear();
    r->read(&out, 0, out.getNumSamples(), 0, true, true);
    return true;
}

bool ConsonantCutterProcessor::saveWav24(const juce::File& file, const juce::AudioBuffer<float>& audio, double sr, juce::String& err)
{
    juce::WavAudioFormat wav;
    file.deleteFile();

    std::unique_ptr<juce::FileOutputStream> os(file.createOutputStream());
    if (!os) { err = "Could not create output file."; return false; }

    std::unique_ptr<juce::AudioFormatWriter> w(wav.createWriterFor(os.get(), sr,
        (unsigned int)audio.getNumChannels(), 24, {}, 0));
    if (!w) { err = "Could not create WAV writer."; return false; }

    os.release(); // writer owns stream now
    return w->writeFromAudioSampleBuffer(audio, 0, audio.getNumSamples());
}

bool ConsonantCutterProcessor::process(const juce::AudioBuffer<float>& in, double sr, const ConsonantCutterParams& p,
                                       juce::AudioBuffer<float>& out, juce::Array<Event>& events, juce::String& err)
{
    err.clear();
    events.clear();

    const int numCh = in.getNumChannels();
    const int n = in.getNumSamples();
    if (n <= 0 || numCh <= 0) { err = "Empty audio."; return false; }

    juce::dsp::IIR::Filter<float> hp;
    auto coeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass(sr, juce::jlimit(2000.0f, 12000.0f, p.hpfHz));
    hp.coefficients = coeffs;
    hp.reset();

    const float envThr = dbToGain(p.thresholdDb);
    const float aMs = 1.5f, rMs = 25.0f;
    const float alphaA = std::exp(-1.0f / (float)msToSamps(aMs, sr));
    const float alphaR = std::exp(-1.0f / (float)msToSamps(rMs, sr));
    float env = 0.0f;

    const int minEvent = msToSamps(p.minEventMs, sr);
    const int maxEvent = msToSamps(p.maxEventMs, sr);
    const int maxCut   = msToSamps(p.maxCutMs, sr);
    const int xfade    = juce::jlimit(1, maxEvent / 4, msToSamps(p.xfadeMs, sr));
    const int baseCutLen = juce::jlimit(0, maxEvent - 2, (int)std::lround((float)maxCut * juce::jlimit(0.0f, 1.0f, p.cutAmount)));

    const float eventGain = dbToGain(p.eventGainDb);

    int i = 0;
    while (i < n)
    {
        float mono = 0.0f;
        for (int ch = 0; ch < numCh; ++ch) mono += in.getSample(ch, i);
        mono *= 1.0f / (float)juce::jmax(1, numCh);

        float hpOut = hp.processSample(mono);
        float a = std::abs(hpOut);

        env = (a > env) ? (alphaA * env + (1.0f - alphaA) * a)
                        : (alphaR * env + (1.0f - alphaR) * a);

        if (env >= envThr)
        {
            const int start = i;
            int len = juce::jmin(maxEvent, n - start);
            if (len >= minEvent)
            {
                Event e;
                e.start = start;
                e.length = len;
                e.cutLen = juce::jmin(baseCutLen, len - 2);
                events.add(e);
                i += len;
                continue;
            }
        }
        ++i;
    }

    std::vector<std::vector<float>> outCh((size_t)numCh);
    for (int ch = 0; ch < numCh; ++ch) outCh[(size_t)ch].reserve((size_t)n);

    int cursor = 0;
    for (auto& e : events)
    {
        if (e.start < cursor) continue; // MVP: skip overlaps
        const int start = e.start;
        const int len = e.length;
        const int cLen = e.cutLen;

        // copy pre-event
        for (int ch = 0; ch < numCh; ++ch)
        {
            auto& dst = outCh[(size_t)ch];
            const float* src = in.getReadPointer(ch);
            dst.insert(dst.end(), src + cursor, src + start);
        }

        const int remaining = len - cLen;
        const int keepA = remaining / 2;
        const int cutStart = keepA;
        const int cutEnd = keepA + cLen;

        const int preAStart = start;
        const int preAEnd   = start + cutStart;
        const int postBStart= start + cutEnd;
        const int postBEnd  = start + len;

        for (int ch = 0; ch < numCh; ++ch)
        {
            auto& dst = outCh[(size_t)ch];
            const float* src = in.getReadPointer(ch);

            // copy A
            for (int s = preAStart; s < preAEnd; ++s)
                dst.push_back(src[s] * eventGain);

            // crossfade
            const int aX0 = juce::jmax(preAStart, preAEnd - xfade);
            const int bX0 = postBStart;
            const int xN  = juce::jmin(xfade, juce::jmin(preAEnd - aX0, postBEnd - bX0));

            if (xN > 0)
            {
                dst.resize(dst.size() - (size_t)xN);
                for (int k = 0; k < xN; ++k)
                {
                    const float t = (float)k / (float)juce::jmax(1, xN - 1);
                    const float aS = src[aX0 + k];
                    const float bS = src[bX0 + k];
                    dst.push_back(((1.0f - t) * aS + t * bS) * eventGain);
                }
                for (int s = bX0 + xN; s < postBEnd; ++s)
                    dst.push_back(src[s] * eventGain);
            }
            else
            {
                for (int s = postBStart; s < postBEnd; ++s)
                    dst.push_back(src[s] * eventGain);
            }
        }

        cursor = start + len;
    }

    // tail
    for (int ch = 0; ch < numCh; ++ch)
    {
        auto& dst = outCh[(size_t)ch];
        const float* src = in.getReadPointer(ch);
        dst.insert(dst.end(), src + cursor, src + n);
    }

    const int outN = (int)outCh[0].size();
    out.setSize(numCh, outN);
    for (int ch = 0; ch < numCh; ++ch)
        std::memcpy(out.getWritePointer(ch), outCh[(size_t)ch].data(), (size_t)outN * sizeof(float));

    return true;
}
