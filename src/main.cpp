#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <math.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define BUTTON_PIN 0       // adjust if your board uses another pin for the button
#define BAUDRATE 115200

// Packet settings
const uint8_t START_BYTE = 0xAA;
const size_t PAYLOAD_LEN = 4*4 + 4; // 4 floats (4 bytes each) + uint32 = 20
const size_t FRAME_LEN = 1 + PAYLOAD_LEN; // 21 bytes

// TUNABLES
const float EMA_ALPHA = 0.25f;
const uint16_t GRAPH_LEN = SCREEN_WIDTH;
const unsigned long DRAW_MS = 100; // redraw interval (ms)
const uint32_t LOSS_WINDOW = 200;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Graph buffers
float graphCPU[GRAPH_LEN];
float graphRAM[GRAPH_LEN];
uint16_t graphIdx = 0;

// EMA state
float cpuEMA = 0.0f;
float ramEMA = 0.0f;
float tempEMA = 0.0f;

// Packet tracking
uint32_t lastSeq = 0xFFFFFFFF;
uint32_t recvCount = 0;
uint32_t lostCount = 0;
uint32_t windowRecv = 0;
uint32_t windowLost = 0;

// FPS / timing
unsigned long lastDraw = 0;
unsigned long lastFpsTime = 0;
uint16_t framesThisSecond = 0;
float fpsVal = 0.0f;

// Latency
float latencyEMA = 0.0f;

// Mode
bool graphMode = true;
unsigned long lastButtonMillis = 0;

int mapToY(float val) {
  float v = constrain(val, 0.0f, 100.0f);
  int y = (int)round((1.0f - v/100.0f) * (SCREEN_HEIGHT - 1));
  if (y < 0) y = 0;
  if (y >= SCREEN_HEIGHT) y = SCREEN_HEIGHT - 1;
  return y;
}

void drawStaticView(float cpu, float ram, float temp) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.printf("CPU: %.1f%%\n", cpu);
  display.printf("RAM: %.1f%%\n", ram);
  display.printf("TEMP: %.1fC\n", temp);
  display.printf("FPS: %.1f\n", fpsVal);
  display.printf("Lat: %.1fms\n", latencyEMA);
  float lossPct = 0.0f;
  if (windowRecv + windowLost > 0) {
    lossPct = (100.0f * (float)windowLost) / (float)(windowRecv + windowLost);
  }
  display.printf("Loss: %.1f%%\n", lossPct);
  display.display();
}

void drawGraphView() {
  display.clearDisplay();
  // draw lines
  for (int x = 0; x < SCREEN_WIDTH - 1; x++) {
    int idx1 = (graphIdx + x) % GRAPH_LEN;
    int idx2 = (graphIdx + x + 1) % GRAPH_LEN;
    int y1_cpu = mapToY(graphCPU[idx1]);
    int y2_cpu = mapToY(graphCPU[idx2]);
    int y1_ram = mapToY(graphRAM[idx1]);
    int y2_ram = mapToY(graphRAM[idx2]);
    display.drawLine(x, y1_cpu, x + 1, y2_cpu, SSD1306_WHITE);
    if (x % 2 == 0) {
      display.drawPixel(x, y1_ram, SSD1306_WHITE);
      display.drawPixel(x + 1, y2_ram, SSD1306_WHITE);
    }
  }
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.printf("Graph | FPS: %.1f | Lat: %.1fms", fpsVal, latencyEMA);
  display.setCursor(0, SCREEN_HEIGHT - 8);
  display.print("CPU=solid  RAM=dotted");
  display.display();
}

void processFrame(uint8_t *payload, size_t len) {
  if (len < PAYLOAD_LEN) return;
  float cpu, ram, temp, ts_ms;
  uint32_t seq;
  memcpy(&cpu, payload + 0, 4);
  memcpy(&ram, payload + 4, 4);
  memcpy(&temp, payload + 8, 4);
  memcpy(&ts_ms, payload + 12, 4);
  memcpy(&seq, payload + 16, 4);

  // sequence & loss
  if (lastSeq == 0xFFFFFFFF) {
    lastSeq = seq;
  } else {
    uint32_t expected = lastSeq + 1;
    if (seq != expected) {
      if (seq > expected) {
        uint32_t missed = seq - expected;
        lostCount += missed;
        windowLost += missed;
      }
    }
    lastSeq = seq;
  }
  recvCount++;
  windowRecv++;

  // latency
  float local_ms = (float)millis();
  float latency = local_ms - ts_ms;
  if (latency < 0) latency = fabs(latency);
  latencyEMA = (latencyEMA == 0.0f) ? latency : (EMA_ALPHA * latency + (1.0f - EMA_ALPHA) * latencyEMA);

  // EMA for values
  cpuEMA = (cpuEMA == 0.0f) ? cpu : (EMA_ALPHA * cpu + (1.0f - EMA_ALPHA) * cpuEMA);
  ramEMA = (ramEMA == 0.0f) ? ram : (EMA_ALPHA * ram + (1.0f - EMA_ALPHA) * ramEMA);
  tempEMA = (tempEMA == 0.0f) ? temp : (EMA_ALPHA * temp + (1.0f - EMA_ALPHA) * tempEMA);

  // graph store
  graphCPU[graphIdx] = cpuEMA;
  graphRAM[graphIdx] = ramEMA;
  graphIdx = (graphIdx + 1) % GRAPH_LEN;
}

void setup() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  Serial.begin(BAUDRATE);
  delay(50);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    while (true) delay(1000);
  }
  display.clearDisplay();
  display.display();

  // init arrays
  for (uint16_t i = 0; i < GRAPH_LEN; i++) {
    graphCPU[i] = 0.0f;
    graphRAM[i] = 0.0f;
  }
  lastDraw = millis();
  lastFpsTime = millis();
}

void loop() {
  // button toggle
  if (digitalRead(BUTTON_PIN) == LOW) {
    unsigned long now = millis();
    if (now - lastButtonMillis > 300) {
      graphMode = !graphMode;
      lastButtonMillis = now;
    }
  }

  // read serial frames: find start byte then read payload
  while (Serial.available() > 0) {
    int b = Serial.read();
    if (b == START_BYTE) {
      // try to read payload
      uint8_t payload[PAYLOAD_LEN];
      size_t got = 0;
      unsigned long start = millis();
      while (got < PAYLOAD_LEN && millis() - start < 50) { // wait up to 50ms for full frame
        if (Serial.available() > 0) {
          int r = Serial.read();
          if (r >= 0) {
            payload[got++] = (uint8_t)r;
          }
        }
      }
      if (got == PAYLOAD_LEN) {
        processFrame(payload, PAYLOAD_LEN);
      } else {
        // incomplete frame; drop and resync
      }
    } else {
      // skip until we find start byte
    }
  }

  // FPS calculation
  framesThisSecond++;
  if (millis() - lastFpsTime >= 1000) {
    fpsVal = (float)framesThisSecond;
    framesThisSecond = 0;
    lastFpsTime = millis();
    // reset window counters if large
    if (windowRecv + windowLost > LOSS_WINDOW) {
      windowRecv = 0;
      windowLost = 0;
    }
  }

  // draw
  unsigned long now = millis();
  if (now - lastDraw >= DRAW_MS) {
    lastDraw = now;
    if (graphMode) drawGraphView();
    else drawStaticView(cpuEMA, ramEMA, tempEMA);
  }
}
      