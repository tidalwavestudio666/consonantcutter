#include <JuceHeader.h>
#include "Processor.h"

class MainComponent : public juce::Component
{
public:
    MainComponent()
    {
        auto addKnob = [&](juce::Slider& s, juce::Label& l, const juce::String& name,
                           double min, double max, double step, double init, bool skew = false)
        {
            s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 84, 18);
            s.setRange(min, max, step);
            if (skew) s.setSkewFactorFromMidPoint(6000.0);
            s.setValue(init);
            l.setText(name, juce::dontSendNotification);
            l.setJustificationType(juce::Justification::centred);
            addAndMakeVisible(s);
            addAndMakeVisible(l);
        };

        addKnob(threshold, lThreshold, "Threshold (dB)", -60.0, -10.0, 0.1, params.thresholdDb);
        addKnob(hpf, lHpf, "HPF (Hz)", 2000.0, 12000.0, 1.0, params.hpfHz, true);
        addKnob(maxEvent, lMaxEvent, "Max Event (ms)", 20.0, 200.0, 1.0, params.maxEventMs);
        addKnob(maxCut, lMaxCut, "Max Cut (ms)", 0.0, 80.0, 1.0, params.maxCutMs);
        addKnob(cutAmount, lCutAmount, "Cut Amount", 0.0, 1.0, 0.001, params.cutAmount);
        addKnob(xfade, lXfade, "Xfade (ms)", 0.5, 15.0, 0.1, params.xfadeMs);
        addKnob(eventGain, lEventGain, "Event Gain (dB)", -24.0, 0.0, 0.1, params.eventGainDb);

        openButton.setButtonText("Open...");
        exportButton.setButtonText("Process + Export...");
        exportButton.setEnabled(false);

        addAndMakeVisible(openButton);
        addAndMakeVisible(exportButton);
        addAndMakeVisible(status);

        openButton.onClick = [this]{ openFile(); };
        exportButton.onClick = [this]{ processAndExport(); };

        status.setJustificationType(juce::Justification::left);
        status.setText("Load a WAV/AIFF.", juce::dontSendNotification);

        setSize(720, 360);
    }

    void paint(juce::Graphics& g) override { g.fillAll(juce::Colours::darkgrey.darker(0.7f)); }

    void resized() override
    {
        auto r = getLocalBounds().reduced(14);
        auto top = r.removeFromTop(260);
        auto row1 = top.removeFromTop(120);
        auto row2 = top.removeFromTop(120);

        auto place = [](juce::Rectangle<int> a, juce::Label& l, juce::Slider& s)
        { l.setBounds(a.removeFromTop(20)); s.setBounds(a.reduced(6)); };

        int w = row1.getWidth() / 4;
        place(row1.removeFromLeft(w), lThreshold, threshold);
        place(row1.removeFromLeft(w), lHpf, hpf);
        place(row1.removeFromLeft(w), lMaxEvent, maxEvent);
        place(row1.removeFromLeft(w), lMaxCut, maxCut);

        w = row2.getWidth() / 3;
        place(row2.removeFromLeft(w), lCutAmount, cutAmount);
        place(row2.removeFromLeft(w), lXfade, xfade);
        place(row2.removeFromLeft(w), lEventGain, eventGain);

        auto buttons = r.removeFromTop(40);
        openButton.setBounds(buttons.removeFromLeft(140));
        buttons.removeFromLeft(10);
        exportButton.setBounds(buttons.removeFromLeft(200));

        r.removeFromTop(8);
        status.setBounds(r);
    }

private:
    ConsonantCutterParams params;
    juce::File inputFile;
    juce::AudioBuffer<float> inputAudio;
    double sampleRate = 48000.0;

    juce::Slider threshold, hpf, maxEvent, maxCut, cutAmount, xfade, eventGain;
    juce::Label lThreshold, lHpf, lMaxEvent, lMaxCut, lCutAmount, lXfade, lEventGain;
    juce::TextButton openButton, exportButton;
    juce::Label status;

    void syncParams()
    {
        params.thresholdDb = (float)threshold.getValue();
        params.hpfHz = (float)hpf.getValue();
        params.maxEventMs = (float)maxEvent.getValue();
        params.maxCutMs = (float)maxCut.getValue();
        params.cutAmount = (float)cutAmount.getValue();
        params.xfadeMs = (float)xfade.getValue();
        params.eventGainDb = (float)eventGain.getValue();
    }

    void openFile()
    {
        juce::FileChooser fc("Open WAV/AIFF...", {}, "*.wav;*.aif;*.aiff");
        if (!fc.browseForFileToOpen()) return;

        juce::String err;
        if (!ConsonantCutterProcessor::loadAudioFile(fc.getResult(), inputAudio, sampleRate, err))
        {
            status.setText("Load failed: " + err, juce::dontSendNotification);
            exportButton.setEnabled(false);
            return;
        }

        inputFile = fc.getResult();
        exportButton.setEnabled(true);
        status.setText("Loaded: " + inputFile.getFileName(), juce::dontSendNotification);
    }

    void processAndExport()
    {
        if (!inputFile.existsAsFile()) return;
        syncParams();

        juce::FileChooser fc("Export edited WAV...", inputFile.getSiblingFile(inputFile.getFileNameWithoutExtension() + "_CC.wav"), "*.wav");
        if (!fc.browseForFileToSave(true)) return;

        juce::AudioBuffer<float> outAudio;
        juce::Array<ConsonantCutterProcessor::Event> events;
        juce::String err;

        if (!ConsonantCutterProcessor::process(inputAudio, sampleRate, params, outAudio, events, err))
        {
            status.setText("Process failed: " + err, juce::dontSendNotification);
            return;
        }

        if (!ConsonantCutterProcessor::saveWav24(fc.getResult(), outAudio, sampleRate, err))
        {
            status.setText("Export failed: " + err, juce::dontSendNotification);
            return;
        }

        status.setText("Exported | events: " + juce::String(events.size()) + " | new samples: " + juce::String(outAudio.getNumSamples()),
                       juce::dontSendNotification);
    }
};

class MainWindow : public juce::DocumentWindow
{
public:
    MainWindow() : DocumentWindow("ConsonantCutter", juce::Colours::black, DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar(true);
        setContentOwned(new MainComponent(), true);
        centreWithSize(getWidth(), getHeight());
        setVisible(true);
    }
    void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
};

class App : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "ConsonantCutter"; }
    const juce::String getApplicationVersion() override { return "0.1.0"; }
    void initialise(const juce::String&) override { mainWindow.reset(new MainWindow()); }
    void shutdown() override { mainWindow = nullptr; }
private:
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION(App)
