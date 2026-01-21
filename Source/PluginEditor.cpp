#include "PluginEditor.h"
#include "PluginProcessor.h"

Vt2aAudioProcessorEditor::Vt2aAudioProcessorEditor(Vt2aAudioProcessor &p)
    : AudioProcessorEditor(&p), audioProcessor(p) {
  backgroundImage = juce::ImageCache::getFromMemory(
      BinaryData::background_jpg, BinaryData::background_jpgSize);

  // Drive Slider
  // Drive Slider
  driveSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
  driveSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
  driveSlider.setPopupDisplayEnabled(true, false, this);
  driveSlider.setLookAndFeel(&lnf);
  addAndMakeVisible(driveSlider);

  driveAttachment =
      std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
          audioProcessor.apvts, "DRIVE", driveSlider);

  // Mix Slider
  mixSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
  mixSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
  mixSlider.setPopupDisplayEnabled(true, false, this);
  mixSlider.setLookAndFeel(&lnf);
  addAndMakeVisible(mixSlider);

  mixAttachment =
      std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
          audioProcessor.apvts, "MIX", mixSlider);

  setSize(1024, 866);
}

Vt2aAudioProcessorEditor::~Vt2aAudioProcessorEditor() {
  driveSlider.setLookAndFeel(nullptr);
  mixSlider.setLookAndFeel(nullptr);
}

void Vt2aAudioProcessorEditor::paint(juce::Graphics &g) {
  g.drawImage(backgroundImage, getLocalBounds().toFloat());
}

void Vt2aAudioProcessorEditor::resized() {
  // User Specified Coordinates (Center):
  // DRIVE: 216, 626
  // MIX: 809, 626
  // Size: 206

  const int knobSize = 206;
  const int halfSize = knobSize / 2; // 103

  // Drive Slider
  driveSlider.setBounds(216 - halfSize, 626 - halfSize, knobSize, knobSize);

  // Mix Slider
  mixSlider.setBounds(809 - halfSize, 626 - halfSize, knobSize, knobSize);
}
