/*

 With arduino IDE 1.8.13 (ie. with package manager)
 
 Arduino VFO with ...
 1) rotary encoder for controlling frequency (using 2 pins to trigger interrupts)
 2) oled screen to show current frequency (+ 5v-3.3v logic level converter since display wants 5v)
    https://uk.farnell.com/midas/mdob128032gv-wi/oled-display-cob-128-x-32-pixel/dp/3407291
 3) si5351 oscillator 

 Uses
  - https://github.com/etherkit/Si5351Arduino (radio-focused library) v2.1.4
  - Adafruit SSD1306 for the oled display v2.5.1
  - Adafruit_GFX 1.10.13 
  - (and dreams of maven)

*/

#include "si5351.h"
#include "Wire.h"

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
#define OLED_RESET     4 // Reset pin # (or -1 if sharing Arduino reset pin)

Si5351 si5351;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Rotary encoder is connected to these two pins, which we'll enable for interrupts.
const int encoderPinA = 7;
const int encoderPinB = 6;

// State to remember what the last rotary encoder 'position' was, so we can tell if we're going clockwise or anticlockwise
boolean prevA = true;
boolean prevB = true;

// As we turn the rotary encoder, [val] will go up and down.  We then derived the frequency from it.
long val = 0;
long lastVal = -1;

unsigned long long INITIAL_FREQUENCY = 14000000ULL * 100ULL;

// Calibration value (parts-per-billion) required to make my si5351 nail the frequency (measured via freq counter)
signed long CALIBRATION_PPB = 30785;

void setup(void)
{
  Serial.begin(9600);
  Serial.println("Hello, world!"); 
  Serial.println("");

  // OLED display
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3C for 128x32
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
  display.clearDisplay();

  PCICR |= 0b00000100;    // turn on port d
  PCMSK2 |= 0b11000000;    // turn on pin PB0, which is PCINT0, physical pin 14

  pinMode(encoderPinA, INPUT);
  pinMode(encoderPinB, INPUT);
  digitalWrite(encoderPinA, HIGH);
  digitalWrite(encoderPinB, HIGH);

  boolean i2c_found = si5351.init(SI5351_CRYSTAL_LOAD_8PF, 0, CALIBRATION_PPB);

  loop();
}


// Interrupt handler for when either of the two rotary encoder pins change
// Based on what the two pin are now, and what they were last time, decide if we're going cw or ccw.
ISR(PCINT2_vect) {
  boolean newA = digitalRead(encoderPinA);
  boolean newB = digitalRead(encoderPinB);

  // Make a nibble with the old+new values, just so it's easy to do a case-statment
  int i = (prevA << 3) | (prevB << 2) | (newA << 1) | newB;

  switch (i) {
    case 0b0010:
    case 0b1011:
    case 0b1101:
    case 0b0100:
      val++;
      break;
    case 0b0001:
    case 0b0111:
    case 0b1110:
    case 0b1000:
      val--;
      break;
  }
  

  prevA = newA;
  prevB = newB;
}


void loop(void)
{
  if (lastVal != val) {

    unsigned long long stepHz = 50;
    unsigned long long delta = (val * stepHz * 100UL);
    unsigned long long freq = INITIAL_FREQUENCY + delta;  
    
    si5351.set_freq( freq, SI5351_CLK0);

    display.clearDisplay();
    display.setTextSize(2);             // Normal 1:1 pixel scale
    display.setTextColor(SSD1306_WHITE);        // Draw white text
    display.setCursor(0,0);             // Start at top-left corner
    long freqHz = freq / 100;
    display.print(freqHz);
    display.display();
  
    lastVal = val;
  }
}
