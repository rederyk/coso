#pragma once

#include <memory>
#include <vector>
#include <string>
#include "audio_player_local.h"
#include "../lib/openESPaudio/src/timeshift_manager.h"
#include "../lib/openESPaudio/src/audio_effects.h"
#include "../lib/openESPaudio/src/audio_types.h"

/**
 * Radio station configuration
 */
struct RadioStation {
    std::string name;
    std::string url;
    std::string genre;
};

/**
 * Audio Manager Singleton
 * Centralizes audio playback for the entire OS
 */
class AudioManager {
public:
    static AudioManager& getInstance();

    // Lifecycle
    void begin();
    void tick();

    // Playback control
    bool playFile(const char* path, uint32_t expected_sample_rate = 0, uint32_t expected_bitrate = 0);
    bool playRadio(const char* url, uint32_t expected_sample_rate = 0, uint32_t expected_bitrate = 0);
    bool playRadioStation(size_t station_index);
    void stop();
    void togglePause();
    void setPause(bool pause);
    void setVolume(int percent);
    void seek(int seconds);
    void toggleStorageMode();
    StorageMode currentStorageMode() const;
    StorageMode preferredStorageMode() const { return preferred_storage_mode_; }

    // Radio stations management
    size_t getNumStations() const { return radio_stations_.size(); }
    const RadioStation* getStation(size_t index) const;
    bool addStation(const char* name, const char* url, const char* genre = "");
    bool removeStation(size_t index);
    void loadDefaultStations();

    // Player state
    bool isPlaying() const { return player_->is_playing(); }
    PlayerState getState() const { return player_->state(); }
    uint32_t getCurrentPositionMs() const { return player_->current_position_ms(); }
    uint32_t getTotalDurationMs() const { return player_->total_duration_ms(); }
    int getVolume() const { return player_->current_volume(); }
    const Metadata& getMetadata() const { return player_->metadata(); }
    SourceType getSourceType() const { return player_->source_type(); }

    // Effects access
    EffectsChain& getEffectsChain() { return player_->getEffectsChain(); }

    // Callbacks
    using ProgressCallback = void(*)(uint32_t pos_ms, uint32_t dur_ms);
    using MetadataCallback = void(*)(const Metadata& meta);
    using StateChangeCallback = void(*)(PlayerState state);

    void setProgressCallback(ProgressCallback cb) { progress_callback_ = cb; }
    void setMetadataCallback(MetadataCallback cb) { metadata_callback_ = cb; }
    void setStateChangeCallback(StateChangeCallback cb) { state_callback_ = cb; }

private:
    AudioManager();
    ~AudioManager() = default;
    AudioManager(const AudioManager&) = delete;
    AudioManager& operator=(const AudioManager&) = delete;

    static void onProgress(uint32_t pos_ms, uint32_t dur_ms);
    static void onMetadata(const Metadata& meta, const char* path);
    static void onStart(const char* path);
    static void onStop(const char* path, PlayerState state);
    static void onEnd(const char* path);
    static void onError(const char* path, const char* detail);

    std::unique_ptr<AudioPlayer> player_;
    std::vector<RadioStation> radio_stations_;
    TimeshiftManager* current_timeshift_;
    StorageMode preferred_storage_mode_;

    ProgressCallback progress_callback_;
    MetadataCallback metadata_callback_;
    StateChangeCallback state_callback_;
    PlayerState last_state_;
    uint32_t playback_start_ms_;
};
