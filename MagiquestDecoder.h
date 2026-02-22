/*
 * MagiquestDecoder — MagiQuest IR decoder for Arduino-compatible boards
 *
 * Supported: Zephyr (e.g. Arduino UNO Q), ESP32, and other AVR/ARM boards.
 * No IRremote dependency. Decodes MagiQuest protocol from raw pulse timing.
 * Use with an IR receiver module on any digital pin (e.g. A0).
 *
 * Usage:
 *   #include <MagiquestDecoder.h>
 *
 *   void setup() {
 *     MagiquestDecoder_init(A0);   // IR receiver pin
 *   }
 *   void loop() {
 *     magiquest_frame frame;
 *     if (MagiquestDecoder_read(&frame)) {
 *       // frame.wand_id (32-bit, use low 16 for display: (uint16_t)frame.wand_id)
 *       // frame.magnitude (scaled 0..~1023 typical)
 *     }
 *     delay(50);
 *   }
 */
#ifndef MAGIQUEST_DECODER_H
#define MAGIQUEST_DECODER_H

#include <Arduino.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Decoded MagiQuest frame (56-bit protocol). */
typedef struct magiquest_frame {
  uint16_t magnitude;  /**< Scaled magnitude (how hard waved). */
  uint32_t wand_id;    /**< 32-bit wand ID; use (uint16_t)wand_id for 16-bit display. */
  uint8_t  padding;
  uint8_t  scrap;
} magiquest_frame_t;

/**
 * Initialize the decoder. Call once from setup().
 * @param recvPin Digital pin connected to IR receiver output (e.g. A0).
 */
void MagiquestDecoder_init(uint8_t recvPin);

/**
 * Try to read and decode one MagiQuest frame.
 * Blocks briefly when no IR is present; blocks up to ~250 ms during capture.
 * @param out  Filled with wand_id and magnitude on success.
 * @return true if a valid frame was decoded, false otherwise.
 */
bool MagiquestDecoder_read(magiquest_frame_t *out);

#ifdef __cplusplus
}
#endif

#endif /* MAGIQUEST_DECODER_H */
