/*
 * MagiquestDecoder — implementation (Zephyr/UNO Q, ESP32, and compatible Arduino)
 */
#include "MagiquestDecoder.h"

#define MAGIQUEST_MARK_0      280
#define MAGIQUEST_MARK_1      560
#define MAGIQUEST_SPACE_0     840
#define MAGIQUEST_SPACE_1     560
#define MAGIQUEST_TOLERANCE   200
#define MAGIQUEST_BITS        56
#define MAGIQUEST_MAX_PULSE   2000
#define MAGIQUEST_CAPTURE_MS  250
#define MAGIQUEST_PULSE_BUF   220
#define MAGIQUEST_TS_BUF      230
#define MAGNITUDE_SCALE_SHIFT 6

#ifndef MAGIQUEST_USE_INTERRUPT
#define MAGIQUEST_USE_INTERRUPT 1
#endif

#if defined(ESP32)
#define MAGIQUEST_ISR_ATTR IRAM_ATTR
#else
#define MAGIQUEST_ISR_ATTR
#endif

static uint8_t s_recvPin = 0;

#if MAGIQUEST_USE_INTERRUPT
static volatile uint32_t s_irTimestamps[MAGIQUEST_TS_BUF];
static volatile uint16_t s_irTsIdx;
static volatile uint32_t s_irLastTs;
static volatile uint8_t s_irReady;

static void MAGIQUEST_ISR_ATTR irEdge(void) {
  uint32_t now = micros();
  if (s_irTsIdx > 0) {
    uint32_t d = now - s_irLastTs;
    if (d > MAGIQUEST_MAX_PULSE && s_irTsIdx >= 113) s_irReady = 1;
  }
  if (s_irTsIdx < MAGIQUEST_TS_BUF) s_irTimestamps[s_irTsIdx++] = now;
  s_irLastTs = now;
}
#endif

