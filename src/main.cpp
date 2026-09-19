#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <YX5300_ESP32.h>

// Connect to YX5300 Serial pins (RX and TX)
#define RX 16
#define TX 17

// Global variables for the pushbuttons
const uint8_t pinBtnPlay = 18;
const uint8_t pinBtnPrev = 5;
const uint8_t pinBtnNext = 19;

// Bool variable to track if the audio is playing or paused
volatile bool isPlaying = true;

byte currentFolder = 1;

// ---- Song name lookup tables ----
// The mp3 module has no way to report back a filename or title - it only knows
// numeric track positions. List your songs here, in the same order as the files
// in each folder (lowest-numbered file first), so the display can show a name.
const char* playlist1Songs[] = {"Motion - CORTIS", "weigex my time - ALNST x bo en"};
const char* playlist2Songs[] = {"Anywhere But Home - 슬기"};
const char* playlist3Songs[] = {"Heart - C!naH", "Dreamer - TXT", "밤소풍-ILLIT"};

const uint8_t playlist1Count = sizeof(playlist1Songs) / sizeof(playlist1Songs[0]);
const uint8_t playlist2Count = sizeof(playlist2Songs) / sizeof(playlist2Songs[0]);
const uint8_t playlist3Count = sizeof(playlist3Songs) / sizeof(playlist3Songs[0]);

// 0-based index of the current track within currentFolder.
// Kept in sync with the module's own next()/prev() wraparound so the display
// always matches what's actually playing.
volatile int currentTrackIndex = 0;

// Set by the ISRs whenever the song changes; cleared by loop() after it
// refreshes the OLED (I2C calls are avoided inside the ISRs themselves).
volatile bool displayNeedsUpdate = true;

// Variables for debouncing the pushbuttons
const unsigned long DEBOUNCE_DELAY = 500;  // in milliseconds
volatile unsigned long lastPressTime = 0;

// Analog input pin the potentiometer is attached to
const uint8_t potVolume = 4;

// Auxiliary volume variables
#define MIN_VOLUME 0
#define MAX_VOLUME 30
int current_volume = 0;

// screen
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define I2C_SDA 21
#define I2C_SCL 22

// joystick (placeholders - assign real GPIO numbers when you wire it up)
// NOTE: the old "#define switch" etc. were removed because defining C++
// keywords as empty macros will break your code as soon as you use them.
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

// Play/Pause Button Interrupt Service Routine (ISR)
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

// Next Track Button Interrupt Service Routine (ISR)
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
  // Init pushbuttons with internal pullup resistor
  pinMode(pinBtnPlay, INPUT_PULLUP);
  pinMode(pinBtnPrev, INPUT_PULLUP);
  pinMode(pinBtnNext, INPUT_PULLUP);

  // Set interrupts ISR functions
  attachInterrupt(pinBtnPlay, buttonPlay, HIGH);
  attachInterrupt(pinBtnPrev, buttonPrev, HIGH);
  attachInterrupt(pinBtnNext, buttonNext, HIGH);

  // Initialize connection with the YX5300/YX6300 module
  mp3 = YX5300_ESP32(Serial2, RX, TX);

  // Shows the hex commands being sent to the mp3 device (includes helpful errors)
  Serial.begin(115200);
  mp3.enableDebugging();
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {  // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }
  delay(2000);
  display.clearDisplay();

  // Draw the initial playlist / song name
  updateDisplay();
  displayNeedsUpdate = false;

  // Plays the first track stored on the SD Card (Track Formats .mp3/.wav | Frequencies 8-48 kHz)
  // SD Card folder and file structure, example folder name 01 and file name 001xxx.mp3:
  // 01/001xxx.mp3   01/002xxx.mp3   01/003xxx.mp3   02/004xxx.mp3
  mp3.playFolderInLoop(1);
}

void loop() {
  // Read the potentiometer's value
  int raw_value = analogRead(potVolume);

  // Map the value to the min/max of the mp3 device's volume values
  int new_volume = map(raw_value, 0, 4095, MIN_VOLUME, MAX_VOLUME);

  // Change the volume if the pot position has changed
  if (new_volume != current_volume) {
    // Update the mp3 device's volume
    mp3.setVolume(new_volume);
    // Update the current volume with the new volume
    current_volume = new_volume;
  }

  // Refresh the OLED whenever the current track has changed
  if (displayNeedsUpdate) {
    updateDisplay();
    displayNeedsUpdate = false;
  }
}