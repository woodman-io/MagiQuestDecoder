/*
 * MagiquestDecoder — minimal example (Zephyr/UNO Q and ESP32)
 * Same sketch works on both: select your board and upload.
 * Open Serial Monitor to see wand ID and magnitude when you wave the wand.
 *
 * Zephyr (UNO Q): uses Monitor for USB serial.
 * ESP32 / others: uses Serial.
 */
#include <MagiquestDecoder.h>

#if defined(ARDUINO_ARCH_ZEPHYR) || defined(ARDUINO_UNO_Q) || defined(ARDUINO_UNO_Q_R4)
#include <Arduino_RouterBridge.h>
#define SERIAL_OUT Monitor
#define SERIAL_BEGIN() Monitor.begin()
#else
#define SERIAL_OUT Serial
#define SERIAL_BEGIN() Serial.begin(115200)
#endif

#define IR_PIN A0

void setup() {
  SERIAL_BEGIN();
  MagiquestDecoder_init(IR_PIN);
  SERIAL_OUT.println("Wave MagiQuest wand at IR sensor (pin A0)");
}

void loop() {
  magiquest_frame_t frame;
  if (MagiquestDecoder_read(&frame)) {
    SERIAL_OUT.print("Wand 0x");
    SERIAL_OUT.print((uint16_t)frame.wand_id, HEX);
    SERIAL_OUT.print(" magnitude ");
    SERIAL_OUT.println(frame.magnitude);
  }
  delay(50);
}
