# HeavenlyPointer — Stack-chan Satellite Tracker Firmware

Turn your Stack-chan (M5Stack **CoreS3** + 2-axis servo base) into a live
satellite tracker. It learns your WiFi, downloads orbital data, figures out
where it is, and then physically **points its face at satellites** as they
pass overhead — showing the satellite name and live telemetry on screen.

---

## What it does

1. **WiFi onboarding** — first boot shows a QR code and the setup network name
   (**`HeavenlyPointer`**) on screen. Scan the QR (or join that network from your
   phone's WiFi list); a setup form opens that **lists the nearby networks it
   found** — pick yours (or choose *Other* for a hidden SSID) and enter the
   password. Prefer not to use a phone? Tap **Touch entry** and use the on-screen
   network picker + keyboard.
2. **Pulls down & stores data** — once online it:
   - syncs **UTC time** from NTP (and saves it to the CoreS3 RTC),
   - finds **where it is** via IP geolocation (or your manual override),
   - downloads a **TLE satellite catalog** from [Celestrak](https://celestrak.org)
     and caches it to flash (LittleFS), with the fetch timestamp.
3. **Desk calibration** — place Stack-chan flat, face pointing straight
   forward, and tap whether the face points **North / East / South / West**.
4. **Live tracking** — using **SGP4** orbital propagation it computes, every
   second, the azimuth/elevation of every catalogued satellite, picks the one
   highest above your horizon, and drives the **pan/tilt servos** so the face
   points at where that satellite is in the sky.
5. **On-screen telemetry** — the display shows the tracked satellite's name,
   azimuth, elevation, slant range, altitude, orbital velocity, sub-satellite
   ground point, visibility, a **type icon** (station / Starlink / nav / weather
   / telescope / rocket body / comsat, drawn from the name), plus a polar sky
   plot and the servo angles.
6. **Phone web control** — point a browser at the Stack-chan's IP (shown in the
   screen's corner) to watch live telemetry and change the satellite
   configuration from anywhere on your network.

Tracking is fully local once data is cached — it keeps pointing even if WiFi
drops. WiFi is only re-used to refresh the catalog (every 12 h) and re-sync time.

---

## Hardware

M5Stack **Stack-chan** — CoreS3 (ESP32-S3, 16 MB flash / 8 MB PSRAM), 2" LCD,
GC0308 0.3 MP camera, **serial-bus pan/tilt servos**, 12 RGB LEDs, capacitive
touch. Full chipset/spec breakdown: [docs/DEVICE.md](docs/DEVICE.md) · official
device docs: <https://docs.m5stack.com/en/StackChan>.

- **M5Stack CoreS3** (ESP32-S3) — the brain inside Stack-chan
- **Stack-chan 2-axis servo base** — pan (azimuth) + tilt (elevation) Feetech
  serial-bus servos, driven by [StackChan-BSP](https://github.com/m5stack/StackChan-BSP)
- USB-C cable for flashing/power

> **Power:** for continuous tracking, keep the CoreS3 on USB-C. The pan/tilt
> servos draw their biggest current when moving and when the tilt motor holds
> the head up against gravity; the 550 mAh internal battery sags under that load
> and can't sustain it for long. (The firmware releases servo torque whenever
> it's idle to minimise this, but tracking sessions still want wall power.)

---

## Install (flash a prebuilt release)

[⚡ Flash from your browser](https://r3dfish.github.io/HeavenlyPointer/)

The easiest path — no toolchain needed. Download `HeavenlyPointer-v1.0.0.bin`
from the **[Releases](https://github.com/r3dfish/HeavenlyPointer/releases)** page and flash it to a CoreS3 over USB-C.
That single image already contains the bootloader, partition table and app; the
device formats its own filesystem and downloads satellite data on first boot.

```bash
# esptool (pip install esptool); add --port /dev/ttyACM0 (Linux/Mac) or COMx (Windows) if needed
esptool.py --chip esp32s3 --baud 1500000 write_flash 0x0 HeavenlyPointer-v1.0.0.bin
```

Prefer a GUI? Use M5Stack's [M5Burner](https://docs.m5stack.com/en/uiflow/m5burner/intro)
("Custom firmware" → pick the `.bin` → burn at offset `0x0`), or any ESP Web Tools
browser flasher. After flashing it boots straight into WiFi setup — see
[Using it](#using-it).

---

## Build from source (PlatformIO)

Prereqs: [PlatformIO](https://platformio.org/) (the VS Code extension or the
`pio` CLI) and `git`.

```bash
git clone https://github.com/r3dfish/HeavenlyPointer.git
cd HeavenlyPointer
pio run                      # build (first run downloads the toolchain + libraries)
pio run -t upload            # flash the firmware
pio run -t uploadfs          # (optional) create the LittleFS data partition
pio device monitor           # serial log @ 115200
```

PlatformIO pulls the libraries automatically (see `platformio.ini`):
`M5Unified`, [`StackChan-BSP`](https://github.com/m5stack/StackChan-BSP)
(drives the servos/RGB/power), `ArduinoJson`, and the
[Hopperpop SGP4 library](https://github.com/Hopperpop/Sgp4-Library).

> The Stack-chan's pan/tilt servos are **Feetech serial-bus servos** driven by
> StackChan-BSP — *not* PWM hobby servos on GPIO pins. The BSP also requires
> **arduino-esp32 3.x**, which is why `platformio.ini` uses the **pioarduino**
> platform fork rather than stock `espressif32`.

---

## ⚠️ Calibration — do this first

Because the BSP owns the servos, there are no pins to wire — but the head
geometry in [`src/config.h`](src/config.h) should be checked on hardware. Use
the live `pan`/`tilt` readout on the tracking screen to dial these in:

| Constant | What to set |
|---|---|
| `YAW_DIR` | `+1`/`-1` — flip if the head pans the *wrong way* (e.g. turns toward West when the satellite is East). |
| `YAW_LIMIT_TENTHS` | Pan limit in 1/10°. `1200` = ±120° (near the ±1280 electronic limit). **Verify the head physically clears the mechanical stop at this angle** — driving into the stop stalls the pan servo and trips its overload protection (head goes limp until power-cycled, and a hard stall can strip the gears). |
| `PITCH_HORIZON_TENTHS` | Pitch value (1/10°) that aims the face at the **horizon** (elevation 0). The main thing to calibrate. |
| `PITCH_DIR` | `+1`/`-1` — flip if the head tilts *down* when the satellite rises. |
| `PITCH_MIN_TENTHS` / `PITCH_MAX_TENTHS` | Safe pitch band, default `50`–`850` (5°–85°). **Keep within 5°–85°** — the product page warns the Y-axis is damaged outside this range. |
| `SERVO_SPEED` | BSP smoothing speed `0`–`1000` (default `250`). |

> Angles are in **1/10-degree units** to match StackChan-BSP (e.g. `900` = 90°).
> The BSP runs its own smoothing engine, so there's no slew/tick tuning to do.

---

## Using it

| Step | What you see / do |
|---|---|
| First boot | QR code + "WiFi Setup". Scan & enter creds, or tap **Touch entry**. |
| After connect | "Syncing time", "Finding location", "Downloading satellite data". |
| Calibrate | Tap the compass direction the face points. (Asked once.) |
| Tracking | Satellite name + telemetry + sky plot. The head moves to follow it. Tap the **top-left/top-right corners** to browse prev/next visible satellite. |
| "Scanning the sky" | Nothing is above the horizon yet — it homes the head, **releases the servo torque** (so the tilt motor isn't holding the head up against gravity), and shows a **countdown to the next pass** (which satellite + HH:MM:SS until it rises, and its peak elevation). The head re-energizes automatically when the next satellite rises. |
| Reset | Press-and-hold the bottom of the screen ~3 s to wipe config & re-provision. |

### Name-bar colour = optical visibility

The strip behind the satellite name (and the matching word lower on the screen)
tells you whether you could actually *see* the satellite right now:

| Colour | Meaning | Why |
|---|---|---|
| 🟢 **Green** | **visible** | Sunlit satellite **and** your sky is dark — you could spot it crossing overhead (e.g. an ISS flyover). Go look. |
| 🔵 **Blue** | **eclipsed** | Above your horizon and the sky is dark, but the satellite is in **Earth's shadow** — not catching sunlight, so there's nothing to see. |
| 🔴 **Dark red** | **daylight** | The sun is up where you are — still tracked, just too bright to see. |

A blue → green change is the satellite emerging from Earth's shadow into
sunlight while your sky is dark — the moment it would "light up" to the eye.

---

## Web control (phone / browser)

Once on WiFi, the Stack-chan runs a small web server. Open **`http://<its-ip>/`**
in any browser on the same network — the IP is shown in the corner of the
screen. The page shows **live telemetry** (auto-refreshing), has **◀ Prev / Next ▶**
buttons to browse the currently-visible satellites (same as the on-screen corner
taps), and lets you change the **satellite configuration** without touching the
device:

| Setting | Effect |
|---|---|
| **Catalog group** | Switch between `visual` / `stations` / `amateur` (ham-radio sats) / `active` / `weather` / `starlink` / `gps-ops`. Triggers a re-download. |
| **Name filter** | Track only satellites whose name contains this text (e.g. `ISS`, `NOAA`, `STARLINK`). Blank = everything. |
| **Reach limits** | Min/max elevation and azimuth (pan ±°) the head can point at. The tracker **only selects satellites inside these limits** — and if the one it's following drifts out of range, it **automatically switches to the next in-range satellite** (or parks if none is reachable). |
| **Orbit class** | `LEO only` (default — fast passes that sweep the sky), `Skip geostationary` (LEO + MEO), or `All orbits`. Classified by mean motion: LEO ≥ 11.25, MEO ~2, GEO ~1 rev/day. |
| **Facing** | Re-set which way the head's "forward" points (N/E/S/W) without the on-screen calibration. |
| **Observer location** | Shows the **geolocation status** (📍 city + coordinates on success, or a ⚠ failure notice). Override lat/lon, or tap **Auto-locate from IP**. |
| **Clock offset** | UTC offset (hours) for the displayed clock — auto-detected from your IP, override here. Tracking itself always uses UTC. |
| **LED bars** | RGB bars **green while tracking**, **red while waiting** for the next pass. On by default; uncheck to turn them off. |
| **Sleep schedule** | Quiet hours — set a local **Sleep at / Wake at** window. During it the head goes limp (servo torque released), and the screen + LEDs turn off (WiFi stays up). Toggle to enable. |
| **Actions** | Re-download TLEs, recenter the head, **Recover servos** (re-enable torque after a stall), or **SLEEP NOW** (the button becomes **WAKE NOW** while asleep). |
| **Motor test** | Tick **Test mode** to pause tracking and drive the pan/tilt servos directly (sliders, presets, and **Sweep pan / Sweep tilt**). Bypasses all az/el math, so it isolates each physical motor — if "Sweep pan" moves nothing but "Sweep tilt" works, the azimuth servo isn't responding. |

While asleep the device keeps serving the web page, so you can **WAKE NOW** from
your phone — or just **tap the (dark) screen** to wake it. A manual wake during a
scheduled window holds until the next window edge, when the schedule resumes.

Changes persist to NVS and apply immediately (a group change kicks off a
catalog re-download, during which the head holds its last position).

The JSON API behind the page (handy for scripting):

| Endpoint | Method | Purpose |
|---|---|---|
| `/` | GET | The control page |
| `/status.json` | GET | Live target + telemetry + servo angles + IP + time |
| `/config.json` | GET | Current saved configuration |
| `/config` | POST | Apply config (form fields: `group`, `filter`, `minel`, `maxel`, `panlim`, `orbit`, `facing`, `lat`, `lon`, `tzoff`, `leds`, `sleepsched`, `sleepstart`, `sleepend`) |
| `/action` | POST | `cmd=` `refetch` \| `relocate` \| `park` \| `prev` \| `next` \| `sleep` \| `wake` \| `recover` \| `test` \| `testexit` |
| `/test` | POST | Motor test: `pan` / `tilt` in degrees (only while test mode is active) |

> No authentication — intended for your **local network** only. Don't port-forward it.

The default catalog group on a fresh install is still `DEFAULT_TLE_GROUP` in
[`src/config.h`](src/config.h); the web UI overrides it at runtime thereafter.

---

## Project layout

```
platformio.ini        Build config, board, libraries, partition table
partitions_16MB.csv   16 MB flash map with a LittleFS data area for TLEs
src/
  config.h            All tunables + servo/mount calibration (edit this!)
  Settings.*          Persistent config in NVS (WiFi, location, facing, catalog)
  Net.*               WiFi connect, NTP, IP geolocation, TLE download+store
  Provision.*         Captive-portal + on-screen QR + touch-keyboard onboarding
  Sky.*               Loads TLEs, runs SGP4, picks the best reachable target
  ServoHead.*         az/el -> StackChan-BSP head pointing (1/10-degree units)
  UI.*                Screens: provision, keyboard, calibrate, tracking HUD
  SatIcon.*           Vector satellite icons by type (classified from the name)
  WebControl.*        HTTP server: live telemetry + satellite config API
  web_assets.h        Embedded phone control page (HTML/CSS/JS, PROGMEM)
  Status.h            Shared live-status struct + web-UI request flags
  main.cpp            State machine wiring it all together
```

---

## How the pointing works

```
SGP4(TLE, UTC time, observer lat/lon/alt)  ->  azimuth, elevation
relAz = wrap180(azimuth - forwardBearing)              # forwardBearing from N/E/S/W choice
yaw   = YAW_DIR   * relAz*10           (tenths, clamped to +/-YAW_LIMIT_TENTHS)
pitch = PITCH_HORIZON_TENTHS + PITCH_DIR * elevation*10 (tenths, clamped 5-85 deg)
M5StackChan.Motion.move(yaw, pitch, SERVO_SPEED)
```

The face can only *orient* (it can't translate), and a satellite is effectively
at infinity, so aiming the display normal along the (az, el) vector is exactly
"pointing at it."

---

## Troubleshooting

*(Device-level notes — these apply to the Stack-chan hardware in general.)*

- **`/dev/ttyACM0` busy / permission denied when flashing** — on Linux your user
  needs the `dialout` group active. One-off: `sudo chmod a+rw /dev/ttyACM0`;
  permanent: add yourself to `dialout` and log out/in.
- **Build fails on the StackChan-BSP / `std::make_unique` / ESP-IDF 5 APIs** —
  the BSP needs **arduino-esp32 3.x**, which is why `platformio.ini` uses the
  **pioarduino** platform fork (not stock `espressif32`) and fetches the BSP
  from git, so PlatformIO needs `git` available.
- **Head pans or tilts the wrong way** — flip `YAW_DIR` / `PITCH_DIR` in
  [`src/config.h`](src/config.h) (see [Calibration](#️-calibration--do-this-first)).
- **Camera shares the CoreS3 internal I²C bus with the touch panel.** This
  firmware uses touch (not the camera), so it's fine — but if you later add the
  GC0308 camera (e.g. for auto-heading or QR scanning), it contends with the
  touch controller on that bus and they can't both be live at once.

---

## Notes & limitations

- **Azimuth coverage** is limited by the pan servo's travel (±120° by default).
  The head clamps and the HUD shows **OUT OF RANGE** when a
  target is further around than the head can turn — rotate the whole robot or
  change the facing direction to recenter the workable arc.
- **Elevation coverage** is limited to the safe 5°–85° pitch band, so satellites
  very near the horizon or directly overhead clamp at the nearest limit.
- **Accuracy** is as good as your TLEs (refreshed every 12 h), your clock (NTP),
  your location (IP geolocation is city-level — set lat/lon in the setup form
  for precision), and your physical leveling/centering.
- **Geolocation/data** use public endpoints (`ip-api.com`, `celestrak.org`).
  TLS certs are not pinned (`setInsecure`) to keep it light on the MCU.
- The servos are **Feetech serial-bus servos** driven by StackChan-BSP; the head
  geometry constants are sensible defaults but should be confirmed on hardware.
```
