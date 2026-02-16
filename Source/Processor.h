#pragma once
#include <JuceHeader.h>

struct ConsonantCutterParams
{
    float thresholdDb = -32.0f;
    float hpfHz = 6000.0f;

    float minEventMs = 18.0f;
    float maxEventMs = 90.0f;

    float maxCutMs = 28.0f;
    float cutAmount = 0.55f; // 0..1 fraction of maxCut
    float xfadeMs = 4.0f;

    float eventGainDb = -6.0f;
};

struct ConsonantCutterProcessor
{
    struct Event { int start = 0; int length = 0; int cutLen = 0; };

    static bool loadAudioFile(const juce::File& file, juce::AudioBuffer<float>& out, double& sr, juce::String& err);
    static bool saveWav24(const juce::File& file, const juce::AudioBuffer<float>& audio, double sr, juce::String& err);

    static bool process(const juce::AudioBuffer<float>& in, double sr, const ConsonantCutterParams& p,
                        juce::AudioBuffer<float>& out, juce::Array<Event>& events, juce::String& err);

private:
    static inline float dbToGain(float db) { return juce::Decibels::decibelsToGain(db); }
    static inline int msToSamps(float ms, double sr) { return juce::jmax(1, (int)std::lround(ms * 0.001 * sr)); }
};
