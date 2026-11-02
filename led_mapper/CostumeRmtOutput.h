#pragma once

#include <Arduino.h>
#include <driver/rmt_tx.h>
#include <driver/rmt_encoder.h>
#include <esp_arduino_version.h>

#if ESP_ARDUINO_VERSION_MAJOR < 3 || !CONFIG_IDF_TARGET_ESP32
#error "This driver requires Arduino-ESP32 3.x and a classic ESP32 (such as ESP32-WROOM-32D)."
#endif
#if ARDUINO_RUNNING_CORE != 1
#error "Select Arduino Runs On: Core 1 so LED interrupts are separate from the default Wi-Fi task on Core 0."
#endif

// Mapper-only driver: fixed transmitters share one symbol buffer.
// service() waits for completion before another output may reuse that buffer.
// No NeoPixel show(), Arduino RMT pin cache, or GPIO remapping is involved.
template <size_t PixelCount>
class CostumeRmtOutput {
public:
  CostumeRmtOutput() = default;
  CostumeRmtOutput(const CostumeRmtOutput&) = delete;
  CostumeRmtOutput& operator=(const CostumeRmtOutput&) = delete;

  bool begin(uint8_t pin) {
    rmt_tx_channel_config_t config = {};
    config.gpio_num = static_cast<gpio_num_t>(pin);
    config.clk_src = RMT_CLK_SRC_DEFAULT;
    config.resolution_hz = 10000000;  // 100 ns per tick
    config.mem_block_symbols = 64; // Six outputs use six of eight classic ESP32 blocks.
    config.trans_queue_depth = 1;
    config.intr_priority = 3;
    error_ = rmt_new_tx_channel(&config, &channel_);
    if (error_ != ESP_OK) return false;

    rmt_copy_encoder_config_t encoderConfig = {};
    error_ = rmt_new_copy_encoder(&encoderConfig, &encoder_);
    if (error_ == ESP_OK) error_ = rmt_enable(channel_);
    if (error_ != ESP_OK) {
      if (encoder_) { rmt_del_encoder(encoder_); encoder_ = nullptr; }
      rmt_del_channel(channel_);
      channel_ = nullptr;
      return false;
    }
    ready_ = true;
    return true;
  }

  void markDirty() { dirty_ = true; }
  bool ready() const { return ready_; }
  esp_err_t error() const { return error_; }
  uint32_t frames() const { return frames_; }

  // Called on the Arduino loop task, the same core that allocated the channels.
  // Application pixels can change at any time; the in-flight snapshot cannot.
  void service(const uint8_t* grb) {
    if (!ready_ || sharedFailed_) return;
    if (!dirty_) return;
    if (!grb) { fail(ESP_ERR_NO_MEM); return; }

    size_t symbol = 0;
    for (size_t byte = 0; byte < PixelCount * 3; byte++) {
      for (uint8_t mask = 0x80; mask != 0; mask >>= 1) {
        bool one = (grb[byte] & mask) != 0;
        symbols_[symbol].level0 = 1;
        symbols_[symbol].duration0 = one ? 8 : 4;
        symbols_[symbol].level1 = 0;
        symbols_[symbol].duration1 = one ? 4 : 8;
        symbol++;
      }
    }
    // Explicit 300 us reset/latch, included in the completion check.
    symbols_[symbol].level0 = 0;
    symbols_[symbol].duration0 = 1500;
    symbols_[symbol].level1 = 0;
    symbols_[symbol].duration1 = 1500;

    rmt_transmit_config_t config = {};
    config.flags.eot_level = 0;
    error_ = rmt_transmit(channel_, encoder_, symbols_, sizeof(symbols_), &config);
    if (error_ != ESP_OK) { fail(error_); return; }
    dirty_ = false;
    // Keep the shared symbols immutable until the peripheral finishes.
    error_ = rmt_tx_wait_all_done(channel_, -1);
    if (error_ != ESP_OK) { sharedFailed_ = true; fail(error_); return; }
    frames_++;
  }

private:
  void fail(esp_err_t error) { error_ = error; ready_ = false; }
  rmt_channel_handle_t channel_ = nullptr;
  rmt_encoder_handle_t encoder_ = nullptr;
  inline static rmt_symbol_word_t symbols_[PixelCount * 24 + 1] = {};
  inline static bool sharedFailed_ = false;
  bool ready_ = false;
  bool dirty_ = true;
  esp_err_t error_ = ESP_OK;
  uint32_t frames_ = 0;
};
