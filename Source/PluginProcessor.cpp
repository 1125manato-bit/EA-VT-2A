#include "PluginProcessor.h"
#include "PluginEditor.h"

Vt2aAudioProcessor::Vt2aAudioProcessor()
    : AudioProcessor(
          BusesProperties()
              .withInput("Input", juce::AudioChannelSet::stereo(), true)
              .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout()) {
  oversampling = std::make_unique<juce::dsp::Oversampling<float>>(
      2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR);
}

Vt2aAudioProcessor::~Vt2aAudioProcessor() {}

juce::AudioProcessorValueTreeState::ParameterLayout
Vt2aAudioProcessor::createParameterLayout() {
  juce::AudioProcessorValueTreeState::ParameterLayout layout;
  layout.add(std::make_unique<juce::AudioParameterFloat>(
      "DRIVE", "Drive", juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
      0.0f));
  layout.add(std::make_unique<juce::AudioParameterFloat>(
      "MIX", "Mix", juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f), 50.0f));
  return layout;
}

const juce::String Vt2aAudioProcessor::getName() const {
  return JucePlugin_Name;
}

bool Vt2aAudioProcessor::acceptsMidi() const { return false; }
bool Vt2aAudioProcessor::producesMidi() const { return false; }
bool Vt2aAudioProcessor::isMidiEffect() const { return false; }
double Vt2aAudioProcessor::getTailLengthSeconds() const { return 0.0; }
int Vt2aAudioProcessor::getNumPrograms() { return 1; }
int Vt2aAudioProcessor::getCurrentProgram() { return 0; }
void Vt2aAudioProcessor::setCurrentProgram(int index) {}
const juce::String Vt2aAudioProcessor::getProgramName(int index) { return {}; }
void Vt2aAudioProcessor::changeProgramName(int index,
                                           const juce::String &newName) {}

void Vt2aAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
  juce::dsp::ProcessSpec spec;
  spec.sampleRate = sampleRate;
  spec.maximumBlockSize = samplesPerBlock;
  spec.numChannels = getTotalNumOutputChannels();

  if (oversampling == nullptr ||
      oversampling->numChannels != spec.numChannels) {
    oversampling = std::make_unique<juce::dsp::Oversampling<float>>(
        spec.numChannels, 1,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR);
  }

  oversampling->initProcessing(spec.maximumBlockSize);
  oversampling->reset();

  // Filter specs use the OVERSAMPLED rate
  juce::dsp::ProcessSpec osSpec = spec;
  osSpec.sampleRate = sampleRate * oversampling->getOversamplingFactor();

  lowMidFilter.prepare(osSpec);
  highCutFilter.prepare(osSpec);
  dcBlocker.prepare(osSpec);

  dcBlocker.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(
      osSpec.sampleRate, 10.0f);

  dryBuffer.setSize(getTotalNumOutputChannels(), samplesPerBlock);

  dryDelayLine.prepare(spec);
  dryDelayLine.setMaximumDelayInSamples(1024);
}

void Vt2aAudioProcessor::releaseResources() {}

bool Vt2aAudioProcessor::isBusesLayoutSupported(
    const BusesLayout &layouts) const {
  const auto &mainIn = layouts.getMainInputChannelSet();
  const auto &mainOut = layouts.getMainOutputChannelSet();

  if (mainIn != mainOut)
    return false;
  if (mainIn != juce::AudioChannelSet::mono() &&
      mainIn != juce::AudioChannelSet::stereo())
    return false;

  return true;
}

