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

bool AudioManager::playFile(const char* path) {
    if (!path) return false;

    auto& logger = Logger::getInstance();
    logger.infof("[AudioMgr] Playing file: %s", path);

    // Stop current playback
    if (player_->is_playing()) {
        player_->stop();
        vTaskDelay(pdMS_TO_TICKS(300));
    }

    // Select and arm source
    if (!player_->select_source(path)) {
        logger.error("[AudioMgr] Failed to select source");
        return false;
    }

    if (!player_->arm_source()) {
        logger.error("[AudioMgr] Failed to arm source");
        return false;
    }

    player_->start();
    return true;
}

bool AudioManager::playRadio(const char* url) {
    if (!url) return false;

    auto& logger = Logger::getInstance();
    logger.infof("[AudioMgr] Starting radio stream: %s", url);

    // Stop current playback
    if (player_->is_playing()) {
        player_->stop();
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    // Create timeshift manager
    auto* ts = new TimeshiftManager();
    ts->setStorageMode(preferred_storage_mode_);

    if (!ts->open(url)) {
        logger.error("[AudioMgr] Failed to open stream URL");
        delete ts;
        return false;
    }

    if (!ts->start()) {
        logger.error("[AudioMgr] Failed to start timeshift download");
        delete ts;
        return false;
    }

    logger.info("[AudioMgr] Waiting for first chunk...");

    // Wait for first chunk
    const uint32_t MAX_WAIT_MS = 10000;
    uint32_t start_wait = millis();
    while (ts->buffered_bytes() == 0) {
        if (millis() - start_wait > MAX_WAIT_MS) {
            logger.error("[AudioMgr] Timeout waiting for first chunk");
            delete ts;
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    logger.info("[AudioMgr] First chunk ready, starting playback");

    // Set auto-pause callback
    ts->set_auto_pause_callback([](bool should_pause) {
        getInstance().player_->set_pause(should_pause);
    });

    current_timeshift_ = ts;

    // Transfer ownership to player
    player_->select_source(std::unique_ptr<IDataSource>(ts));

    if (!player_->arm_source()) {
        logger.error("[AudioMgr] Failed to arm timeshift source");
        current_timeshift_ = nullptr;
        return false;
    }

    player_->start();
    logger.info("[AudioMgr] Radio stream started successfully");
    return true;
}

bool AudioManager::playRadioStation(size_t station_index) {
    if (station_index >= radio_stations_.size()) {
        return false;
    }

    auto& logger = Logger::getInstance();
    logger.infof("[AudioMgr] Playing station: %s", radio_stations_[station_index].name.c_str());

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
    Logger::getInstance().infof("[AudioMgr] Playback started: %s", path);
    // Store start time for duration calculation
    getInstance().playback_start_ms_ = millis();
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
    Logger::getInstance().errorf("[AudioMgr] Playback error: %s - %s", path, detail);
}
