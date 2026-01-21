#include "PluginProcessor.h"
#include "PluginEditor.h"

Vt2aAudioProcessor::Vt2aAudioProcessor()
    : AudioProcessor(
          BusesProperties()
              .withInput("Input", juce::AudioChannelSet::stereo(), true)
              .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout()) {
  // 4x Oversampling for aliasing prevention
  oversampling = std::make_unique<juce::dsp::Oversampling<float>>(
      2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR);
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

  // Reinitialize oversampling if channel count changed (4x = factor 2)
  if (oversampling == nullptr ||
      oversampling->numChannels != spec.numChannels) {
    oversampling = std::make_unique<juce::dsp::Oversampling<float>>(
        spec.numChannels, 2,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR);
  }

  oversampling->initProcessing(spec.maximumBlockSize);
  oversampling->reset();

  // Filter specs use the OVERSAMPLED rate (4x)
  juce::dsp::ProcessSpec osSpec = spec;
  osSpec.sampleRate = sampleRate * oversampling->getOversamplingFactor();

  lowMidFilter.prepare(osSpec);
  highCutFilter.prepare(osSpec);
  dcBlocker.prepare(osSpec);

  // DC Blocker at 10Hz
  dcBlocker.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(
      osSpec.sampleRate, 10.0f);

  dryBuffer.setSize(getTotalNumOutputChannels(), samplesPerBlock);
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

  // === Copy dry signal for later mixing ===
  if (dryBuffer.getNumSamples() < numSamples)
    dryBuffer.setSize(totalNumInputChannels, numSamples);
  for (int i = 0; i < totalNumInputChannels; ++i)
    dryBuffer.copyFrom(i, 0, buffer, i, 0, numSamples);

  // === Filter Updates (4x Oversampled Rate) ===
  float osRate = getSampleRate() * oversampling->getOversamplingFactor();

  // Mid-bass warmth: Peak at 400Hz, Q=0.6, Gain scales with drive (+1dB to
  // +4dB)
  float warmthGainDb = 1.0f + (normalizedDrive * 3.0f);
  *lowMidFilter.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter(
      osRate, 400.0f, 0.6f, warmthGainDb);

  // High-frequency rolloff: 18kHz → 8kHz as drive increases
  float cutoff = 18000.0f - (normalizedDrive * 10000.0f);
  cutoff = juce::jmax(cutoff, 6000.0f);
  *highCutFilter.state =
      *juce::dsp::IIR::Coefficients<float>::makeLowPass(osRate, cutoff, 0.707f);

  // === Input gain: 0dB to +18dB (less aggressive than before) ===
  float inputGainDb = normalizedDrive * 18.0f;
  float inputGainLin = juce::Decibels::decibelsToGain(inputGainDb);

  // === DSP with 4x Oversampling ===
  juce::dsp::AudioBlock<float> block(buffer);
  juce::dsp::AudioBlock<float> osBlock = oversampling->processSamplesUp(block);

  // Apply mid-bass warmth first
  juce::dsp::ProcessContextReplacing<float> context(osBlock);
  lowMidFilter.process(context);

  // === Vacuum Tube Saturation ===
  auto osNumSamples = osBlock.getNumSamples();
  auto osNumChannels = osBlock.getNumChannels();

  for (size_t channel = 0; channel < osNumChannels; ++channel) {
    auto *data = osBlock.getChannelPointer(channel);
    for (size_t i = 0; i < osNumSamples; ++i) {
      float x = data[i] * inputGainLin;

      // === Soft-knee saturation (smooth tanh curve) ===
      // Low drive: almost clean but "forward" presence
      // High drive: warm saturation without harsh clipping
      float driveAmount = 1.0f + normalizedDrive * 3.0f;
      float sat = std::tanh(x * driveAmount) / driveAmount;

      // === Even harmonic generation (2nd harmonic dominant) ===
      // Adds warmth and "tube-like" character
      float evenHarmonic = x * x * 0.12f * normalizedDrive;
      // Asymmetric clipping for more natural tube behavior
      if (x < 0.0f)
        evenHarmonic *= 0.7f;

      // === Soft compression (subtle transient taming) ===
      // Doesn't crush transients, just gently limits peaks
      float compressed = sat + evenHarmonic;
      float compressionThresh = 0.8f - (normalizedDrive * 0.2f);
      if (std::abs(compressed) > compressionThresh) {
        float excess = std::abs(compressed) - compressionThresh;
        float ratio = 0.3f; // Soft ratio
        compressed = (compressed > 0.0f ? 1.0f : -1.0f) *
                     (compressionThresh + excess * ratio);
      }

      data[i] = compressed;
    }
  }

  // High-frequency rolloff and DC blocking
  highCutFilter.process(context);
  dcBlocker.process(context);

  // Downsample back
  oversampling->processSamplesDown(block);

  // === Output gain compensation (prevent excessive loudness) ===
  float outputCompensation = 1.0f / (1.0f + normalizedDrive * 0.3f);
  buffer.applyGain(outputCompensation);

  // === Mix: Blend dry and wet signals ===
  for (int ch = 0; ch < totalNumInputChannels; ++ch) {
    auto *wet = buffer.getWritePointer(ch);
    auto *dry = dryBuffer.getReadPointer(ch);
    for (int i = 0; i < numSamples; ++i) {
      wet[i] = dry[i] + (wet[i] - dry[i]) * mixParam;
    }
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
