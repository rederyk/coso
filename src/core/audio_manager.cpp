#include "audio_manager.h"
#include "utils/logger.h"
#include "drivers/sd_card_driver.h"

AudioManager& AudioManager::getInstance() {
    static AudioManager instance;
    return instance;
}

AudioManager::AudioManager()
    : current_timeshift_(nullptr),
      preferred_storage_mode_(StorageMode::PSRAM_ONLY),  // Changed from SD_CARD to PSRAM_ONLY to avoid SD write errors
      progress_callback_(nullptr),
      metadata_callback_(nullptr),
      state_callback_(nullptr),
      last_state_(PlayerState::STOPPED) {

    player_.reset(new AudioPlayer());
}

void AudioManager::begin() {
    auto& logger = Logger::getInstance();
    logger.info("[AudioMgr] Initializing audio manager");

    // Setup callbacks
    PlayerCallbacks callbacks;
    callbacks.on_start = onStart;
    callbacks.on_stop = onStop;
    callbacks.on_end = onEnd;
    callbacks.on_error = onError;
    callbacks.on_metadata = onMetadata;
    callbacks.on_progress = onProgress;
    player_->set_callbacks(callbacks);

    // Load default radio stations
    loadDefaultStations();

    logger.info("[AudioMgr] Audio manager initialized");
}

void AudioManager::tick() {
    if (player_) {
        player_->tick_housekeeping();

        // Check for state changes
        PlayerState current_state = player_->state();
        if (current_state != last_state_) {
            last_state_ = current_state;
            if (state_callback_) {
                state_callback_(current_state);
            }
        }
    }
}

bool AudioManager::playFile(const char* path, uint32_t expected_sample_rate, uint32_t expected_bitrate) {
    if (!path) return false;

    auto& logger = Logger::getInstance();
    logger.infof("[AudioMgr] Playing file: %s (expected sr=%u, br=%u)", path, expected_sample_rate, expected_bitrate);

    // Stop current playback
    if (player_->is_playing()) {
        player_->stop();
        vTaskDelay(pdMS_TO_TICKS(300));
    }

    // Select and arm source
    if (!player_->select_source(path)) {
        logger.errorf("[AudioMgr] Failed to select source for %s", path);
        return false;
    }

    if (!player_->arm_source()) {
        logger.errorf("[AudioMgr] Failed to arm source for %s", path);
        return false;
    }

    player_->start();

    // Post-start validation for MP3 (or any format)
    if (player_->state() == PlayerState::PLAYING) {
        uint32_t detected_sr = player_->current_sample_rate();
        uint32_t detected_br = player_->current_bitrate();
        const char* fmt_str = "UNKNOWN";
        AudioFormat fmt = player_->current_format();
        if (fmt == AudioFormat::MP3) fmt_str = "MP3";
        else if (fmt == AudioFormat::WAV) fmt_str = "WAV";
        else if (fmt == AudioFormat::AAC) fmt_str = "AAC";

        logger.infof("[AudioMgr] Playback started: format=%s, sr=%u Hz, br=%u kbps",
                     fmt_str, detected_sr, detected_br);

        if (expected_sample_rate > 0 && detected_sr != expected_sample_rate) {
            logger.warnf("[AudioMgr] Sample rate mismatch: detected %u != expected %u. Consider forcing.", detected_sr, expected_sample_rate);
        }
        if (expected_bitrate > 0 && detected_br != expected_bitrate) {
            logger.warnf("[AudioMgr] Bitrate mismatch: detected %u != expected %u.", detected_br, expected_bitrate);
        }

        if (detected_sr == 0 || detected_sr > 96000) {
            logger.errorf("[AudioMgr] Invalid sample rate detected (%u Hz). Forcing retry or fallback.", detected_sr);
            player_->stop();
            return false;
        }
    } else {
        logger.errorf("[AudioMgr] Failed to start playback");
        return false;
    }

    return true;
}

