// Requires Adafruit_NeoPixel library in /<arduino_home>/libraries
#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
  #include <avr/power.h>
#endif

#define NEO_PIN 5
#define PROXIMITY_THRESHOLD 150
#define MIN_BRIGHTNESS 10
#define MAX_BRIGHTNESS 75
#define filterSamples 27                // filterSamples should  be an odd number, no smaller than 3
int sensSmoothArray1 [filterSamples];   // array for holding raw sensor values for ping
int smoothed;                           // variables for ping data
const int pingPin = 7;
#define STRIP_PIXELS_PER_METER 60
#define STRIP_METER_COUNT 1
#define LED_COUNT STRIP_PIXELS_PER_METER*STRIP_METER_COUNT
#define LONG_STRIP_BITSTREAM NEO_KHZ800
#define SHORT_STRIP_BITSTREAM NEO_KHZ400

// Parameter 1 = number of pixels in strip
// Parameter 2 = Arduino pin number (most are valid)
// Parameter 3 = pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
Adafruit_NeoPixel strip = Adafruit_NeoPixel(LED_COUNT, NEO_PIN, NEO_GRB + SHORT_STRIP_BITSTREAM);
int brightness = MIN_BRIGHTNESS;
bool neo = HIGH;
bool idle = LOW;
int incomingByte = 0;   // for incoming serial data

void setup() {
  // initialize serial communication:
  Serial.begin(9600);
  strip.begin();

  colorWipe(strip.Color(127, 127, 127), 20);
  
  for(uint16_t i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, strip.Color(0, 127, 0)); //  GREEN
  }

//  setColorAll(strip.Color(0, 127, 0));
  strip.setBrightness(20);
  strip.show();

//  delay(2000);

//  setColorAll(strip.Color(MIN_BRIGHTNESS, MIN_BRIGHTNESS, MIN_BRIGHTNESS));
//  strip.setBrightness(20);
//  strip.show();
}

void setColorAll(uint32_t color) {
  for(uint16_t i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, color);
  }
}

// Fill the dots one after the other with a color
void colorWipe(uint32_t c, uint8_t wait) {
  for(uint16_t i=0; i<strip.numPixels(); i++) {
    strip.setPixelColor(i, c);
    strip.show();
    delay(wait);
  }
}

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if(WheelPos < 85) {
    return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if(WheelPos < 170) {
    WheelPos -= 85;
    return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}

void rainbow(uint8_t wait) {
  uint16_t i, j;

  for(j=0; j<256; j++) {
    for(i=0; i<strip.numPixels(); i++) {
      strip.setPixelColor(i, Wheel((i+j) & 255));
    }
    strip.show();
    delay(wait);
  }
}

void loop() {

  // TODO: Check for messages from serial port (or use an interrupt)
  //       If there is a message, run the response routine,
//       i.e. flash the NeoPixels red if foodCam or HB100 trigger

  // send data only when you receive data:
  if (Serial.available() > 0) {
    // read the incoming byte:
    incomingByte = Serial.read();

    // say what you got:
    Serial.print("I received: ");
    Serial.println(incomingByte, DEC);
    rainbow(20);
  }
  
  // Adjust brightness every other loop to make the NeoPixel happy
  neo = !neo;
  // establish variables for duration of the ping,
  // and the distance result in inches and centimeters:
  long duration, inches, cm;

  // The PING))) is triggered by a HIGH pulse of 2 or more microseconds.
  // Give a short LOW pulse beforehand to ensure a clean HIGH pulse:
  pinMode(pingPin, OUTPUT);
  digitalWrite(pingPin, LOW);
  delayMicroseconds(2);
  digitalWrite(pingPin, HIGH);
  delayMicroseconds(5);
  digitalWrite(pingPin, LOW);

  // The same pin is used to read the signal from the PING))): a HIGH
  // pulse whose duration is the time (in microseconds) from the sending
  // of the ping to the reception of its echo off of an object.
  pinMode(pingPin, INPUT);
  duration = pulseIn(pingPin, HIGH);

  smoothed = digitalSmooth(duration, sensSmoothArray1);
  cm = microsecondsToCentimeters(smoothed);

  if(neo && cm <= PROXIMITY_THRESHOLD) {
    if(idle) {
      idle = LOW;
      setColorAll(strip.Color(0, 0, 127));  // BLUE
    }
    // Clamp the value to our upper limit if we get Ping values larger
    if(cm > PROXIMITY_THRESHOLD) {
      cm = PROXIMITY_THRESHOLD;
    }
    Serial.print(inches);
    Serial.print("in, ");
    Serial.print(cm);
    Serial.print("cm");
    Serial.println();
    brightness = map(cm, PROXIMITY_THRESHOLD, 10, MIN_BRIGHTNESS, MAX_BRIGHTNESS);
    strip.setBrightness(brightness);
    strip.show();
  } 

  // If we're running this cycle and there is nobody closer than the distance threshold
  if(neo && cm > PROXIMITY_THRESHOLD) {
    // TODO: This should ramp down the current brightness value until activity
    //       triggers a ramp back up again, from the block above.
    setColorAll(strip.Color(127, 0, 0));  // RED
    strip.setBrightness(MIN_BRIGHTNESS);
    strip.show();
    idle = HIGH;
  }
  
  delay(20);
}

long microsecondsToCentimeters(long microseconds) {
  // The speed of sound is 340 m/s or 29 microseconds per centimeter.
  // The ping travels out and back, so to find the distance of the
  // object we take half of the distance travelled.
  return microseconds / 29 / 2;
}

int digitalSmooth(int rawIn, int *sensSmoothArray){     // "int *sensSmoothArray" passes an array to the function - the asterisk indicates the array name is a pointer
  int j, k, temp, top, bottom;
  long total;
  static int i;
 // static int raw[filterSamples];
  static int sorted[filterSamples];
  boolean done;

  i = (i + 1) % filterSamples;    // increment counter and roll over if necc. -  % (modulo operator) rolls over variable
  sensSmoothArray[i] = rawIn;                 // input new data into the oldest slot

  // Serial.print("raw = ");

  for (j=0; j<filterSamples; j++){     // transfer data array into anther array for sorting and averaging
    sorted[j] = sensSmoothArray[j];
  }

  done = 0;                // flag to know when we're done sorting              
  while(done != 1){        // simple swap sort, sorts numbers from lowest to highest
    done = 1;
    for (j = 0; j < (filterSamples - 1); j++){
      if (sorted[j] > sorted[j + 1]){     // numbers are out of order - swap
        temp = sorted[j + 1];
        sorted [j+1] =  sorted[j] ;
        sorted [j] = temp;
        done = 0;
      }
    }
  }

/*
  for (j = 0; j < (filterSamples); j++){    // print the array to debug
    Serial.print(sorted[j]); 
    Serial.print("   "); 
  }
  Serial.println();
*/

  // throw out top and bottom 15% of samples - limit to throw out at least one from top and bottom
  bottom = max(((filterSamples * 15)  / 100), 1); 
  top = min((((filterSamples * 85) / 100) + 1  ), (filterSamples - 1));   // the + 1 is to make up for asymmetry caused by integer rounding
  k = 0;
  total = 0;
  for ( j = bottom; j< top; j++){
    total += sorted[j];  // total remaining indices
    k++; 
    // Serial.print(sorted[j]); 
    // Serial.print("   "); 
  }

//  Serial.println();
//  Serial.print("average = ");
//  Serial.println(total/k);
  return total / k;    // divide by number of samples
}
