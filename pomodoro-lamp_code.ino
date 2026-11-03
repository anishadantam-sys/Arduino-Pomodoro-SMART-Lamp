#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
#include <Adafruit_NeoPixel.h>

// OLED Display Dimensions
#define SCREEN_WIDTH 128 
#define SCREEN_HEIGHT 64 

// Pin Definitions
#define button_SP 4
#define button_reset 5
#define led_pin 6
#define pir_pin 7
#define ldr_pin A0
#define led_count 60

// Buttons 
unsigned long lastBtn1Press = 0; 
unsigned long lastBtn2Press = 0;
#define debounceTime 200

// Constants
#define work_duration 25UL * 60UL * 1000UL 
#define break_duration 5UL * 60UL * 1000UL 
#define motion_timeout 90UL * 1000UL

// Objects
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
Adafruit_NeoPixel strip(led_count, led_pin, NEO_GRB + NEO_KHZ800);

// Timing
unsigned long stateStartTime = 0; 
unsigned long pausedTimeRemaining = 0; 
unsigned long lastMotionTime = 0;

// State Machine
enum State {IDLE, WORK, BREAK, PAUSED, NO_MOTION};
State currentState = IDLE;
State stateBeforePause = IDLE;
State stateBeforeAway = IDLE;

unsigned long remainingTime(unsigned long total, unsigned long now) {
  unsigned long elapsed = now - stateStartTime;
  return (elapsed < total) ? (total - elapsed) : 0;
}

void setup() {
  Serial.begin(9600); 

  // Pin Setup
  pinMode(pir_pin, INPUT);
  pinMode(button_SP,INPUT_PULLUP);
  pinMode(button_reset, INPUT_PULLUP);

  // NeoPixel LED Setup
  strip.begin(); 
  strip.setBrightness(100);
  strip.show();

  // OLED Setup
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay(); 
  display.setTextColor(WHITE);

  lastMotionTime = millis();
}

void loop() {

  unsigned long now = millis();

// Photoresistor (LDR) Reading & AutoBrightness Calculation
  int ldrValue = analogRead(ldr_pin); 
  int brightness = map(ldrValue, 0, 1023, 255, 30);
  int scale = map(brightness, 0,255, 0,100);

// Checking for Motion
  if (digitalRead(pir_pin) == HIGH) {
    lastMotionTime = now; 
    if (currentState == NO_MOTION) {
      if (stateBeforeAway == WORK) {
        stateStartTime = now - (work_duration - pausedTimeRemaining);
      } else if (stateBeforeAway == BREAK) {
        stateStartTime = now - (break_duration - pausedTimeRemaining);
      }
      currentState = stateBeforeAway;
    }
  } else if (currentState != IDLE && currentState != NO_MOTION && now - lastMotionTime > motion_timeout) {
    stateBeforeAway = currentState;
    if (currentState == WORK) {
      pausedTimeRemaining = remainingTime(work_duration, now);
    } else if (currentState == BREAK) {
      pausedTimeRemaining = remainingTime(break_duration, now);
    }
    currentState = NO_MOTION;
  }
  

// BUTTON HANDLING

  // Button 1: Start / Pause / Resume
  if (digitalRead(button_SP) == LOW && now - lastBtn1Press > debounceTime) {
    lastBtn1Press = now; 
    lastMotionTime = now;
    if (currentState == IDLE) {
      // Start the timer for working
      currentState = WORK;
      stateStartTime = now;
    }

    else if (currentState == WORK) {
      // Pausing during work
      pausedTimeRemaining = work_duration - (now - stateStartTime);
      stateBeforePause = WORK;
      currentState = PAUSED;
    }

    else if (currentState == BREAK) {
      // Pausing during break
      pausedTimeRemaining = break_duration - (now - stateStartTime); 
      stateBeforePause = BREAK; 
      currentState = PAUSED; 
    }

    else if (currentState == PAUSED) {
      // resuming either work or break timer
      if (stateBeforePause == WORK) {
        stateStartTime = now - (work_duration - pausedTimeRemaining); 
      } else {
        stateStartTime = now - (break_duration - pausedTimeRemaining); 
      }
      currentState = stateBeforePause;
    }
  }

  // Button 2: Reset

  if (digitalRead(button_reset) == LOW && now - lastBtn2Press > debounceTime) {
    lastBtn2Press = now; 
    currentState = IDLE; 
    stateStartTime = 0; 
    pausedTimeRemaining = 0;
  }

// Building the State Machine Logic 

  switch (currentState) {
    case IDLE:
      strip.fill(strip.Color(50*scale,50*scale,50*scale)); // light is white
      strip.show(); 
      updateOLED("IDLE", "Press START",0); 
      break;
    case WORK: {
      strip.fill(strip.Color(0,200*scale,0)); // light turns green
      strip.show();
      unsigned long remaining = 0;
      if (now - stateStartTime < work_duration) {
        remaining = work_duration - (now - stateStartTime);
      }
      updateOLED("WORK", "Focus :)",remaining);

      if (now - stateStartTime >= work_duration) {
        currentState = BREAK; 
        stateStartTime = now;
      }
      break;
    }
    case BREAK: {
      strip.fill(strip.Color(200*scale,0,0)); // light turns red
      strip.show();
      unsigned long remaining = 0;
      if (now - stateStartTime < break_duration) {
        remaining = break_duration - (now - stateStartTime);
      }
      updateOLED("BREAK","Rest!", remaining);
      if (now - stateStartTime >= break_duration) {
        currentState = WORK; 
        stateStartTime = now;
      }
      break;
    }
    case PAUSED:
      strip.fill(strip.Color(80*scale,80*scale,0)); // light turns yellow
      strip.show();
      updateOLED("PAUSED", "Paused", pausedTimeRemaining); 
      break;
    case NO_MOTION:
      strip.fill(strip.Color(0,0,0)); 
      strip.show(); 
      updateOLED("AWAY", "No motion", 0);
      break;
  }
  delay(50);


}

// OLED Update Function
void updateOLED(String stateName, String messageOutput, unsigned long msRemaining) {
  display.clearDisplay();
  display.setTextSize(2); 
  display.setCursor(0,0); 
  display.println(stateName); 

  display.setTextSize(1); 
  display.setCursor(0,30); 
  display.println(messageOutput);

  if (msRemaining > 0) {
    unsigned long totalSeconds = msRemaining / 1000; 
    int mins = totalSeconds / 60;
    int secs = totalSeconds % 60; 
    display.setCursor(0,50); 
    if (mins < 10) {
      display.print("0"); 
    }
    display.print(mins); 
    display.print(":"); 
    if (secs < 10) {
      display.print("0");
    }
    display.print(secs);
    display.println(" remaining");
  }
  display.display();
}
