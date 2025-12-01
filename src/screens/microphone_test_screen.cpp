#include "screens/microphone_test_screen.h"
#include "ui/ui_symbols.h"
#include "utils/logger.h"
#include "utils/color_utils.h"
#include "core/voice_assistant.h"
#include "core/audio_manager.h"
#include <Arduino.h>
#include <vector>
#include <string>
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <utility>
#include <functional>
#include <limits>
#include <FS.h>
#include <SD_MMC.h>
#include <LittleFS.h>
#include <driver/i2s.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace {

constexpr const char* kRecordingsDir = "/test_recordings";
constexpr int kMicI2sBckPin = 5;
constexpr int kMicI2sWsPin = 7;
constexpr int kMicI2sDinPin = 6;
constexpr int kMicI2sMckPin = 4;
constexpr uint32_t kDefaultRecordingDurationSeconds = 6;
constexpr uint32_t kMicSampleRate = 16000;

enum class RecordingStorage {
    SD_CARD,
    LITTLEFS
};

struct RecordingStorageInfo {
    RecordingStorage storage = RecordingStorage::LITTLEFS;
    fs::FS* fs = nullptr;
    const char* directory = kRecordingsDir;
    const char* playback_prefix = "";
    const char* label = "LittleFS";
};

RecordingStorageInfo getRecordingStorageInfo() {
    RecordingStorageInfo info;
    if (SD_MMC.cardType() != CARD_NONE) {
        info.storage = RecordingStorage::SD_CARD;
        info.fs = &SD_MMC;
        info.playback_prefix = "/sd";
        info.label = "SD card";
    } else {
        info.storage = RecordingStorage::LITTLEFS;
        info.fs = &LittleFS;
        info.playback_prefix = "";
        info.label = "LittleFS";
    }
    return info;
}

bool ensureRecordingDirectory(const RecordingStorageInfo& info) {
    if (!info.fs) {
        Logger::getInstance().error("[MicTest] No filesystem available for recordings");
        return false;
    }

    if (info.fs->exists(info.directory)) {
        return true;
    }

    if (!info.fs->mkdir(info.directory)) {
        Logger::getInstance().errorf("[MicTest] Failed to create %s on %s", info.directory, info.label);
        return false;
    }
    return true;
}

std::string normalizeRelativePath(const char* raw_name, const RecordingStorageInfo& info) {
    std::string path = raw_name ? raw_name : "";
    if (path.empty()) {
        return path;
    }

    if (path.front() != '/') {
        std::string base = info.directory ? info.directory : "";
        if (!base.empty() && base.back() != '/') {
            base += '/';
        }
        path = base + path;
    }
    return path;
}

std::string buildPlaybackPath(const RecordingStorageInfo& info, const std::string& relative_path) {
    if (info.playback_prefix && info.playback_prefix[0] != '\0') {
        return std::string(info.playback_prefix) + relative_path;
    }
    return relative_path;
}

std::string playbackDisplayName(const std::string& playback_path) {
    if (playback_path.empty()) {
        return "";
    }
    std::string display = playback_path;
    if (display.rfind("/sd/", 0) == 0) {
        display.erase(0, 3);
    }
    size_t slash = display.find_last_of('/');
    if (slash != std::string::npos && slash + 1 < display.size()) {
        display = display.substr(slash + 1);
    }
    return display;
}

void cleanupFileButtonUserData(lv_event_t* e) {
    if (!e) return;
    if (lv_event_get_code(e) != LV_EVENT_DELETE) {
        return;
    }
    lv_obj_t* target = lv_event_get_target(e);
    if (!target) return;
    auto* path_ptr = static_cast<std::string*>(lv_obj_get_user_data(target));
    delete path_ptr;
    lv_obj_set_user_data(target, nullptr);
}

// WAV file header structure
struct WAVHeader {
    char riff[4] = {'R', 'I', 'F', 'F'};
    uint32_t fileSize = 0;
    char wave[4] = {'W', 'A', 'V', 'E'};
    char fmt[4] = {'f', 'm', 't', ' '};
    uint32_t formatLength = 16;
    uint16_t formatType = 1; // PCM
    uint16_t channels = 1;
    uint32_t sampleRate = 16000;
    uint32_t bytesPerSecond = 32000; // sampleRate * channels * bitsPerSample / 8
    uint16_t blockAlign = 2; // channels * bitsPerSample / 8
    uint16_t bitsPerSample = 16;
    char data[4] = {'d', 'a', 't', 'a'};
    uint32_t dataSize = 0;
};

