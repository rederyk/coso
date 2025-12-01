// Copyright (c) 2025 rederyk
// Licensed under the MIT License. See LICENSE file for details.


#pragma once

#include <cstdint>
#include <memory>
#include <string>

// Simple effects parameters
struct EQParams {
    float bass_gain = 1.0f;
    float mid_gain = 1.0f;
    float treble_gain = 1.0f;
};

struct ReverbParams {
    float decay = 0.5f;
    float mix = 0.3f;
};

struct EchoParams {
    float delay_ms = 200.0f;
    float decay = 0.4f;
    float mix = 0.2f;
};

// Effect base class
class AudioEffect {
public:
    virtual ~AudioEffect() = default;
    virtual bool isEnabled() const = 0;
    virtual void setEnabled(bool enabled) = 0;
};

// Equalizer effect
class EqualizerEffect : public AudioEffect {
public:
    enum class Band {
        BASS = 0,
        LOW_MID = 1,
        MID = 2,
        HIGH_MID = 3,
        TREBLE = 4
    };

    EqualizerEffect();
    ~EqualizerEffect() override = default;

    bool isEnabled() const override { return enabled_; }
    void setEnabled(bool enabled) override { enabled_ = enabled; }

    float getBandGain(Band band) const;
    void setBandGain(Band band, float gain);

    void applyPreset(const std::string& preset_name);

private:
    bool enabled_ = false;
    float band_gains_[5] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f}; // Bass, Low-Mid, Mid, High-Mid, Treble
};

// Reverb effect
class ReverbEffect : public AudioEffect {
public:
    ReverbEffect();
    ~ReverbEffect() override = default;

    bool isEnabled() const override { return enabled_; }
    void setEnabled(bool enabled) override { enabled_ = enabled; }

    float getRoomSize() const { return room_size_; }
    void setRoomSize(float size) { room_size_ = size; }

    float getDamping() const { return damping_; }
    void setDamping(float damping) { damping_ = damping; }

    float getWetMix() const { return wet_mix_; }
    void setWetMix(float mix) { wet_mix_ = mix; }

private:
    bool enabled_ = false;
    float room_size_ = 0.5f;
    float damping_ = 0.5f;
    float wet_mix_ = 0.3f;
};

// Echo effect
class EchoEffect : public AudioEffect {
public:
    EchoEffect();
    ~EchoEffect() override = default;

    bool isEnabled() const override { return enabled_; }
    void setEnabled(bool enabled) override { enabled_ = enabled; }

    float getDelayTime() const { return delay_time_; }
    void setDelayTime(float time) { delay_time_ = time; }

    float getFeedback() const { return feedback_; }
    void setFeedback(float feedback) { feedback_ = feedback; }

    float getWetMix() const { return wet_mix_; }
    void setWetMix(float mix) { wet_mix_ = mix; }

private:
    bool enabled_ = false;
    float delay_time_ = 300.0f;
    float feedback_ = 0.4f;
    float wet_mix_ = 0.3f;
};

class EffectsChain {
public:
    EffectsChain();
    ~EffectsChain();

    // Initialize with sample rate
    void setSampleRate(uint32_t sample_rate);

    // Enable/disable effects
    void setEQEnabled(bool enabled) { eq_enabled_ = enabled; }
    void setReverbEnabled(bool enabled) { reverb_enabled_ = enabled; }
    void setEchoEnabled(bool enabled) { echo_enabled_ = enabled; }

    // Set parameters (preserves when sample rate changes)
    void setEQParams(const EQParams& params) { eq_params_ = params; }
    void setReverbParams(const ReverbParams& params) { reverb_params_ = params; }
    void setEchoParams(const EchoParams& params) { echo_params_ = params; }

    // Process stereo PCM buffer (in-place)
    void process(int16_t* buffer, size_t samples);

    // Get current params
    const EQParams& getEQParams() const { return eq_params_; }
    const ReverbParams& getReverbParams() const { return reverb_params_; }
    const EchoParams& getEchoParams() const { return echo_params_; }

    bool isEQEnabled() const { return eq_enabled_; }
    bool isReverbEnabled() const { return reverb_enabled_; }
    bool isEchoEnabled() const { return echo_enabled_; }

    // Get effect objects for UI
    EqualizerEffect* getEqualizer() { return &equalizer_; }
    ReverbEffect* getReverb() { return &reverb_; }
    EchoEffect* getEcho() { return &echo_; }

private:
    uint32_t sample_rate_ = 44100;

    bool eq_enabled_ = false;
    bool reverb_enabled_ = false;
    bool echo_enabled_ = false;

    EQParams eq_params_;
    ReverbParams reverb_params_;
    EchoParams echo_params_;

    // Effect objects
    EqualizerEffect equalizer_;
    ReverbEffect reverb_;
    EchoEffect echo_;

    // Simple delay buffers for echo/reverb
    std::unique_ptr<float[]> delay_buffer_;
    size_t delay_buffer_size_ = 0;
    size_t delay_write_pos_ = 0;

    // Simple IIR filters for EQ
    float bass_filter_state_[2] = {0.0f, 0.0f};
    float treble_filter_state_[2] = {0.0f, 0.0f};

    // Helper methods
    void applyEQ(float* left, float* right);
    void applyReverb(float* left, float* right);
    void applyEcho(float* left, float* right);
    void updateDelayBufferSize();
};
