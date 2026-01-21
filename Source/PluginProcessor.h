#pragma once

#include <JuceHeader.h>

class Vt2aAudioProcessor : public juce::AudioProcessor {
public:
  Vt2aAudioProcessor();
  ~Vt2aAudioProcessor() override;

  void prepareToPlay(double sampleRate, int samplesPerBlock) override;
  void releaseResources() override;

#ifndef JucePlugin_PreferredChannelConfigurations
  bool isBusesLayoutSupported(const BusesLayout &layouts) const override;
#endif

  void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;

  juce::AudioProcessorEditor *createEditor() override;
  bool hasEditor() const override;

  const juce::String getName() const override;

  bool acceptsMidi() const override;
  bool producesMidi() const override;
  bool isMidiEffect() const override;
  double getTailLengthSeconds() const override;

  int getNumPrograms() override;
  int getCurrentProgram() override;
  void setCurrentProgram(int index) override;
  const juce::String getProgramName(int index) override;
  void changeProgramName(int index, const juce::String &newName) override;

  void getStateInformation(juce::MemoryBlock &destData) override;
  void setStateInformation(const void *data, int sizeInBytes) override;

  static juce::AudioProcessorValueTreeState::ParameterLayout
  createParameterLayout();

  juce::AudioProcessorValueTreeState apvts;

private:
  // DSP Objects
  std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;

  // Filters
  juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                 juce::dsp::IIR::Coefficients<float>>
      lowMidFilter;
  juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                 juce::dsp::IIR::Coefficients<float>>
      highCutFilter;
  juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                 juce::dsp::IIR::Coefficients<float>>
      dcBlocker;

  // Dry Buffer and Delay for Mix alignment
  juce::AudioBuffer<float> dryBuffer;
  juce::dsp::DelayLine<float> dryDelayLine;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Vt2aAudioProcessor)
};
