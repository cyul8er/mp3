#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <YX5300_ESP32.h>

// yx5300 module serial pins
#define RX 16
#define TX 17

// pushbuttons
const uint8_t pinBtnPlay = 18;
const uint8_t pinBtnPrev = 5;
const uint8_t pinBtnNext = 19;

volatile bool isPlaying = true;

byte currentFolder = 1;

// -Song names 
const char* playlist1Songs[] = {"Motion - CORTIS", "weigex my time - ALNST x bo en"};
const char* playlist2Songs[] = {"Anywhere But Home - 슬기"};
const char* playlist3Songs[] = {"Heart - C!naH", "Dreamer - TXT", "밤소풍-ILLIT"};

const uint8_t playlist1Count = sizeof(playlist1Songs) / sizeof(playlist1Songs[0]);
const uint8_t playlist2Count = sizeof(playlist2Songs) / sizeof(playlist2Songs[0]);
const uint8_t playlist3Count = sizeof(playlist3Songs) / sizeof(playlist3Songs[0]);

volatile int currentTrackIndex = 0;

volatile bool displayNeedsUpdate = true;

//debounce (ms)
const unsigned long DEBOUNCE_DELAY = 500;  
volatile unsigned long lastPressTime = 0;

//volume = potentiometer
const uint8_t potVolume = 4;

// auxiliary volume variables
#define MIN_VOLUME 0
#define MAX_VOLUME 30
int current_volume = 0;

// screen
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define I2C_SDA 21
#define I2C_SCL 22

// selection = joystick 
// const uint8_t pinJoyUp     = ;
// const uint8_t pinJoyDown   = ;
// const uint8_t pinJoyLeft   = ;
// const uint8_t pinJoyRight  = ;
// const uint8_t pinJoySwitch = ;

// mp3 object
YX5300_ESP32 mp3;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// song id
const char* getSongName(byte folder, int index) {
  if (folder == 1 && index >= 0 && index < playlist1Count) {
    return playlist1Songs[index];
  }
  if (folder == 2 && index >= 0 && index < playlist2Count) {
    return playlist2Songs[index];
  }
  return "Unknown";
}

void updateDisplay() {
  char folderStr[4];
  sprintf(folderStr, "%02d", currentFolder);

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.print("Playlist --> ");
  display.println(folderStr);
  display.setCursor(0, 20);
  display.print("Now Playing: ");
  display.println(getSongName(currentFolder, currentTrackIndex));
  display.display();
}

//play/pause
void ARDUINO_ISR_ATTR buttonPlay() {
  unsigned long now = millis();
  if (now - lastPressTime > DEBOUNCE_DELAY) {
    if (isPlaying) {
      mp3.pause();
      isPlaying = false;
    } else {
      mp3.resume();
      isPlaying = true;
    }
  }
  lastPressTime = now;
}

void ARDUINO_ISR_ATTR buttonPrev() {
  unsigned long now = millis();
  if (now - lastPressTime > DEBOUNCE_DELAY) {
    mp3.prev();
    isPlaying = true;
    uint8_t count = (currentFolder == 1) ? playlist1Count : playlist2Count;
    if (count > 0) {
      currentTrackIndex = (currentTrackIndex - 1 + count) % count;
    }
    displayNeedsUpdate = true;
  }
  lastPressTime = now;
}

// skip 
void ARDUINO_ISR_ATTR buttonNext() {
  unsigned long now = millis();
  if (now - lastPressTime > DEBOUNCE_DELAY) {
    mp3.next();
    isPlaying = true;
    uint8_t count = (currentFolder == 1) ? playlist1Count : playlist2Count;
    if (count > 0) {
      currentTrackIndex = (currentTrackIndex + 1) % count;
    }
    displayNeedsUpdate = true;
  }
  lastPressTime = now;
}

void setup() {
  pinMode(pinBtnPlay, INPUT_PULLUP);
  pinMode(pinBtnPrev, INPUT_PULLUP);
  pinMode(pinBtnNext, INPUT_PULLUP);

  attachInterrupt(pinBtnPlay, buttonPlay, HIGH);
  attachInterrupt(pinBtnPrev, buttonPrev, HIGH);
  attachInterrupt(pinBtnNext, buttonNext, HIGH);

  mp3 = YX5300_ESP32(Serial2, RX, TX);

  Serial.begin(115200);
  mp3.enableDebugging();
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {  // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }
  delay(2000);
  display.clearDisplay();

  updateDisplay();
  displayNeedsUpdate = false;

  mp3.playFolderInLoop(1);
}

void loop() {
  //volume
  int raw_value = analogRead(potVolume);

  int new_volume = map(raw_value, 0, 4095, MIN_VOLUME, MAX_VOLUME);

  if (new_volume != current_volume) {
    mp3.setVolume(new_volume);
    current_volume = new_volume;
  }

  if (displayNeedsUpdate) {
    updateDisplay();
    displayNeedsUpdate = false;
  }
}