void Vt2aAudioProcessor::processBlock(juce::AudioBuffer<float> &buffer,
                                      juce::MidiBuffer &midiMessages) {
  juce::ScopedNoDenormals noDenormals;
  auto totalNumInputChannels = getTotalNumInputChannels();
  auto totalNumOutputChannels = getTotalNumOutputChannels();
  auto numSamples = buffer.getNumSamples();

  for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
    buffer.clear(i, 0, numSamples);

  float driveParam = *apvts.getRawParameterValue("DRIVE");
  float mixParam = *apvts.getRawParameterValue("MIX") / 100.0f;
  float normalizedDrive = driveParam / 100.0f;

  // Wet/Dry logic (Copy dry and delay to match Oversampling latency)
  if (dryBuffer.getNumSamples() < numSamples)
    dryBuffer.setSize(totalNumInputChannels, numSamples);

  auto latency = oversampling->getLatencyInSamples();
  for (int i = 0; i < totalNumInputChannels; ++i) {
    auto *dryData = dryBuffer.getWritePointer(i);
    auto *inData = buffer.getReadPointer(i);
    for (int s = 0; s < numSamples; ++s) {
      dryDelayLine.pushSample(i, inData[s]);
      dryData[s] = dryDelayLine.popSample(i, latency);
    }
  }

  // Filter Updates (Oversampled Rate)
  float osRate = getSampleRate() * oversampling->getOversamplingFactor();
  float filterGain = 1.0f + (normalizedDrive * 2.0f);
  *lowMidFilter.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter(
      osRate, 500.0f, 0.7f, juce::Decibels::gainToDecibels(filterGain));

  float cutoff = 15000.0f - (normalizedDrive * 5000.0f);
  if (cutoff < 1000.0f)
    cutoff = 1000.0f;
  *highCutFilter.state =
      *juce::dsp::IIR::Coefficients<float>::makeLowPass(osRate, cutoff);

  // Map drive to gain
  float inputGainDb = normalizedDrive * 24.0f;
  float inputGainLin = juce::Decibels::decibelsToGain(inputGainDb);

  // DSP with Oversampling
  juce::dsp::AudioBlock<float> block(buffer);
  juce::dsp::AudioBlock<float> osBlock = oversampling->processSamplesUp(block);

  juce::dsp::ProcessContextReplacing<float> context(osBlock);
  lowMidFilter.process(context);

  auto osNumSamples = osBlock.getNumSamples();
  auto osNumChannels = osBlock.getNumChannels();
  for (size_t channel = 0; channel < osNumChannels; ++channel) {
    auto *data = osBlock.getChannelPointer(channel);
    for (size_t i = 0; i < osNumSamples; ++i) {
      float x = data[i] * inputGainLin;
      float sat = std::tanh(x);
      float even = (x * x) * 0.1f * normalizedDrive;
      data[i] = sat + even;
    }
  }

  highCutFilter.process(context);
  dcBlocker.process(context);

  oversampling->processSamplesDown(block);

  // Mix (Constant Power)
  float mixAngle = mixParam * juce::MathConstants<float>::halfPi;
  float dryGain = std::cos(mixAngle);
  float wetGain = std::sin(mixAngle);

  for (int i = 0; i < totalNumInputChannels; ++i) {
    buffer.applyGain(i, 0, numSamples, wetGain);
    buffer.addFrom(i, 0, dryBuffer, i, 0, numSamples, dryGain);
  }
}

juce::AudioProcessorEditor *Vt2aAudioProcessor::createEditor() {
  return new Vt2aAudioProcessorEditor(*this);
}

bool Vt2aAudioProcessor::hasEditor() const { return true; }

void Vt2aAudioProcessor::getStateInformation(juce::MemoryBlock &destData) {
  auto state = apvts.copyState();
  std::unique_ptr<juce::XmlElement> xml(state.createXml());
  copyXmlToBinary(*xml, destData);
}

void Vt2aAudioProcessor::setStateInformation(const void *data,
                                             int sizeInBytes) {
  std::unique_ptr<juce::XmlElement> xmlState(
      getXmlFromBinary(data, sizeInBytes));
  if (xmlState.get() != nullptr)
    if (xmlState->hasTagName(apvts.state.getType()))
      apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter() {
  return new Vt2aAudioProcessor();
}
