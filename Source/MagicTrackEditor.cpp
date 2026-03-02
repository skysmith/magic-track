#include "MagicTrackEditor.h"

#include <thread>

namespace magictrack
{
MagicTrackEditor::MagicTrackEditor(MagicTrackAudioProcessor& p)
    : AudioProcessorEditor(&p),
      processor(p)
{
    setSize(920, 420);

    commandBox.setTextToShowWhenEmpty("Type: brighter but not harsh", juce::Colours::grey);
    addAndMakeVisible(commandBox);

    applyButton.onClick = [this] { submitCommand(); };
    addAndMakeVisible(applyButton);

    undoButton.onClick = [this] {
        processor.undoLast();
        telemetryLabel.setText(processor.getTelemetry(), juce::dontSendNotification);
    };
    addAndMakeVisible(undoButton);

    storeAButton.onClick = [this] { processor.storeSlotA(); };
    addAndMakeVisible(storeAButton);

    recallAButton.onClick = [this] {
        processor.recallSlotA();
        telemetryLabel.setText(processor.getTelemetry(), juce::dontSendNotification);
    };
    addAndMakeVisible(recallAButton);

    storeBButton.onClick = [this] { processor.storeSlotB(); };
    addAndMakeVisible(storeBButton);

    recallBButton.onClick = [this] {
        processor.recallSlotB();
        telemetryLabel.setText(processor.getTelemetry(), juce::dontSendNotification);
    };
    addAndMakeVisible(recallBButton);

    llmFallbackToggle.setToggleState(true, juce::dontSendNotification);
    addAndMakeVisible(llmFallbackToggle);

    sidecarLabel.setText("Sidecar URL", juce::dontSendNotification);
    sidecarLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(sidecarLabel);

    sidecarUrlBox.setText("http://127.0.0.1:8787/interpret", juce::dontSendNotification);
    addAndMakeVisible(sidecarUrlBox);

    profileLabel.setText("Profile", juce::dontSendNotification);
    profileLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(profileLabel);

    profileBox.addItem("General", 1);
    profileBox.addItem("Guitar", 2);
    profileBox.addItem("Vocal", 3);
    profileBox.addItem("Upright Bass", 4);
    addAndMakeVisible(profileBox);

    profileAttachment = std::make_unique<ComboBoxAttachment>(
        processor.getValueTreeState(),
        "profile",
        profileBox);

    telemetryLabel.setText(processor.getTelemetry(), juce::dontSendNotification);
    telemetryLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(telemetryLabel);

    sliderAttachments.reserve(kMacroCount);

    for (int i = 0; i < kMacroCount; ++i)
    {
        auto& slider = sliders[static_cast<size_t>(i)];
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 18);
        addAndMakeVisible(slider);

        auto& label = sliderLabels[static_cast<size_t>(i)];
        label.setText(kMacroNames[static_cast<size_t>(i)], juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(label);

        sliderAttachments.push_back(std::make_unique<SliderAttachment>(
            processor.getValueTreeState(),
            juce::String(kMacroNames[static_cast<size_t>(i)]),
            slider));
    }

    commandBox.onReturnKey = [this] { submitCommand(); };

    startTimerHz(10);
}

MagicTrackEditor::~MagicTrackEditor()
{
    stopTimer();
}

void MagicTrackEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1e1f23));
    g.setColour(juce::Colour(0xfff2f2f2));
    g.setFont(18.0f);
    g.drawText("MagicTrack", 16, 10, 220, 28, juce::Justification::centredLeft);
}

