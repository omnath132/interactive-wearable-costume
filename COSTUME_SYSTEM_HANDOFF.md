# ESP32 wearable LED costume: planning handoff

## Instructions for the next model

This document records the user's decisions and the proposed plan from a planning conversation. Focus primarily on firmware and the website. Keep explanations practical and understandable.

- **Do not write implementation code yet unless the user now explicitly asks for implementation.** The original request was planning only.
- Do not ask the user to reconfirm decisions marked agreed. Ask only about unresolved details needed for the next step, in manageable groups, and wait before committing to those choices.
- Keep hardware assignments as explicit placeholders where indicated. Do not silently turn old sketch constants into confirmed wiring.
- The user handles compilation and uploading unless they explicitly ask otherwise.
- Preserve existing standalone test sketches. No implementation files were modified during this planning conversation.
- Proposed choices below are recommendations, not confirmed requirements.

Workspace: `C:\Users\sikap\Downloads\costume code`

## Goal

Plan a complete ESP32 wearable with six separate LED strips, arm touch inputs, heartbeat visualization, left/right footstep effects, and an ESP32-hosted control website with a body diagram and access control.

## Existing project: read before implementation

Review:

- `README.md`
- `heartrate_scan/heartrate_scan.ino`
- `heartrate_scan/TODO.md`
- `heartrate_scan/HeartRhythm.h`
- `heartrate_scan/BeatFilter.h`
- `heartrate_scan/HeartWeb.h`
- `ws2812_wifi_ap/ws2812_wifi_ap.ino`
- `ws2812_wifi_ap/IndependentRmtOutput.h`
- `ws2812_wifi_ap/DRIVER_NOTES.md`
- `ledTest/ledTest.ino`
- `tca9548_alternating_test/tca9548_alternating_test.ino`

Findings from the planning review (recheck files if they have changed):

- The old heart sketch uses SDA 21 / SCL 22, mux address 0x70 channel 7, heart sensor address 0x57, and an 18-LED heart on GPIO 4. These are historical values, not final wiring.
- The old combined touch test uses MPR121 boards at 0x5A on channels 2/3 and the heart sensor on channel 7. The intended new assignment is touch on 6/7 and heart on 2.
- The current heart sketch loops an averaged rhythm independently of new detections. `TODO.md` explicitly calls for immediate real-beat response, prediction only to fill gaps, timing outlier rejection, and duplicate suppression.
- `HeartRhythm.h` stores four intervals corresponding to five accepted beats. Its existing numerical rejection thresholds are implementation history, not agreed final tuning.
- The Wi-Fi LED sketch has three independent outputs on GPIO 16/17/18, each sized for up to 181 pixels. These are not final counts or pin assignments.
- The dedicated RMT driver allocates one persistent transmitter and encoder per output, keeps separate immutable in-flight frames, and polls completion rather than overwriting active frames.
- That driver targets classic ESP32, Arduino-ESP32 3.x, and Arduino running on Core 1. It uses 128 hardware symbols (two blocks) per output; the notes describe eight blocks total on classic ESP32. **Do not expand this exact allocation to six outputs without resolving the resource budget.** Six independent strips are required, but the driver strategy is still a design decision.
- Earlier multi-strip output interference was reported. The exact physical cause and successful resolution have not been established here.
- `ledTest` is a small FastLED three-output test; its existence is not evidence that the final integrated output path is validated.

## Hardware inventory and unresolved wiring

The user called the board “esp32d defkit.” Treat this as an ESP32 dev kit, with the exact module variant still unconfirmed.

