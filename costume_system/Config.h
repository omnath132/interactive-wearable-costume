#pragma once
#include <stdint.h>
namespace costume {
constexpr unsigned SECTIONS = 6, MAX_PIXELS = 124, MAX_PULSES = 24;
enum Section { LEFT_ARM, RIGHT_ARM, LEFT_LEG, RIGHT_LEG, SPINE, HEART };
// Measured with led_mapper; see measured-led-mapping.json (1-based inclusive).
constexpr unsigned LENGTHS[SECTIONS] = {118,124,70,70,97,18};
constexpr const char* NAMES[SECTIONS] = {"Left arm","Right arm","Left leg","Right leg","Spine","Heart"};
// User-confirmed groups: spine 4; arms/heart 16/17/18; legs 32/33.
// Output labels and lengths imported from the saved mapper file.
constexpr int LED_PINS[SECTIONS] = {16,17,32,33,4,18};
constexpr bool REVERSE[SECTIONS] = {false,false,false,false,false,false};
// LED indices follow physical data-input order. Direction/junction geometry is provisional.
// Arm linkage enters the final 12 spine LEDs and continues toward higher indices.
constexpr unsigned JUNCTION[4] = {LENGTHS[SPINE]-12,LENGTHS[SPINE]-12,0,0};
constexpr bool ENABLE_LEDS = true, ENABLE_SENSORS = true;
// Separate ESP32 hardware I2C controllers, one mux per controller.
constexpr int SDA_PINS[2] = {21,26}, SCL_PINS[2] = {22,25};
constexpr int SDA_PIN = SDA_PINS[0], SCL_PIN = SCL_PINS[0];
constexpr int MUX_ADDRESS[2] = {-1,-1}; // -1 discovers a responding mux at 0x70..0x77.
struct Branch { int mux, channel, address; };
// User confirmed channel 6 = left arm, channel 7 = right arm. Heart channel 2.
constexpr Branch TOUCH[2] = {{0,6,0x5A},{0,7,0x5A}};
constexpr Branch HEART_BUS = {0,2,0x57};
// User-specified IMU channels; discover 0x68/0x69 separately on each branch.
// A is channel 2; B is channel 7. Physical left/right mounting is unassigned.
constexpr Branch MOTION[2] = {{1,2,-1},{1,7,-1}};
constexpr uint32_t RAW_DIAGNOSTIC_MS = 1000; // 0 disables periodic raw sensor logs.
constexpr float DEFAULT_STEP_THRESHOLD_G = 0.15f; // Excess over learned resting magnitude (normally 1 g).
constexpr float STEP_ROTATION_DEGREES = 6.0f;
constexpr uint32_t STEP_MOTION_WINDOW_MS = 800;
constexpr bool USE_ROUGH_TOUCH = true;
constexpr uint16_t ROUGH_TOUCH_IDLE = 6, ROUGH_TOUCH_BELOW = 5;
constexpr uint32_t ROUGH_TOUCH_PRESS_MS = 100, ROUGH_TOUCH_RELEASE_MS = 400;
struct Region { int first, last; };
// Zero-based half-open ranges [first,last). {0,0} means unmeasured: log only.
// Touch board channel 6 = left, channel 7 = right (user confirmed).
constexpr Region ELECTRODES[2][12] = {
  {{114,118},{107,111},{102,106},{97,101},{90,95},{0,0},{0,0},{66,71},{0,0},{0,0},{48,54},{40,46}},
  {{117,122},{110,114},{104,108},{97,101},{90,94},{0,0},{0,0},{0,0},{0,0},{61,66},{51,56},{43,48}}
};
constexpr bool ELECTRODES_CONFIRMED = true;
constexpr uint32_t LEASE_MS = 30000, SESSION_MS = 3600000;
constexpr uint32_t MATCH_MS = 140, LIVE_MS = 2500, FADE_MS = 2500;
constexpr uint8_t BRIGHTNESS_CAP = 160;
constexpr uint32_t CHARGE_MS = 2500, GHOST_BREATH_MS = 4500, COLLISION_MS = 320;
constexpr unsigned MAX_FLARES = 8;
constexpr const char* AP_NAME = "Costume";
}