void MagicTrackEditor::resized()
{
    auto bounds = getLocalBounds().reduced(12);

    auto top = bounds.removeFromTop(34).reduced(0, 1);
    commandBox.setBounds(top.removeFromLeft(350));
    applyButton.setBounds(top.removeFromLeft(74).reduced(2, 0));
    undoButton.setBounds(top.removeFromLeft(92).reduced(2, 0));
    profileLabel.setBounds(top.removeFromLeft(52));
    profileBox.setBounds(top.removeFromLeft(120).reduced(2, 0));
    storeAButton.setBounds(top.removeFromLeft(72).reduced(2, 0));
    recallAButton.setBounds(top.removeFromLeft(72).reduced(2, 0));
    storeBButton.setBounds(top.removeFromLeft(72).reduced(2, 0));
    recallBButton.setBounds(top.removeFromLeft(72).reduced(2, 0));

    bounds.removeFromTop(4);
    auto sidecarRow = bounds.removeFromTop(28);
    llmFallbackToggle.setBounds(sidecarRow.removeFromLeft(190));
    sidecarLabel.setBounds(sidecarRow.removeFromLeft(80));
    sidecarUrlBox.setBounds(sidecarRow.removeFromLeft(390));

    bounds.removeFromTop(4);
    telemetryLabel.setBounds(bounds.removeFromTop(24));
    bounds.removeFromTop(8);

    const int sliderWidth = 108;
    const int gap = 6;
    const int rowY = bounds.getY() + 20;

    for (int i = 0; i < kMacroCount; ++i)
    {
        const int x = bounds.getX() + i * (sliderWidth + gap);
        sliderLabels[static_cast<size_t>(i)].setBounds(x, rowY, sliderWidth, 18);
        sliders[static_cast<size_t>(i)].setBounds(x, rowY + 22, sliderWidth, 200);
    }
}

void MagicTrackEditor::timerCallback()
{
    telemetryLabel.setText(processor.getTelemetry(), juce::dontSendNotification);
}

void MagicTrackEditor::submitCommand()
{
    const auto commandText = commandBox.getText().trim();
    if (commandText.isEmpty())
        return;

    const auto localTelemetry = processor.applyTextCommand(commandText);
    telemetryLabel.setText(localTelemetry, juce::dontSendNotification);

    if (processor.wasLastCommandMatchedLocally())
        return;

    if (!llmFallbackToggle.getToggleState())
        return;

    requestSidecarFallback(commandText);
}

void MagicTrackEditor::requestSidecarFallback(const juce::String& commandText)
{
    const auto endpoint = sidecarUrlBox.getText().trim();
    if (endpoint.isEmpty())
    {
        telemetryLabel.setText("applied: sidecar URL is empty", juce::dontSendNotification);
        return;
    }

    const auto profileName = profileBox.getText().isNotEmpty() ? profileBox.getText() : juce::String("General");
    const auto requestBody = buildSidecarRequestJson(commandText, profileName);
    const auto requestId = ++requestCounter;
    processor.setTelemetryFromUi("applied: local miss, asking sidecar...");
    telemetryLabel.setText(processor.getTelemetry(), juce::dontSendNotification);

    auto safeThis = juce::Component::SafePointer<MagicTrackEditor>(this);

    std::thread([safeThis, endpoint, requestBody, requestId]() {
        int statusCode = 0;
        juce::StringPairArray responseHeaders;
        juce::URL url(endpoint);
        auto response = juce::String();

        auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                           .withConnectionTimeoutMs(2000)
                           .withHttpRequestCmd("POST")
                           .withExtraHeaders("Content-Type: application/json\r\nAccept: application/json\r\n")
                           .withStatusCode(&statusCode)
                           .withResponseHeaders(&responseHeaders);

        auto stream = url.withPOSTData(requestBody).createInputStream(options);
        if (stream != nullptr)
            response = stream->readEntireStreamAsString();

        juce::MessageManager::callAsync([safeThis, requestId, statusCode, response]() {
            if (safeThis == nullptr)
                return;

            if (requestId != safeThis->requestCounter.load())
                return;

            if (statusCode < 200 || statusCode >= 300)
            {
                safeThis->processor.setTelemetryFromUi("applied: sidecar HTTP " + juce::String(statusCode));
                safeThis->telemetryLabel.setText(safeThis->processor.getTelemetry(), juce::dontSendNotification);
                return;
            }

            const auto telemetry = safeThis->processor.applyExternalDeltaJson(response, "sidecar");
            safeThis->telemetryLabel.setText(telemetry, juce::dontSendNotification);
        });
    }).detach();
}

juce::String MagicTrackEditor::buildSidecarRequestJson(const juce::String& text, const juce::String& profileName)
{
    juce::DynamicObject::Ptr payload = new juce::DynamicObject();
    payload->setProperty("text", text);
    payload->setProperty("profile", profileName);

    juce::Array<juce::var> macros;
    for (const auto* name : kMacroNames)
        macros.add(juce::String(name));
    payload->setProperty("macros", juce::var(macros));

    return juce::JSON::toString(juce::var(payload), false);
}
} // namespace magictrack