| Item | User-confirmed / intended assignment | Still unresolved |
|---|---|---|
| ESP32 dev kit | Main controller | Exact board/module, usable GPIOs |
| TCA9548A mux 1 | Directly connected to ESP32 I2C | Address, A0/A1/A2 wiring, SDA/SCL pins |
| Two MPR121 modules | Mux 1 channels 6 and 7 | Which arm is which, device addresses, electrodes |
| MAX30102 module | Mux 1 channel 2 | Current electrical health and verified address |
| TCA9548A mux 2 | Also directly connected to ESP32 I2C, not cascaded through mux 1 | Distinct address and address-select wiring |
| Two MPU6050 modules | Mux 2; one per leg | Channels, addresses, orientation, exact mounting |
| Six separate LED strips | Left arm, right arm, left leg, right leg, spine, heart | Type, color order, counts, pins, physical directions |
| USB power bank | Into ESP32 USB port; user also described direct 5 V and ground wiring | Exact external power distribution and module supply connections |

Both mux addresses must coexist without conflict. Address selections are not confirmed. Manage mux selections across both devices so identical downstream addresses are not accidentally connected simultaneously.

Known hardware issues are **deferred, not resolved**:

- A sensor on mux port 2 previously heated up; do not assume it is healthy.
- Downstream SDA was measured around 2.3 V.
- Corrupted serial output was previously observed.
- Measured supplies, pull-ups, cable lengths, common ground, LED power distribution, and independently verified components are unknown.
- Before integrated hardware testing, establish actual module requirements and diagnose these issues. Do not prescribe a final electrical design from the historical sketches.

## Body-to-LED configuration

Agreed: all six sections are physically separate strips. Use placeholders for counts, GPIOs, and directions.

| Section | Mapping placeholders | Sensor behavior |
|---|---|---|
| Left arm | Output, LED count, physical first pixel, direction toward spine, electrode regions | Hold local light; release traveling pulse |
| Right arm | Same as left arm | Hold local light; release traveling pulse |
| Left leg | Output, count, first pixel, direction toward spine | Left step launches pulse |
| Right leg | Same as left leg | Right step launches pulse |
| Spine | Output, count, bottom/top direction, arm and leg entry points | Receives pulses and carries them to top |
| Heart | Output and count | Red baseline and double pulse |

Proposed: keep mapping in one configuration layer. Effects should address logical sections and travel directions rather than hard-coded physical indices. Define how each limb route joins the spine; the junction positions are not yet known.

## Agreed animation and sensor behavior

### Palette and overlapping effects

- Most effects should be **ghostly blue**.
- Heart uses dim red with red heartbeat flashes.
- Arm and leg pulses continue onto the spine, travel to its top, then fade.
- Multiple pulses travel independently and blend where they overlap, with brightness capped.
- Exact blue color, pulse size, speed, and fade timings remain tunable choices.

### Arm touch

- Holding an electrode keeps its mapped local LEDs illuminated.
- **Release launches the pulse**, not initial touch and not repeated triggers while held.
- Electrode-to-LED mapping and channel-to-arm mapping remain placeholders.
- Touch behavior should remain responsive while heartbeat, other animations, and website requests run.

Proposed: detect transitions from previous/current touch masks. Track held regions separately from traveling pulse objects. Define initialization and sensor-disconnect behavior so a missing sensor does not masquerade as an intentional release.

### Heartbeat

- Before the first usable rhythm: steady dim red glow only.
- Real accepted beats should trigger the main visual flash immediately, followed by a smaller secondary flash.
- Use timing from the last five accepted beats (four intervals) to fill gaps; reject extreme timing outliers from the estimate.
- Once a rhythm is established, continue predicted beats **indefinitely** through signal loss.
- Real detections resynchronize prediction; prediction and detection must not create accidental duplicate double pulses.
- Heartbeat normally controls the heart, but an explicit manual heart override is allowed.
- Returning the heart to sensor mode restores heartbeat behavior.

Proposed duplicate handling: when a predicted flash has just fired and a real beat arrives within a small matching window, update/reanchor timing without launching another double pulse. Otherwise, launch the real-beat flash immediately. The window is not yet chosen.

Keep three concepts separate: waveform detection, acceptance for timing estimation, and whether a visual pulse has already represented that beat. Do not assume every timing outlier is a reason to flash, or that the first detected beat must wait for a full five-beat history. Explicitly settle startup acceptance, prediction start criteria, and reacquisition after a real tempo change during implementation planning.