lv_obj_t* create_fixed_card(lv_obj_t* parent, const char* title, lv_color_t bg_color = lv_color_hex(0x10182c)) {
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_remove_style_all(card);
    lv_obj_set_width(card, lv_pct(100));
    lv_obj_set_height(card, LV_SIZE_CONTENT);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(card, bg_color, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, 14, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_outline_width(card, 0, 0);
    lv_obj_set_style_pad_all(card, 12, 0);
    lv_obj_set_layout(card, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(card, 4, 0);

    lv_obj_t* header = lv_label_create(card);
    lv_label_set_text(header, title);
    lv_obj_set_style_text_font(header, &lv_font_montserrat_16, 0);
    lv_color_t text_color = ColorUtils::invertColor(bg_color);
    lv_obj_set_style_text_color(header, text_color, 0);

    return card;
}

lv_obj_t* create_button(lv_obj_t* parent, const char* text, lv_color_t bg_color) {
    lv_obj_t* btn = lv_btn_create(parent);
    lv_obj_set_height(btn, 50);
    lv_obj_set_style_bg_color(btn, bg_color, 0);
    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_center(lbl);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xffffff), 0);
    return btn;
}

bool parseRecordingIndex(const std::string& path, uint32_t& out_index) {
    if (path.empty()) {
        return false;
    }

    size_t slash = path.find_last_of('/');
    size_t name_start = (slash == std::string::npos) ? 0 : slash + 1;
    constexpr const char* kPrefix = "test_";
    constexpr const char* kSuffix = ".wav";

    if (path.size() < name_start + strlen(kPrefix) + strlen(kSuffix)) {
        return false;
    }

    if (path.compare(name_start, strlen(kPrefix), kPrefix) != 0) {
        return false;
    }

    size_t suffix_pos = path.rfind(kSuffix);
    if (suffix_pos == std::string::npos || suffix_pos <= name_start + strlen(kPrefix)) {
        return false;
    }

    std::string number_str = path.substr(name_start + strlen(kPrefix),
                                         suffix_pos - (name_start + strlen(kPrefix)));
    if (number_str.empty()) {
        return false;
    }

    char* end_ptr = nullptr;
    long value = strtol(number_str.c_str(), &end_ptr, 10);
    if (!end_ptr || *end_ptr != '\0' || value < 0) {
        return false;
    }

    out_index = static_cast<uint32_t>(value);
    return true;
}

uint32_t findNextRecordingIndex(const RecordingStorageInfo& storage) {
    if (!storage.fs || !ensureRecordingDirectory(storage)) {
        return 0;
    }

    uint32_t max_index = 0;
    bool found_any = false;

    File dir = storage.fs->open(storage.directory);
    if (dir && dir.isDirectory()) {
        File entry = dir.openNextFile();
        while (entry) {
            if (!entry.isDirectory()) {
                uint32_t idx = 0;
                if (parseRecordingIndex(entry.name(), idx)) {
                    if (!found_any || idx > max_index) {
                        max_index = idx;
                        found_any = true;
                    }
                }
            }
            entry = dir.openNextFile();
        }
        dir.close();
    }

    if (!found_any) {
        return 0;
    }
    return max_index + 1;
}

// Generate a unique filename for recording
std::string generateRecordingFilename(const RecordingStorageInfo& storage) {
    uint32_t next_index = findNextRecordingIndex(storage);
    std::string directory = (storage.directory && storage.directory[0] != '\0')
                                ? storage.directory
                                : kRecordingsDir;
    if (!directory.empty() && directory.back() == '/') {
        directory.pop_back();
    }

    char filename[48];
    snprintf(filename, sizeof(filename), "%s/test_%06u.wav", directory.c_str(), next_index);
    return filename;
}

bool recordAudioToFile(const RecordingStorageInfo& storage,
                       const char* filename,
                       uint32_t duration_seconds,
                       std::atomic<bool>& stop_flag,
                       const std::function<void(uint16_t)>& level_callback);

