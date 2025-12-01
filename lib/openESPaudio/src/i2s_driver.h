// Copyright (c) 2025 rederyk
// Licensed under the MIT License. See LICENSE file for details.


#pragma once

#include <cstddef>
#include <cstdint>
#include "driver/i2s.h"
#include "audio_types.h"

class I2sDriver {
public:
    I2sDriver() = default;

    void configure(uint32_t sample_rate,
                   const AudioConfig &cfg,
                   uint32_t bytes_per_sample,
                   uint32_t channels);
    void init(uint32_t sample_rate,
              const AudioConfig &cfg,
              uint32_t bytes_per_sample,
              uint32_t channels,
              int bck_pin,
              int ws_pin,
              int dout_pin,
              int mclk_pin = I2S_PIN_NO_CHANGE,
              i2s_mclk_multiple_t mclk_multiple = I2S_MCLK_MULTIPLE_DEFAULT);
    void uninstall();

    size_t chunk_bytes() const { return chunk_bytes_active_; }
    uint32_t dma_buf_len() const { return dma_buf_len_active_; }
    uint32_t dma_buf_count() const { return dma_buf_count_active_; }
    bool installed() const { return installed_; }

private:
    uint32_t dma_buf_len_active_ = 0;
    uint32_t dma_buf_count_active_ = 0;
    size_t chunk_bytes_active_ = 0;
    bool installed_ = false;
};