Status must distinguish live sensor tracking from prediction, even though the visible rhythm continues indefinitely.

### Footsteps

- Detect ordinary walking, not just deliberate stomping.
- Each accelerometer controls its corresponding leg.
- Thresholds, calibration, and retrigger timing can be tuned later.
- User's placement answer was “thing ish”; the assistant interpreted this as **thigh-ish**, but the user has not explicitly confirmed that interpretation. Do not treat placement as confirmed.

Proposed: consistent sampling, baseline/gravity-aware movement processing, an impact/recovery state machine, and a brief retrigger lockout. Provide sensitivity controls and a calibration action on the website. Validate against actual walking and ordinary non-step movement at the final mounting location.

## Manual control and painting

Agreed:

- Use per-section manual overrides. Controlling one section does not disable sensor control everywhere.
- Provide a prominent **Return all to sensors** action.
- Heartbeat has normal priority on the heart, but the heart can be explicitly switched to manual control.
- Section controls include color, brightness, animation, speed, and direction.
- Users can drag a finger over the body diagram to illuminate approximately **10 LEDs at once** around the brush position.
- Include both persistent **Paint** mode and **Fade** mode for trails.
- Show actual firmware-commanded output on the diagram about once per second.

Proposed:

- Painting a section activates manual override for that section.
- Persistent paint remains until cleared; fade strokes decay with configurable duration.
- Provide per-section clear and return-to-sensors actions.
- Give immediate local drawing feedback, send brush commands promptly, and reconcile with the next device snapshot.
- Interpolate between successive drag positions to avoid gaps during fast movement.
- Brush width can be adjustable with a default near 10 pixels; clamp to section boundaries unless cross-section painting is deliberately supported.
- Continue sensor sampling while overridden, but do not queue old visual triggers for later replay.
- A manually overridden spine suppresses sensor rendering on that section; upstream limb behavior continues. This specific route/override interaction is proposed, not separately agreed.
- The preview shows commanded colors, not measured proof that physical LEDs illuminated.

Individual pixel editing can be supported by reducing brush width; it was not separately specified as a required distinct editor.

## Website and access control

### Agreed access model

- ESP32 creates its own Wi-Fi network.
- Password protection is at the **website control layer**; the user did not request a Wi-Fi password.
- While the website is locked, show **no body diagram, sensor status, or other costume information** to unauthorized visitors.
- Owner credentials permit enabling open access and reclaiming control.
- Open access permits visitors to control, subject to one-controller-at-a-time ownership.
- Only one controller at a time.
- Restart/recovery behavior will be decided later.

### Proposed interaction model

- Locked state shows only the login/access screen.
- Owner can unlock open visitor access, close it, and take over control.
- In open mode, one visitor claims control while others may view the live state.
- Expire abandoned controller ownership using a configurable timeout/lease.
- Revoke visitor access when the owner locks the website.
- Apply authorization and ownership checks to every command and status endpoint, not just visible buttons.
- Do not embed owner credentials in public page assets or return them in status responses.
- Password storage, session handling, transport protections, persistence, and recovery require an explicit implementation design; do not imply a website password alone encrypts network traffic.

### Main authenticated/open screen

- Body diagram: arms, legs, spine, heart, and configured LED sections.
- Section selection with color, brightness, animation, speed, and direction.
- Paint/Fade controls and brush settings.
- Clear-section, return-section-to-sensors, and return-all-to-sensors actions.
- Approximately one-second snapshots of final pixel colors.
- Sensor presence/freshness and faults, touch states, step activity, live/predicted heartbeat, active overrides, current controller, and LED transmission status.
- Owner-only access settings.

Website refresh timing must not slow animation or sensor response. Command delivery should be prompt even though the full visual preview refreshes only once per second.

## Proposed firmware architecture

Use clear modules with nonblocking service functions and independent schedules. Do not assume separate RTOS tasks are necessary for every module.