// Simple audio recording function (runs in dedicated task)
bool recordAudioToFile(const RecordingStorageInfo& storage,
                       const char* filename,
                       uint32_t duration_seconds,
                       std::atomic<bool>& stop_flag,
                       const std::function<void(uint16_t)>& level_callback) {
    if (!ensureRecordingDirectory(storage) || !storage.fs) {
        Logger::getInstance().error("[MicTest] Recording directory unavailable");
        return false;
    }

    // Open file for writing
    File file = storage.fs->open(filename, FILE_WRITE);
    if (!file) {
        Logger::getInstance().errorf("[MicTest] Failed to open recording file on %s", storage.label);
        return false;
    }

    Logger::getInstance().infof("[MicTest] Recording to %s:%s", storage.label, filename);

    // Write initial WAV header (will be updated with correct size at end)
    WAVHeader initial_header;
    file.write((uint8_t*)&initial_header, sizeof(WAVHeader));

    // Configure I2S for microphone input (same as VoiceAssistant)
    const i2s_config_t i2s_config = {
        .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = kMicSampleRate,
        .bits_per_sample = i2s_bits_per_sample_t(16),
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 4,
        .dma_buf_len = 512,
        .use_apll = false
    };

    const i2s_pin_config_t pin_config = {
        .mck_io_num = kMicI2sMckPin,
        .bck_io_num = kMicI2sBckPin,
        .ws_io_num = kMicI2sWsPin,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = kMicI2sDinPin
    };

    esp_err_t err = i2s_driver_install(I2S_NUM_1, &i2s_config, 0, nullptr);
    if (err != ESP_OK) {
        Logger::getInstance().errorf("I2S install failed: %d", err);
        file.close();
        return false;
    }

    err = i2s_set_pin(I2S_NUM_1, &pin_config);
    if (err != ESP_OK) {
        Logger::getInstance().errorf("I2S set pin failed: %d", err);
        i2s_driver_uninstall(I2S_NUM_1);
        file.close();
        return false;
    }
    i2s_set_clk(I2S_NUM_1, i2s_config.sample_rate, i2s_config.bits_per_sample, I2S_CHANNEL_MONO);

    // Buffer for audio data (mono frames)
    constexpr size_t kSamplesPerChunk = 2048;
    constexpr float kTargetPeak = 30000.0f;
    constexpr float kMaxGainFactor = 12.0f;
    uint8_t* buffer = static_cast<uint8_t*>(malloc(kSamplesPerChunk * sizeof(int16_t)));
    if (!buffer) {
        Logger::getInstance().error("Failed to allocate audio buffer");
        i2s_driver_uninstall(I2S_NUM_1);
        file.close();
        return false;
    }

    const uint32_t target_duration_ms = duration_seconds * 1000;
    const bool limit_by_duration = duration_seconds > 0;
    uint32_t start_ms = millis();

    uint32_t total_bytes = 0;
    uint64_t recorded_samples = 0;

    Logger::getInstance().info("Starting audio recording");

    while (true) {
        if (stop_flag.load()) {
            break;
        }
        uint32_t elapsed_ms = millis() - start_ms;
        if (limit_by_duration && elapsed_ms >= target_duration_ms) {
            break;
        }

        size_t bytes_read = 0;
        err = i2s_read(I2S_NUM_1,
                       buffer,
                       kSamplesPerChunk * sizeof(int16_t),
                       &bytes_read,
                       pdMS_TO_TICKS(100));

        if (err == ESP_OK && bytes_read >= sizeof(int16_t)) {
            int16_t* samples = reinterpret_cast<int16_t*>(buffer);
            size_t sample_count = bytes_read / sizeof(int16_t);
            size_t mono_bytes = sample_count * sizeof(int16_t);
            int32_t chunk_peak = 0;

            for (size_t i = 0; i < sample_count; ++i) {
                int32_t value = std::abs(samples[i]);
                if (value > chunk_peak) {
                    chunk_peak = value;
                }
            }

            float gain = 1.0f;
            if (chunk_peak > 0 && chunk_peak < kTargetPeak) {
                gain = std::min<float>(kMaxGainFactor, kTargetPeak / static_cast<float>(chunk_peak));
            }

            int32_t scaled_peak = chunk_peak;
            if (gain != 1.0f) {
                scaled_peak = 0;
                constexpr int32_t kInt16Min = std::numeric_limits<int16_t>::min();
                constexpr int32_t kInt16Max = std::numeric_limits<int16_t>::max();
                for (size_t i = 0; i < sample_count; ++i) {
                    float scaled_value = samples[i] * gain;
                    int32_t scaled_int = static_cast<int32_t>(scaled_value);
                    if (scaled_int < kInt16Min) {
                        scaled_int = kInt16Min;
                    } else if (scaled_int > kInt16Max) {
                        scaled_int = kInt16Max;
                    }
                    samples[i] = static_cast<int16_t>(scaled_int);
                    int32_t abs_value = std::abs(scaled_int);
                    if (abs_value > scaled_peak) {
                        scaled_peak = abs_value;
                    }
                }
            }

            file.write(reinterpret_cast<uint8_t*>(samples), mono_bytes);
            recorded_samples += sample_count;
            total_bytes += mono_bytes;

            if (level_callback) {
                uint16_t level = static_cast<uint16_t>(std::min<int>(100, (scaled_peak * 100) / 32767));
                level_callback(level);
            }
        } else {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }

    if (level_callback) {
        level_callback(0);
    }

    // Cleanup
    free(buffer);
    i2s_driver_uninstall(I2S_NUM_1);

    uint32_t recording_duration_ms = millis() - start_ms;
    if (recording_duration_ms == 0) {
        recording_duration_ms = 1;
    }

    uint32_t configured_sample_rate = i2s_config.sample_rate;
    uint32_t candidate_rate = recorded_samples > 0
                                  ? static_cast<uint32_t>((recorded_samples * 1000ull) / recording_duration_ms)
                                  : 0;
    uint32_t measured_sample_rate = configured_sample_rate;

    Logger::getInstance().infof("Recording complete: %u bytes, %llu samples in %ums (~%u Hz) stored as %u Hz",
                                total_bytes,
                                static_cast<unsigned long long>(recorded_samples),
                                recording_duration_ms,
                                candidate_rate,
                                measured_sample_rate);

    // Update WAV header with actual data size
    if (total_bytes > 0) {
        WAVHeader header;
        header.sampleRate = measured_sample_rate;
        header.bytesPerSecond = header.sampleRate * header.channels * header.bitsPerSample / 8;
        header.blockAlign = header.channels * (header.bitsPerSample / 8);
        header.dataSize = total_bytes;
        header.fileSize = 36 + total_bytes; // 44 - 8 = 36

        file.seek(0);
        file.write((uint8_t*)&header, sizeof(WAVHeader));
    }

    file.close();
    return total_bytes > 0;
}
} // namespace

