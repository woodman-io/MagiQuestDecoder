# MagiquestDecoder

MagiQuest wand IR decoder for Arduino-compatible boards. No IRremote required—decodes from raw pulse timing with optional interrupt-based capture.

**Supported platforms:**

| Platform        | Board examples   | Serial output      |
|-----------------|------------------|---------------------|
| Zephyr          | Arduino UNO Q    | `Monitor` (Router Bridge) |
| ESP32           | ESP32 Dev Module | `Serial`            |
| AVR / SAM / etc.| Uno, Mega, etc.  | `Serial`            |

Use the **same sketch** and switch the board in the IDE; the example uses preprocessor defines to pick Monitor vs Serial.

## Installation

**Option A — Use from this project (same folder as your sketch)**  
If your sketch is inside `magiquest-sensor-irremote/`, the IDE may find the library automatically when it’s in the same parent folder. Otherwise use Option B.

**Option B — Install as a user library (recommended for reuse)**  
1. Copy the entire `MagiquestDecoder` folder into your Arduino **libraries** folder:
   - **macOS:** `~/Documents/Arduino/libraries/`
   - **Windows:** `Documents\Arduino\libraries\`
   - **Linux:** `~/Arduino/libraries/`
2. Restart the Arduino IDE (or Arduino App Lab) so it picks up the new library.

## Hardware

- **IR receiver** (e.g. 38 kHz demodulator) output connected to any **digital pin** (e.g. **A0**).
- **VCC** and **GND** as per the receiver module. No other libraries or hardware required.

## Usage

```cpp
#include <MagiquestDecoder.h>

#define IR_PIN  A0

void setup() {
  MagiquestDecoder_init(IR_PIN);
}

void loop() {
  magiquest_frame_t frame;
  if (MagiquestDecoder_read(&frame)) {
    // frame.wand_id   — 32-bit wand ID (use (uint16_t)frame.wand_id for 16-bit)
    // frame.magnitude — scaled 0..~1023 (how hard the wand was waved)
    // e.g. Serial/Monitor, LEDs, etc.
  }
  delay(50);
}
```

## API

- **`void MagiquestDecoder_init(uint8_t recvPin)`**  
  Call once from `setup()`. Sets the pin and enables interrupt-based capture (if enabled).

- **`bool MagiquestDecoder_read(magiquest_frame_t *out)`**  
  Tries to read and decode one MagiQuest frame. Returns `true` if a valid frame was decoded and fills `out` with `wand_id` and `magnitude`. May block briefly when idle and up to ~250 ms during a capture.

- **`magiquest_frame_t`**  
  - `magnitude` (uint16_t) — scaled magnitude.  
  - `wand_id` (uint32_t) — full wand ID; for display, cast to `(uint16_t)` to match common 16-bit usage.

## Configuration (optional)

Before `#include <MagiquestDecoder.h>` you can define:

- **`MAGIQUEST_USE_INTERRUPT 0`** — Use polling instead of interrupt (e.g. if the pin doesn’t support interrupts). Default is `1` (interrupt).

## Switching between Zephyr (UNO Q) and ESP32

1. Use the **SimpleRead** example as-is.
2. In the IDE, select **Board: Arduino UNO Q** (or your Zephyr board) and upload — serial goes to **Monitor** (USB).
3. Switch to **Board: ESP32 Dev Module** (or your ESP32) and upload — serial goes to **Serial** (USB).  
   On ESP32, open Serial Monitor at **115200** baud.

The library uses the same API on all platforms. On ESP32 the IR ISR is placed in IRAM for reliable timing.

## Compatibility

Tested on **Arduino UNO Q** (Zephyr) and **ESP32**. Should work on any Arduino-compatible board that provides `micros()`, `digitalRead()`, and `attachInterrupt()`. For UNO Q you need **Arduino_RouterBridge** in your sketch if you use Monitor for logging.
