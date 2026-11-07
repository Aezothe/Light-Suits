#include <Arduino.h>
#include <FastLED.h>

#define DATA_PIN     16     // ESP32: GPIO 6 is reserved for flash
#define BTN_PIN      4      // ESP32: avoid GPIO 0/2/12/15 (boot pins)
#define N_UP         19
#define N_DOWN       11
#define N_BAR        8
#define BAR_FIRST    (N_UP + N_DOWN)
#define N_LEDS       (N_UP + N_DOWN + N_BAR)
#define BRIGHTNESS   128
#define BAR_REVERSED false

#define PITCH        16
#define A_TOP_Y      ((N_UP - 1) * PITCH)
#define B_TOP_Y      (A_TOP_Y - PITCH / 2)
#define BAR_GAP      (2 * PITCH)
#define BAR_Y        (A_TOP_Y + BAR_GAP)

#define A_X          24
#define B_X          0
#define BAR_X_LEFT   0
#define BAR_X_RIGHT  24

#define CENTER_X     A_X
#define CENTER_Y     (A_TOP_Y - 5 * PITCH)

uint16_t bpm = 120;

#define RING_MS_PER_LED  35
#define RING_W           40
#define MAX_OVERLAP      3
#define BASE_LEVEL       16

const CRGB PULSE_COLOR = CRGB(255, 255, 255);

CRGB     leds[N_LEDS];
uint16_t ledDist[N_LEDS];
uint16_t maxDist = 1;

uint32_t beatStart = 0;
bool     lastBtn = HIGH;
uint32_t lastBtnChange = 0;

uint8_t ease(uint8_t x) { return (uint16_t)x * x / 255; }

CRGB scaleColor(CRGB c, uint8_t lvl) {
  return CRGB((uint16_t)c.r * lvl / 255,
              (uint16_t)c.g * lvl / 255,
              (uint16_t)c.b * lvl / 255);
}

void setPos(uint8_t i, int16_t x, int16_t y) {
  float dx = x - CENTER_X, dy = y - CENTER_Y;
  ledDist[i] = (uint16_t)(sqrtf(dx * dx + dy * dy) + 0.5f);
  if (ledDist[i] > maxDist) maxDist = ledDist[i];
}

void pollButton() {
  bool s = digitalRead(BTN_PIN);
  if (s != lastBtn && millis() - lastBtnChange > 30) {
    lastBtnChange = millis();
    lastBtn = s;
    if (s == LOW) {
      beatStart = millis();
      Serial.println("Beat reset");
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(BTN_PIN, INPUT_PULLUP);
  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, N_LEDS);
  FastLED.setBrightness(BRIGHTNESS);
  FastLED.clear(true);

  for (uint8_t i = 0; i < N_UP; i++)   setPos(i, A_X, i * PITCH);
  for (uint8_t k = 0; k < N_DOWN; k++) setPos(N_UP + k, B_X, B_TOP_Y - k * PITCH);
  for (uint8_t b = 0; b < N_BAR; b++) {
    uint8_t p = BAR_REVERSED ? (N_BAR - 1 - b) : b;
    int16_t x = BAR_X_LEFT + (int32_t)(BAR_X_RIGHT - BAR_X_LEFT) * p / (N_BAR - 1);
    setPos(BAR_FIRST + b, x, BAR_Y);
  }

  beatStart = millis();
  Serial.println("Ready");
}

void loop() {
  pollButton();

  uint32_t since  = millis() - beatStart;
  uint32_t period = 60000UL / bpm;
  uint32_t age0   = (uint32_t)(((uint64_t)since * bpm) % 60000UL) / bpm;

  for (uint8_t i = 0; i < N_LEDS; i++) {
    uint8_t best = 0;
    for (uint8_t k = 0; k < MAX_OVERLAP; k++) {
      uint32_t age = age0 + k * period;
      if (age > since) break;
      uint32_t radius = age * PITCH / RING_MS_PER_LED;
      if (radius > maxDist + RING_W) continue;
      int32_t d = (int32_t)ledDist[i] - (int32_t)radius;
      if (d < 0) d = -d;
      if (d >= RING_W) continue;
      uint8_t v = ease((RING_W - d) * 255 / RING_W);
      uint16_t amp = radius >= maxDist ? 0 : 255 - radius * 255 / maxDist;
      uint8_t c = (uint16_t)v * amp / 255;
      if (c > best) best = c;
    }
    uint8_t lvl = BASE_LEVEL + (uint16_t)(255 - BASE_LEVEL) * best / 255;
    leds[i] = scaleColor(PULSE_COLOR, lvl);
  }
  FastLED.show();
}