void MicrophoneTestScreen::recordingTask(void* param) {
    auto* screen = static_cast<MicrophoneTestScreen*>(param);
    if (!screen) {
        vTaskDelete(nullptr);
        return;
    }

    auto storage = getRecordingStorageInfo();
    if (!storage.fs || !ensureRecordingDirectory(storage)) {
        Logger::getInstance().error("[MicTest] Unable to access recording storage");
        if (screen->record_status_label) {
            lv_label_set_text(screen->record_status_label, "Storage non disponibile");
        }
        screen->is_recording = false;
        screen->stop_recording_requested.store(false);
        screen->recording_task_handle = nullptr;
        screen->applyThemeStyles(SettingsManager::getInstance().getSnapshot());
        screen->updateMicLevelIndicator(0);
        vTaskDelete(nullptr);
        return;
    }

    std::string filename = generateRecordingFilename(storage);
    if (storage.storage == RecordingStorage::LITTLEFS) {
        Logger::getInstance().warn("[MicTest] SD card unavailable, recording to LittleFS");
    }

    auto level_callback = [screen](uint16_t level) {
        screen->updateMicLevelIndicator(level);
    };

    bool success = recordAudioToFile(storage,
                                     filename.c_str(),
                                     kDefaultRecordingDurationSeconds,
                                     screen->stop_recording_requested,
                                     level_callback);
    bool cancelled = screen->stop_recording_requested.load();
    screen->stop_recording_requested.store(false);
    if (success) {
        screen->current_playback_file = buildPlaybackPath(storage, filename);
        if (screen->playback_status_label) {
            std::string status = "Selected: " + playbackDisplayName(screen->current_playback_file);
            lv_label_set_text(screen->playback_status_label, status.c_str());
        }
    }

    if (screen->record_status_label) {
        if (!success) {
            lv_label_set_text(screen->record_status_label, "Recording failed!");
        } else if (cancelled) {
            lv_label_set_text(screen->record_status_label, "Recording stopped");
        } else {
            lv_label_set_text(screen->record_status_label, "Recording saved!");
        }
    }

    screen->is_recording = false;
    screen->updateMicLevelIndicator(0);
    screen->recording_task_handle = nullptr;
    screen->applyThemeStyles(SettingsManager::getInstance().getSnapshot());
    screen->refreshAudioFilesList();

    Logger::getInstance().infof("Recording task completed, success: %d (cancelled=%d)", success, cancelled);
    vTaskDelete(nullptr);
}

