#pragma once

#include "PluginProcessor.h"
#include <JuceHeader.h>

class Vt2aAudioProcessorEditor : public juce::AudioProcessorEditor {
public:
  Vt2aAudioProcessorEditor(Vt2aAudioProcessor &);
  ~Vt2aAudioProcessorEditor() override;

  void paint(juce::Graphics &) override;
  void resized() override;

private:
  Vt2aAudioProcessor &audioProcessor;

  juce::Image backgroundImage;

  juce::Slider driveSlider;
  juce::Slider mixSlider;

  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
      driveAttachment;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
      mixAttachment;

  class Vt2aLookAndFeel : public juce::LookAndFeel_V4 {
  public:
    Vt2aLookAndFeel() {
      knobImage = juce::ImageCache::getFromMemory(BinaryData::knob_png,
                                                  BinaryData::knob_pngSize);
    }

    void drawRotarySlider(juce::Graphics &g, int x, int y, int width,
                          int height, float sliderPos,
                          const float rotaryStartAngle,
                          const float rotaryEndAngle, juce::Slider &) override {
      if (knobImage.isValid()) {
        const float angle =
            rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

        // Ensure image is scaled to fit the requested width/height (206x206)
        auto scaledKnob = knobImage.rescaled(
            width, height, juce::Graphics::highResamplingQuality);

        const float centreX = x + width * 0.5f;
        const float centreY = y + height * 0.5f;

        juce::AffineTransform t =
            juce::AffineTransform::translation(-width * 0.5f, -height * 0.5f)
                .rotated(angle)
                .translated(centreX, centreY);

        g.drawImageTransformed(scaledKnob, t);
      }
    }

  private:
    juce::Image knobImage;
  };

  Vt2aLookAndFeel lnf;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Vt2aAudioProcessorEditor)
};
