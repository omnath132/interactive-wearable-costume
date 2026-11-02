#pragma once
#include <stdint.h>
#include <math.h>

// Visual beat detection tuned for the configured 25 FIFO samples/second.
// No predicted beats: the LED only fires from a detected IR waveform peak.
class BeatFilter {
public:
  explicit BeatFilter(uint32_t fingerThreshold) : fingerThreshold_(fingerThreshold) {}

  void reset() {
    finger = false; bpm = 0; signal = 0;
    baseline_ = previousRaw_ = envelope_ = 0;
    warmup_ = 0; sampleMs_ = lastBeatMs_ = 0;
    armed_ = true; haveBeat_ = false;
  }

  bool feed(uint32_t ir) {
    if (ir < fingerThreshold_ || ir >= 0x3FFFF) { reset(); return false; }
    if (!finger) {
      finger = true;
      baseline_ = previousRaw_ = ir;
      warmup_ = 25; // let placement transients settle for one second
    }
    sampleMs_ += 40;
    float jump = fabsf(float(ir) - previousRaw_);
    previousRaw_ = ir;
    // Reject abrupt motion/placement changes; learn the new baseline first.
    if (jump > fmaxf(1500.0f, baseline_ * 0.08f)) {
      baseline_ = ir; signal = envelope_ = 0;
      warmup_ = 25; armed_ = true; haveBeat_ = false; bpm = 0;
      return false;
    }
    baseline_ += (float(ir) - baseline_) * 0.05f; // remove slow DC drift
    signal += ((float(ir) - baseline_) - signal) * 0.45f; // smooth noise
    envelope_ = fmaxf(fabsf(signal), envelope_ * 0.97f);
    if (warmup_) { warmup_--; return false; }
    if (haveBeat_ && sampleMs_ - lastBeatMs_ > 2000) { haveBeat_ = false; bpm = 0; }
    float threshold = fmaxf(20.0f, envelope_ * 0.45f);
    if (signal < -threshold * 0.25f) armed_ = true;
    if (!armed_ || signal < threshold) return false;
    armed_ = false;
    uint32_t interval = sampleMs_ - lastBeatMs_;
    if (haveBeat_ && interval < 320) return false; // reject double triggers
    if (haveBeat_) {
      float instantBpm = 60000.0f / interval;
      bpm = bpm == 0 ? instantBpm : bpm * 0.75f + instantBpm * 0.25f;
    }
    haveBeat_ = true;
    lastBeatMs_ = sampleMs_;
    return true;
  }

  bool finger = false;
  float bpm = 0;    // approximate visual feedback, not a calibrated measurement
  float signal = 0;

private:
  uint32_t fingerThreshold_;
  float baseline_ = 0, previousRaw_ = 0, envelope_ = 0;
  uint8_t warmup_ = 0;
  uint32_t sampleMs_ = 0, lastBeatMs_ = 0;
  bool armed_ = true, haveBeat_ = false;
};