static bool readPulses(uint64_t *outRaw) {
  uint32_t pulses[MAGIQUEST_PULSE_BUF];
  int n = 0;

#if MAGIQUEST_USE_INTERRUPT
  s_irTsIdx = 0;
  s_irReady = 0;
  s_irLastTs = micros();
  attachInterrupt(digitalPinToInterrupt(s_recvPin), irEdge, CHANGE);
  uint32_t captureStart = millis();
  while (!s_irReady && (millis() - captureStart) < MAGIQUEST_CAPTURE_MS) {
    if (s_irTsIdx >= MAGIQUEST_TS_BUF) break;
  }
  detachInterrupt(digitalPinToInterrupt(s_recvPin));
  uint16_t cnt = s_irTsIdx;
  if (cnt < 112) return false;
  for (uint16_t i = 0; i < cnt - 1 && n < MAGIQUEST_PULSE_BUF; i++) {
    uint32_t d = s_irTimestamps[i + 1] - s_irTimestamps[i];
    if (d <= MAGIQUEST_MAX_PULSE) pulses[n++] = d;
  }
  if (cnt == 112) {
    uint32_t d = micros() - s_irTimestamps[111];
    if (d <= MAGIQUEST_MAX_PULSE && n < MAGIQUEST_PULSE_BUF) pulses[n++] = d;
  }
#else
  uint32_t captureStart = micros();
  uint8_t s0 = digitalRead(s_recvPin);
  uint32_t t = micros();
  while (digitalRead(s_recvPin) == s0 && (micros() - t) < 100000) { ; }
  if (digitalRead(s_recvPin) == s0) return false;
  int lastState = digitalRead(s_recvPin);
  uint32_t last = micros();
  while (n < MAGIQUEST_PULSE_BUF && (micros() - captureStart) < (MAGIQUEST_CAPTURE_MS * 1000UL)) {
    uint8_t current = digitalRead(s_recvPin);
    if (current == lastState) continue;
    uint32_t now = micros();
    uint32_t len = now - last;
    last = now;
    lastState = current;
    if (len > MAGIQUEST_MAX_PULSE) continue;
    pulses[n++] = len;
  }
#endif

  int need = MAGIQUEST_BITS * 2;
  if (n < 111) return false;
  int useBits = MAGIQUEST_BITS;
  int usePulses = need;
  if (n == 111) { useBits = 55; usePulses = 110; }
  uint64_t bestRaw = 0;
  uint16_t bestMag = 0xFFFF;
  int bestSet = 0;
  int bases[2], nBases;
  if (n == 111) { bases[0] = 0; bases[1] = 1; nBases = 2; }
  else          { bases[0] = n - usePulses; nBases = 1; }
  for (int bi = 0; bi < nBases; bi++) {
    int base = bases[bi];
    if (base < 0) continue;
    for (int markOffset = 0; markOffset <= 1; markOffset++) {
      int startIdx = base + markOffset;
      if (startIdx + 2 * (useBits - 1) >= n) continue;
      for (int useSpace = 0; useSpace <= 1; useSpace++) {
        for (int lsbFirst = 0; lsbFirst <= 1; lsbFirst++) {
          uint64_t raw56 = 0;
          int ok = 1;
          for (int i = 0; i < useBits; i++) {
            int mi = startIdx + 2 * i;
            if (mi + useSpace >= n) { ok = 0; break; }
            uint32_t us = pulses[mi + useSpace];
            int bit;
            if (useSpace) {
              if (us >= MAGIQUEST_SPACE_0 - MAGIQUEST_TOLERANCE && us <= MAGIQUEST_SPACE_0 + MAGIQUEST_TOLERANCE) bit = 0;
              else if (us >= MAGIQUEST_SPACE_1 - MAGIQUEST_TOLERANCE && us <= MAGIQUEST_SPACE_1 + MAGIQUEST_TOLERANCE) bit = 1;
              else { ok = 0; break; }
            } else {
              if (us >= MAGIQUEST_MARK_0 - MAGIQUEST_TOLERANCE && us <= MAGIQUEST_MARK_0 + MAGIQUEST_TOLERANCE) bit = 0;
              else if (us >= MAGIQUEST_MARK_1 - MAGIQUEST_TOLERANCE && us <= MAGIQUEST_MARK_1 + MAGIQUEST_TOLERANCE) bit = 1;
              else { ok = 0; break; }
            }
            if (lsbFirst) raw56 |= ((uint64_t)(bit & 1) << i);
            else raw56 = (raw56 << 1) | (bit & 1);
          }
          if (!ok) continue;
          if (useBits == 55) {
            if (lsbFirst) raw56 &= ((uint64_t)1 << 55) - 1;
            else raw56 <<= 1;
          }
          uint8_t preamble;
          uint32_t wid;
          uint16_t mag;
          if (lsbFirst) {
            preamble = (uint8_t)(raw56 & 0xFF);
            wid     = (uint32_t)((raw56 >> 8) & 0xFFFFFFFF);
            mag     = (uint16_t)((raw56 >> 40) & 0xFFFF);
          } else {
            preamble = (uint8_t)(raw56 >> 48);
            wid     = (uint32_t)((raw56 >> 16) & 0xFFFFFFFF);
            mag     = (uint16_t)(raw56 & 0xFFFF);
          }
          if (preamble != 0 || wid == 0) continue;
          for (int shift = 0; shift <= 2; shift++) {
            uint64_t r = raw56;
            if (shift == 1) r = raw56 >> 1;
            else if (shift == 2) r = (raw56 << 1) & (((uint64_t)1 << 56) - 1);
            uint8_t pr;
            uint32_t w;
            uint16_t mg;
            if (lsbFirst) {
              pr = (uint8_t)(r & 0xFF);
              w  = (uint32_t)((r >> 8) & 0xFFFFFFFF);
              mg = (uint16_t)((r >> 40) & 0xFFFF);
            } else {
              pr = (uint8_t)(r >> 48);
              w  = (uint32_t)((r >> 16) & 0xFFFFFFFF);
              mg = (uint16_t)(r & 0xFFFF);
            }
            if (pr != 0 || w == 0 || mg == 0) continue;
            if (mg < bestMag) {
              bestMag = mg;
              if (lsbFirst)
                bestRaw = ((uint64_t)(r & 0xFF) << 48) | ((uint64_t)((r >> 8) & 0xFFFFFFFF) << 16) | ((r >> 40) & 0xFFFF);
              else bestRaw = r;
              bestSet = 1;
            }
          }
        }
      }
    }
  }
  if (bestSet) { *outRaw = bestRaw; return true; }
  return false;
}

static int decodeFrame(uint64_t raw, magiquest_frame_t *frame) {
  if ((uint8_t)(raw >> 48) != 0) return 0;
  frame->wand_id = (uint32_t)((raw >> 16) & 0xFFFFFFFF);
  uint32_t magRaw = (uint32_t)(raw & 0xFFFF);
  frame->magnitude = (uint16_t)(magRaw >> MAGNITUDE_SCALE_SHIFT);
  if (frame->magnitude == 0 && magRaw != 0) frame->magnitude = 1;
  frame->padding = 0;
  frame->scrap = 0;
  if (frame->wand_id == 0) return 0;
  return 1;
}

void MagiquestDecoder_init(uint8_t recvPin) {
  s_recvPin = recvPin;
  pinMode(s_recvPin, INPUT_PULLUP);
}

bool MagiquestDecoder_read(magiquest_frame_t *out) {
  uint64_t raw;
  if (!readPulses(&raw)) return false;
  return decodeFrame(raw, out) != 0;
}