MicrophoneTestScreen::~MicrophoneTestScreen() {
    if (settings_listener_id != 0) {
        SettingsManager::getInstance().removeListener(settings_listener_id);
        settings_listener_id = 0;
    }
}

void MicrophoneTestScreen::build(lv_obj_t* parent) {
    if (!parent) {
        return;
    }

    SettingsManager& manager = SettingsManager::getInstance();
    const SettingsSnapshot& snapshot = manager.getSnapshot();

    root = lv_obj_create(parent);
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(root, lv_color_hex(0x040b18), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    lv_obj_add_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(root, LV_DIR_VER);
    lv_obj_set_layout(root, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(root, 6, 0);
    lv_obj_set_style_pad_row(root, 8, 0);
    lv_obj_set_style_border_width(root, 0, 0);
    lv_obj_set_style_outline_width(root, 0, 0);

    title_label = lv_label_create(root);
    lv_label_set_text(title_label, LV_SYMBOL_AUDIO " Microphone Test");
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(title_label, lv_color_hex(0xffffff), 0);
    lv_obj_set_width(title_label, lv_pct(100));

    // Recording card
    record_card = create_fixed_card(root, "Recording Test");

    lv_obj_t* controls_row = lv_obj_create(record_card);
    lv_obj_remove_style_all(controls_row);
    lv_obj_set_width(controls_row, lv_pct(100));
    lv_obj_set_height(controls_row, LV_SIZE_CONTENT);
    lv_obj_set_layout(controls_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(controls_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(controls_row, 8, 0);

    record_start_button = create_button(controls_row, LV_SYMBOL_AUDIO " Avvia", lv_color_hex(0xff4444));
    lv_obj_set_flex_grow(record_start_button, 1);
    lv_obj_add_event_cb(record_start_button, handleRecordStartButton, LV_EVENT_CLICKED, this);

    record_stop_button = create_button(controls_row, LV_SYMBOL_STOP " Stop", lv_color_hex(0x2a3a4a));
    lv_obj_set_flex_grow(record_stop_button, 1);
    lv_obj_add_event_cb(record_stop_button, handleRecordStopButton, LV_EVENT_CLICKED, this);

    record_status_label = lv_label_create(record_card);
    lv_label_set_text(record_status_label, "Premi Avvia per registrare");
    lv_obj_set_style_text_font(record_status_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(record_status_label, lv_color_hex(0xa0a0a0), 0);

    mic_level_arc = lv_arc_create(record_card);
    lv_obj_set_size(mic_level_arc, 140, 140);
    lv_arc_set_range(mic_level_arc, 0, 100);
    lv_arc_set_value(mic_level_arc, 0);
    lv_arc_set_rotation(mic_level_arc, 270);
    lv_arc_set_bg_angles(mic_level_arc, 0, 360);
    lv_obj_center(mic_level_arc);
    mic_level_label = lv_label_create(mic_level_arc);
    lv_label_set_text(mic_level_label, "0%");
    lv_obj_center(mic_level_label);
    updateMicLevelIndicator(0);

    // Playback card
    playback_card = create_fixed_card(root, "Playback Test");
    playback_button = create_button(playback_card, LV_SYMBOL_PLAY " Play Test Audio", lv_color_hex(0x44aa44));
    lv_obj_add_event_cb(playback_button, handlePlaybackButton, LV_EVENT_CLICKED, this);
    playback_status_label = lv_label_create(playback_card);
    lv_label_set_text(playback_status_label, "No audio file to play");
    lv_obj_set_style_text_font(playback_status_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(playback_status_label, lv_color_hex(0xa0a0a0), 0);

    // Saved files card
    files_card = create_fixed_card(root, "Saved Audio Files");
    files_container = lv_obj_create(files_card);
    lv_obj_remove_style_all(files_container);
    lv_obj_set_size(files_container, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_layout(files_container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(files_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(files_container, 8, 0);

    refreshAudioFilesList();

    applySnapshot(snapshot);

    if (settings_listener_id == 0) {
        settings_listener_id = manager.addListener([this](SettingsManager::SettingKey, const SettingsSnapshot& snap) {
            if (!root) return;
            applySnapshot(snap);
        });
    }
}

void MicrophoneTestScreen::onShow() {
    Logger::getInstance().info(LV_SYMBOL_AUDIO " Microphone test screen shown");
    applySnapshot(SettingsManager::getInstance().getSnapshot());
    refreshAudioFilesList();
}

void MicrophoneTestScreen::onHide() {
    Logger::getInstance().info(LV_SYMBOL_AUDIO " Microphone test screen hidden");
    if (is_recording) {
        requestStopRecording();
    }
}

void MicrophoneTestScreen::applySnapshot(const SettingsSnapshot& snapshot) {
    updating_from_manager = true;
    applyThemeStyles(snapshot);
    updating_from_manager = false;
}

void MicrophoneTestScreen::applyThemeStyles(const SettingsSnapshot& snapshot) {
    lv_color_t primary = lv_color_hex(snapshot.primaryColor);
    lv_color_t accent = lv_color_hex(snapshot.accentColor);

    if (root) {
        lv_obj_set_style_bg_color(root, primary, 0);
    }
    if (title_label) {
        lv_obj_set_style_text_color(title_label, accent, 0);
    }

    auto style_card = [&](lv_obj_t* card) {
        if (!card) return;
        lv_color_t card_color = lv_color_hex(snapshot.cardColor);
        lv_obj_set_style_bg_color(card, card_color, 0);
        lv_obj_set_style_radius(card, snapshot.borderRadius, 0);

        uint32_t child_count = lv_obj_get_child_cnt(card);
        for (uint32_t i = 0; i < child_count; ++i) {
            lv_obj_t* child = lv_obj_get_child(card, i);
            if (child && lv_obj_check_type(child, &lv_label_class)) {
                const lv_font_t* font = lv_obj_get_style_text_font(child, 0);
                if (font == &lv_font_montserrat_16) {
                    lv_color_t header_color = ColorUtils::invertColor(card_color);
                    lv_obj_set_style_text_color(child, header_color, 0);
                } else {
                    lv_obj_set_style_text_color(child, lv_color_mix(accent, lv_color_hex(0xffffff), LV_OPA_40), 0);
                }
            }
        }
    };

    style_card(record_card);
    style_card(playback_card);
    style_card(files_card);

    if (record_start_button) {
        lv_color_t button_color = is_recording ? lv_color_hex(0x555555) : lv_color_hex(0xff4444);
        lv_obj_set_style_bg_color(record_start_button, button_color, 0);
        if (is_recording) {
            lv_obj_add_state(record_start_button, LV_STATE_DISABLED);
        } else {
            lv_obj_clear_state(record_start_button, LV_STATE_DISABLED);
        }
        lv_obj_t* btn_label = lv_obj_get_child(record_start_button, 0);
        if (btn_label) {
            lv_label_set_text(btn_label, LV_SYMBOL_AUDIO " Avvia");
        }
    }
    if (record_stop_button) {
        lv_color_t button_color = is_recording ? lv_color_hex(0x2266aa) : lv_color_hex(0x2a3a4a);
        lv_obj_set_style_bg_color(record_stop_button, button_color, 0);
        if (is_recording) {
            lv_obj_clear_state(record_stop_button, LV_STATE_DISABLED);
        } else {
            lv_obj_add_state(record_stop_button, LV_STATE_DISABLED);
        }
        lv_obj_t* btn_label = lv_obj_get_child(record_stop_button, 0);
        if (btn_label) {
            lv_label_set_text(btn_label, LV_SYMBOL_STOP " Stop");
        }
    }
    if (mic_level_arc) {
        lv_obj_set_style_arc_color(mic_level_arc, lv_color_mix(accent, lv_color_hex(0x000000), LV_OPA_80), LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(mic_level_arc, lv_color_mix(accent, primary, LV_OPA_20), LV_PART_MAIN);
    }
    if (mic_level_label) {
        lv_obj_set_style_text_color(mic_level_label, lv_color_hex(0xffffff), 0);
    }
    if (playback_button) {
        lv_color_t button_color = is_playing ? lv_color_hex(0x448844) : lv_color_hex(0x44aa44);
        lv_obj_set_style_bg_color(playback_button, button_color, 0);
        lv_obj_t* btn_label = lv_obj_get_child(playback_button, 0);
        if (btn_label) {
            lv_label_set_text(btn_label, is_playing ? LV_SYMBOL_PAUSE " Stop Playback" : LV_SYMBOL_PLAY " Play Test Audio");
        }
    }
}

void MicrophoneTestScreen::updateMicLevelIndicator(uint16_t level) {
    uint16_t value = std::min<uint16_t>(level, 100);
    if (mic_level_arc) {
        lv_arc_set_value(mic_level_arc, value);
    }
    if (mic_level_label) {
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "%u%%", value);
        lv_label_set_text(mic_level_label, buffer);
    }
}

void MicrophoneTestScreen::requestStopRecording() {
    if (!is_recording) {
        if (record_status_label) {
            lv_label_set_text(record_status_label, "Nessuna registrazione attiva");
        }
        return;
    }
    stop_recording_requested.store(true);
    if (record_status_label) {
        lv_label_set_text(record_status_label, "Arresto registrazione...");
    }
    Logger::getInstance().info("[MicTest] Stop requested");
}

void MicrophoneTestScreen::refreshAudioFilesList() {
    if (!files_container) return;

    lv_obj_clean(files_container);

    struct AudioFileEntry {
        std::string display_name;
        std::string playback_path;
    };

    std::vector<AudioFileEntry> audio_files;
    auto storage = getRecordingStorageInfo();

    if (!storage.fs || !ensureRecordingDirectory(storage)) {
        lv_obj_t* no_storage = lv_label_create(files_container);
        lv_label_set_text(no_storage, "Recording storage not available");
        lv_obj_set_style_text_color(no_storage, lv_color_hex(0xff6666), 0);
        current_playback_file.clear();
        if (playback_status_label) {
            lv_label_set_text(playback_status_label, "No audio file to play");
        }
        return;
    }

    File root_dir = storage.fs->open(storage.directory);
    if (root_dir && root_dir.isDirectory()) {
        File file = root_dir.openNextFile();
        while (file) {
            if (!file.isDirectory()) {
                std::string name = file.name();
                std::string relative_path = normalizeRelativePath(name.c_str(), storage);
                if (relative_path.find(".wav") != std::string::npos) {
                    AudioFileEntry entry;
                    entry.playback_path = buildPlaybackPath(storage, relative_path);
                    entry.display_name = playbackDisplayName(entry.playback_path);
                    audio_files.push_back(std::move(entry));
                }
            }
            file = root_dir.openNextFile();
        }
        root_dir.close();
    }

    if (audio_files.empty()) {
        lv_obj_t* no_files = lv_label_create(files_container);
        lv_label_set_text(no_files, "No recorded audio files found");
        lv_obj_set_style_text_color(no_files, lv_color_hex(0xc0c0c0), 0);
        current_playback_file.clear();
        if (playback_status_label) {
            lv_label_set_text(playback_status_label, "No audio file to play");
        }
    } else {
        bool selection_valid = false;
        for (const auto& entry : audio_files) {
            if (entry.playback_path == current_playback_file) {
                selection_valid = true;
                break;
            }
        }
        if (!selection_valid) {
            current_playback_file.clear();
            if (playback_status_label) {
                lv_label_set_text(playback_status_label, "Select a recording to play");
            }
        }

        for (const auto& filename : audio_files) {
            lv_obj_t* file_btn = lv_btn_create(files_container);
            lv_obj_set_height(file_btn, 40);
            lv_obj_set_width(file_btn, lv_pct(100));
            lv_obj_set_style_bg_color(file_btn, lv_color_hex(0x2a3a4a), 0);
            lv_obj_set_style_radius(file_btn, 8, 0);

            lv_obj_t* btn_label = lv_label_create(file_btn);
            std::string btn_text = std::string(LV_SYMBOL_FILE) + " " + filename.display_name;
            lv_label_set_text(btn_label, btn_text.c_str());
            lv_obj_set_style_text_font(btn_label, &lv_font_montserrat_14, 0);
            lv_obj_set_style_text_color(btn_label, lv_color_hex(0xffffff), 0);
            lv_obj_set_align(btn_label, LV_ALIGN_LEFT_MID);

            // Store filename in user data for callback
            lv_obj_set_user_data(file_btn, new std::string(filename.playback_path));

            lv_obj_add_event_cb(file_btn, handleAudioFileButton, LV_EVENT_CLICKED, this);
            lv_obj_add_event_cb(file_btn, cleanupFileButtonUserData, LV_EVENT_DELETE, nullptr);
        }
    }
}

void MicrophoneTestScreen::handleRecordStartButton(lv_event_t* e) {
    auto* screen = static_cast<MicrophoneTestScreen*>(lv_event_get_user_data(e));
    if (!screen || screen->is_recording) return;

    screen->is_recording = true;
    screen->stop_recording_requested.store(false);
    screen->updateMicLevelIndicator(0);
    if (screen->record_status_label) {
        lv_label_set_text(screen->record_status_label, "Registrazione in corso...");
    }
    Logger::getInstance().info("Starting microphone recording task");

    xTaskCreate(MicrophoneTestScreen::recordingTask,
                "mic_recording",
                4096,
                screen,
                1,
                &screen->recording_task_handle);

    screen->applyThemeStyles(SettingsManager::getInstance().getSnapshot());
}

void MicrophoneTestScreen::handleRecordStopButton(lv_event_t* e) {
    auto* screen = static_cast<MicrophoneTestScreen*>(lv_event_get_user_data(e));
    if (!screen) return;
    screen->requestStopRecording();
    screen->applyThemeStyles(SettingsManager::getInstance().getSnapshot());
}

void MicrophoneTestScreen::handlePlaybackButton(lv_event_t* e) {
    auto* screen = static_cast<MicrophoneTestScreen*>(lv_event_get_user_data(e));
    if (!screen || screen->current_playback_file.empty()) return;

    if (screen->is_playing) {
        // Stop playback
        AudioManager::getInstance().stop();
        screen->is_playing = false;
        if (screen->playback_status_label) lv_label_set_text(screen->playback_status_label, "Playback stopped");
        Logger::getInstance().info("Audio playback stopped");
    } else {
        // Start playback
        bool started = AudioManager::getInstance().playFile(screen->current_playback_file.c_str());
        screen->is_playing = started;
        if (screen->playback_status_label) {
            lv_label_set_text(screen->playback_status_label,
                              started ? "Playing..." : "Playback failed");
        }
        if (started) {
            Logger::getInstance().info("Audio playback started");
        } else {
            Logger::getInstance().error("Failed to start audio playback");
        }
    }

    screen->applyThemeStyles(SettingsManager::getInstance().getSnapshot());
}

void MicrophoneTestScreen::handleAudioFileButton(lv_event_t* e) {
    auto* screen = static_cast<MicrophoneTestScreen*>(lv_event_get_user_data(e));
    if (!screen) return;

    auto* filename_ptr = static_cast<std::string*>(lv_obj_get_user_data(lv_event_get_target(e)));
    if (!filename_ptr) return;

    screen->current_playback_file = *filename_ptr;
    std::string status = "Selected: " + playbackDisplayName(*filename_ptr);
    if (screen->playback_status_label) lv_label_set_text(screen->playback_status_label, status.c_str());

    Logger::getInstance().infof("Selected audio file: %s", filename_ptr->c_str());
}