| Module | Responsibility |
|---|---|
| Hardware/body configuration | Pin/channel placeholders, sensor addresses, counts, directions, electrode regions, spine entry points |
| I2C/mux service | Coordinated branch selection, bounded transactions, sensor-specific polling and error tracking |
| Sensor acquisition | Touch masks, heart FIFO samples, accelerometer samples and freshness |
| Event detection | Touch transitions, accepted beat events, left/right step events |
| Heart scheduler | Timing history, outlier handling, predicted beats, real/predicted reconciliation |
| Animation engine | Traveling pulses, held regions, heart envelope, manual patterns, paint and fading trails |
| Mixer | Section ownership, overlap blending, brightness caps, final pixel buffers |
| LED transport | Independent output submission, frame ownership, completion/error reporting |
| Web/session service | Authentication, controller ownership, commands, status and pixel snapshots |

Use bounded queues/pulse pools and explicit overflow behavior so repeated gestures cannot exhaust memory. Bound work per loop iteration, including reconnect attempts and FIFO draining. Use elapsed-time arithmetic appropriate for timer rollover.

For six LED outputs, evaluate the confirmed board's peripheral and memory budget, strip lengths, refresh needs, and interaction with Wi-Fi before selecting the transport configuration. Preserve the existing driver's frame-ownership discipline. Do not assume changing GPIO numbers solves timing or signal-integrity problems.

## Staged implementation and hardware-test plan

These stages describe future work; implementation is not authorized merely by this handoff.

1. **Configuration and simulated website:** represent six sections with placeholders; exercise synthetic touch, beat, and step events; implement the interaction/access model after authorization.
2. **Isolated hardware verification:** diagnose heating and low SDA, confirm module supplies and wiring, establish unique mux addresses, then test individual sensors and strips. User compiles/uploads.
3. **Six-output validation:** distinct static colors, concurrent animations, one output off while others run, and repeated web traffic. Verify isolation, completion counters, and errors.
4. **Arm behavior:** verify each held region and release pulse, simultaneous touches, electrode mappings, strip direction, and spine junctions.
5. **Heartbeat:** test startup glow, real-beat timing, double pulse, outliers, indefinite prediction, near-coincident real/predicted events, signal reacquisition, and manual override/return.
6. **Footsteps:** record movement at actual mount points; tune for ordinary walking; test standing, leg swings, and unrelated motion for false positives.
7. **Integration:** simultaneous limbs and heart, painting, website snapshots, ownership handoff, locking access, disconnections, sensor faults, and sustained operation.

Meaningful software checks should cover event timing and state transitions, particularly release-only arm triggering, prediction reconciliation, overrides, authorization, and controller ownership. Hardware output acceptance cannot be established by compilation or software tests alone.

## Remaining decisions and assumptions

Do not block software planning on all hardware details; keep placeholders until a dependent choice requires them.

- Exact ESP32 module, LED driver resource strategy, GPIO assignments.
- LED type/color order/counts, power requirements, strip starts and directions, spine entry positions.
- Electrode regions and channel-to-arm mapping.
- Mux addresses/select wiring; accelerometer channels, addresses, placement, orientation.
- Hardware fault diagnosis, supplies, pull-ups, cable lengths, ground and LED power distribution.
- Exact ghostly-blue palette, pulse widths/speeds/fades, paint fade duration.
- Heartbeat acceptance/reacquisition details, prediction startup, duplicate-matching window, secondary pulse timing.
- Footstep thresholds, calibration method, retrigger timing.
- Detailed override behavior at shared spine junctions and on sensor loss.
- Controller lease duration, password/session persistence, access recovery and restart policy.
- Whether future configuration changes persist across reboot; not yet requested or agreed.

## Suggested next-model opening

Read this file and the listed source files. Briefly acknowledge the agreed scope. If the user asks for implementation, identify which parts can proceed using placeholders and ask only the smallest necessary group of unresolved questions. Otherwise, continue refining the plan without generating implementation code.
