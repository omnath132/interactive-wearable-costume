#pragma once
#include <stdint.h>

// Five accepted beats define four intervals. Keep the last good rhythm
// across missing samples; only incoming detected beats can update it.
class HeartRhythm {
public:
  uint32_t periodMs = 0;
  uint32_t rejected = 0;
  void breakTracking() { haveAnchor = false; }
  float bpm() const { return periodMs ? 60000.0f / periodMs : 0; }
  bool beat(uint32_t timeMs) {
    if (!haveAnchor) { anchor = timeMs; haveAnchor = true; return false; }
    const uint32_t interval = timeMs - anchor;
    // Ignore extra early detections without moving the last-beat anchor.
    if (interval < 333 || (periodMs && interval < periodMs * 0.65f)) {
      rejected++; return false;
    }
    anchor = timeMs;
    // 35..180 BPM, and at most 35% away from the established rhythm.
    // A missed beat/dropout reanchors timing but does not change the average.
    if (interval > 1714 || (periodMs && interval > periodMs * 1.35f)) {
      rejected++; return false;
    }
    intervals[next] = interval;
    next = (next + 1) % 4;
    if (count < 4) count++;
    uint32_t sum = 0;
    for (unsigned i = 0; i < count; i++) sum += intervals[i];
    periodMs = sum / count;
    return true;
  }
private:
  uint32_t intervals[4] = {}, anchor = 0;
  unsigned next = 0, count = 0;
  bool haveAnchor = false;
};