bool AudioManager::playRadio(const char* url, uint32_t expected_sample_rate, uint32_t expected_bitrate) {
    if (!url) return false;

    auto& logger = Logger::getInstance();
    logger.infof("[AudioMgr] Starting radio stream: %s (expected sr=%u, br=%u)", url, expected_sample_rate, expected_bitrate);

    // Stop current playback
    if (player_->is_playing()) {
        player_->stop();
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    // Create timeshift manager
    auto* ts = new TimeshiftManager();
    ts->setStorageMode(preferred_storage_mode_);

    if (!ts->open(url)) {
        logger.errorf("[AudioMgr] Failed to open stream URL %s", url);
        delete ts;
        return false;
    }

    if (!ts->start()) {
        logger.errorf("[AudioMgr] Failed to start timeshift download for %s", url);
        delete ts;
        return false;
    }

    logger.info("[AudioMgr] Waiting for first chunk...");

    // Wait for first chunk
    const uint32_t MAX_WAIT_MS = 10000;
    uint32_t start_wait = millis();
    while (ts->buffered_bytes() == 0) {
        if (millis() - start_wait > MAX_WAIT_MS) {
            logger.errorf("[AudioMgr] Timeout waiting for first chunk from %s", url);
            delete ts;
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    logger.infof("[AudioMgr] First chunk ready (%u bytes), starting playback", (unsigned)ts->buffered_bytes());

    // Set auto-pause callback
    ts->set_auto_pause_callback([](bool should_pause) {
        getInstance().player_->set_pause(should_pause);
    });

    current_timeshift_ = ts;

    // Transfer ownership to player
    player_->select_source(std::unique_ptr<IDataSource>(ts));

    if (!player_->arm_source()) {
        logger.errorf("[AudioMgr] Failed to arm timeshift source for %s", url);
        current_timeshift_ = nullptr;
        return false;
    }

    player_->start();

    // Post-start validation (for streams, bitrate may be estimated)
    if (player_->state() == PlayerState::PLAYING) {
        uint32_t detected_sr = player_->current_sample_rate();
        uint32_t detected_br = player_->current_bitrate();
        const char* fmt_str = "UNKNOWN";
        AudioFormat fmt = player_->current_format();
        if (fmt == AudioFormat::MP3) fmt_str = "MP3";
        else if (fmt == AudioFormat::WAV) fmt_str = "WAV";
        else if (fmt == AudioFormat::AAC) fmt_str = "AAC";

        logger.infof("[AudioMgr] Stream playback started: format=%s, sr=%u Hz, br=%u kbps",
                     fmt_str, detected_sr, detected_br);

        if (expected_sample_rate > 0 && detected_sr != expected_sample_rate) {
            logger.warnf("[AudioMgr] Sample rate mismatch in stream: detected %u != expected %u. Consider forcing.", detected_sr, expected_sample_rate);
        }
        if (expected_bitrate > 0 && detected_br > 0 && detected_br != expected_bitrate) {
            logger.warnf("[AudioMgr] Bitrate mismatch in stream: detected %u != expected %u.", detected_br, expected_bitrate);
        }

        if (detected_sr == 0 || detected_sr > 96000) {
            logger.errorf("[AudioMgr] Invalid sample rate in stream (%u Hz). Forcing retry or fallback.", detected_sr);
            player_->stop();
            return false;
        }
    } else {
        logger.errorf("[AudioMgr] Failed to start stream playback");
        return false;
    }

    logger.info("[AudioMgr] Radio stream started successfully");
    return true;
}

bool AudioManager::playRadioStation(size_t station_index) {
    if (station_index >= radio_stations_.size()) {
        return false;
    }

    auto& logger = Logger::getInstance();
    logger.infof("[AudioMgr] Playing station: %s (%s)", radio_stations_[station_index].name.c_str(), radio_stations_[station_index].url.c_str());

    return playRadio(radio_stations_[station_index].url.c_str());
}

void AudioManager::stop() {
    if (player_) {
        player_->stop();
        current_timeshift_ = nullptr;
    }
}

void AudioManager::togglePause() {
    if (player_) {
        player_->toggle_pause();
    }
}

void AudioManager::setPause(bool pause) {
    if (player_) {
        player_->set_pause(pause);
    }
}

void AudioManager::setVolume(int percent) {
    if (player_) {
        player_->set_volume(percent);
    }
}

void AudioManager::seek(int seconds) {
    if (player_) {
        player_->request_seek(seconds);
    }
}

void AudioManager::toggleStorageMode() {
    StorageMode current_mode = current_timeshift_ ? current_timeshift_->getStorageMode() : preferred_storage_mode_;
    StorageMode new_mode = (current_mode == StorageMode::SD_CARD) ? StorageMode::PSRAM_ONLY : StorageMode::SD_CARD;

    if (current_timeshift_) {
        if (!current_timeshift_->switchStorageMode(new_mode)) {
            Logger::getInstance().error("[AudioMgr] Failed to switch timeshift storage mode");
            return;
        }
        Logger::getInstance().infof("[AudioMgr] Timeshift storage switched to %s",
                                    new_mode == StorageMode::SD_CARD ? "SD" : "PSRAM");
    } else {
        Logger::getInstance().infof("[AudioMgr] Preferred timeshift storage set to %s",
                                    new_mode == StorageMode::SD_CARD ? "SD" : "PSRAM");
    }

    preferred_storage_mode_ = new_mode;
}

StorageMode AudioManager::currentStorageMode() const {
    if (current_timeshift_) {
        return current_timeshift_->getStorageMode();
    }
    return preferred_storage_mode_;
}


const RadioStation* AudioManager::getStation(size_t index) const {
    if (index >= radio_stations_.size()) {
        return nullptr;
    }
    return &radio_stations_[index];
}

bool AudioManager::addStation(const char* name, const char* url, const char* genre) {
    if (!name || !url) return false;

    RadioStation station;
    station.name = name;
    station.url = url;
    station.genre = genre ? genre : "";

    radio_stations_.push_back(station);
    Logger::getInstance().infof("[AudioMgr] Added station: %s", name);
    return true;
}

bool AudioManager::removeStation(size_t index) {
    if (index >= radio_stations_.size()) {
        return false;
    }

    radio_stations_.erase(radio_stations_.begin() + index);
    return true;
}

void AudioManager::loadDefaultStations() {
    radio_stations_.clear();

    // Default stations (2 Italian stations as requested)
    addStation("Radio Paradise", "https://paradise.stream.laut.fm/paradise", "Pop");
    addStation("Radio 105", "http://icecast.unitedradio.it/Radio105.mp3", "Pop/Rock");

    Logger::getInstance().infof("[AudioMgr] Loaded %d default stations", radio_stations_.size());
}

// Static callbacks
void AudioManager::onProgress(uint32_t pos_ms, uint32_t dur_ms) {
    auto& instance = getInstance();
    if (instance.progress_callback_) {
        instance.progress_callback_(pos_ms, dur_ms);
    }
}

void AudioManager::onMetadata(const Metadata& meta, const char* path) {
    auto& instance = getInstance();
    if (instance.metadata_callback_) {
        instance.metadata_callback_(meta);
    }
}

void AudioManager::onStart(const char* path) {
    auto& instance = getInstance();
    auto& logger = Logger::getInstance();
    logger.infof("[AudioMgr] Playback started: %s", path ? path : "unknown");
    
    // Log detected params on start
    if (instance.player_) {
        AudioFormat fmt = instance.player_->current_format();
        uint32_t sr = instance.player_->current_sample_rate();
        uint32_t br = instance.player_->current_bitrate();
        const char* fmt_str = "UNKNOWN";
        if (fmt == AudioFormat::MP3) fmt_str = "MP3";
        else if (fmt == AudioFormat::WAV) fmt_str = "WAV";
        else if (fmt == AudioFormat::AAC) fmt_str = "AAC";
        logger.infof("[AudioMgr] Detected on start: format=%s, sr=%u Hz, br=%u kbps", fmt_str, sr, br);
    }
    
    // Store start time for duration calculation
    instance.playback_start_ms_ = millis();
}

void AudioManager::onStop(const char* path, PlayerState state) {
    Logger::getInstance().infof("[AudioMgr] Playback stopped: %s (state: %d)", path, static_cast<int>(state));
}

void AudioManager::onEnd(const char* path) {
    uint32_t playback_end_ms = millis();
    uint32_t playback_duration_ms = playback_end_ms - getInstance().playback_start_ms_;
    Logger::getInstance().infof("[AudioMgr] Playback ended: %s (duration: %u ms)", path, playback_duration_ms);
}

void AudioManager::onError(const char* path, const char* detail) {
    auto& logger = Logger::getInstance();
    logger.errorf("[AudioMgr] Playback error: %s - %s", path ? path : "unknown", detail ? detail : "no detail");

    // If MP3-related (check if path ends with .mp3 or detail mentions MP3/decoder)
    if (path && strstr(path, ".mp3")) {
        logger.error("[AudioMgr] MP3-specific error detected. Check decoder init (sample rate/bitrate detection).");
    } else if (detail && (strstr(detail, "decoder") || strstr(detail, "MP3"))) {
        logger.error("[AudioMgr] Decoder/MP3 init failure. Consider specifying expected sample_rate (e.g., 44100) or bitrate.");
    }

    // Attempt recovery if not already in error state
    auto& instance = getInstance();
    if (instance.player_ && instance.player_->state() != PlayerState::ERROR) {
        logger.warn("[AudioMgr] Attempting auto-recovery...");
        if (instance.player_->is_playing() || instance.player_->state() == PlayerState::PLAYING) {
            instance.player_->stop();
            vTaskDelay(pdMS_TO_TICKS(500));
            logger.info("[AudioMgr] Recovery: Playback stopped. Manual restart recommended.");
        }
    }
}
