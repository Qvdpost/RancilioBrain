// OLED control library
#include <Adafruit_SSD1306.h>

// Arduino flash memory control library
// This is being used to store the target weight so when arduino power cycles it retains last used weight
#include <EEPROM.h>

// this is the little board that comes with each load cell
// URL: https://github.com/RobTillaart/HX711
#include "HX711.h"


// OLED-screen setup
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
// Reset pin # (or -1 if sharing Arduino reset pin)
#define OLED_RESET     4
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Scale setup
HX711 scale;
HX711 scale2;
uint8_t dataPin = 5;
uint8_t clockPin = 6;
uint8_t dataPin2 = 11;
uint8_t clockPin2 = 12;
// This is the calibration value for each load cell, you need to calibrate
// them with a known weight The good thing is that the value is linear so if
// you know something weights 100g you can calculate what the value should be.
#define scaleCalibration 2040.54
#define scaleCalibration2 1986.60



// the variable to hold a value for each load cell (float because it is a decimal)
float w1;
float w2;

// Weight control variables
int currentWeight;
int lastWeight;
bool weightReached = false;
int targetWeight;
int weightOffset = 0; // This value is how many decigrams before the target should the pump shut off, you need to test


/*
   Macros and globals needed for the offset hashtable based on flowrate
*/
// The interval over which the flow is calculated
#define flowTime 2
// Maximum flowrate to remember offset for in centigrams per second
#define maxFlow 500
// Flow rate bucketsize in centigrams per second
#define flowInterval 20
// starting postion in EEPROM to write the offset hashtable
#define hashOffset 10
// Delta in offsets to warrant a new write to EEPROM
#define offsetPrecision 20

int hashMax = maxFlow / flowInterval;
int hashStep = flowInterval;
int weightFlow[flowTime] = {0};

// Flow of coffee measured as centigram per flowTime - 1
int currentFlow;
int lastFlow;



// Timing control variables
const unsigned long eventInterval = 100;
const unsigned long reachedInterval = 5000; // DEPRECATED: (will be changed into flowrate) how long to keep reading and updating the weight after the target weight is reached in milliseconds
unsigned long previousTime = 0;
unsigned long startCounting;
unsigned long currentTime;
unsigned int currentSecond = 0;
// NOTE: tV is interchangeable with elapsedTime; why do both exist?
unsigned long elapsedTime;
unsigned long tV = 0;

// This is the weight that arduino will default to when powered on
int storedWeight;


// This is rudimentary state logic
bool started = false;
bool preStarted = false;


// The rotary encoder
int buttonPin = 4;
int buttonPinState = 1;
int buttonPinLast = 1;
bool buttonPressed = 0;
int pinA = 2;
int pinB = 3;
int pinAStateCurrent = HIGH;
int pinAStateLast = pinAStateCurrent;
bool turnDetected = false;


// The Relays
int pump = 9;
int valve = 10;

// Helper functions that turn the relays on and off
void extractOn() {
  digitalWrite(pump, HIGH);
  digitalWrite(valve, HIGH);
}

void extractOff() {
  digitalWrite(pump, LOW);
  digitalWrite(valve, LOW);
}



/***
   These are the different layouts for each screen that we will use,
   helps keep our code more readable to break them out like this
 ***/

/*
   When you first turn the machine on
*/
void preStartedDisplay() {
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE); // Draw white text
  display.setCursor(0, 0);
  display.print("Target Weight");
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE); // Draw white text
  display.setCursor(0, 14);
  display.print(targetWeight * 0.01, 1);


  // if you've pulled a shot already put the info on screen (if TV isnt 0 you've pulled a shot)
  if (tV != 0) {
    display.setTextSize(1); // this is size..  1 is 8 pixels tall, 2 is 16, and so on
    display.setTextColor(SSD1306_WHITE); // Draw white text
    display.setCursor(0, 32); // this is position in (x,y)
    display.print("Previous Weight"); //display.print sends this to buffer
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE); // Draw white text
    display.setCursor(98, 32);
    display.print(lastWeight * 0.01, 1);
    display.setTextSize(1); 
    display.setTextColor(SSD1306_WHITE); // Draw white text
    display.setCursor(0, 44); 
    display.print("Previous Flow"); 
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE); // Draw white text
    display.setCursor(98, 44);
    display.print(lastFlow * 0.01, 1);
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE); // Draw white text
    display.setCursor(0, 56);
    display.print("Previous Time");
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE); // Draw white text
    display.setCursor(98, 56);
    display.print(tV / 1000.0, 1);
  }
}

/*
   When the shot is being pulled this is what is shown
*/
void startedDisplay() {

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE); // Draw white text
  display.setCursor(0, 0);
  display.print("Target Weight");
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE); // Draw white text
  display.setCursor(85, 0);
  display.print(targetWeight * 0.01, 1);
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE); // Draw white text
  display.setCursor(0, 22);
  display.print("Weight");
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE); // Draw white text
  display.setCursor(80, 22);
  display.print(currentWeight * 0.01, 1);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE); // Draw white text
  display.setCursor(80, 34);
  display.print(currentFlow * 0.01, 1);
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE); // Draw white text
  display.setCursor(0, 46);
  display.print("Time");
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE); // Draw white text
  display.setCursor(80, 46);
  display.print(tV / 1000.0, 1);
}

/*
   This is identical to when a shot is pulled only we switch to a more precise method of reading the scale (called lastWeight)
   NOTE: This can be merged somehow with startedDisplay()
*/
void justEndedDisplay() {
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE); // Draw white text
  display.setCursor(0, 0);
  display.print("Target Weight");
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE); // Draw white text
  display.setCursor(85, 0);
  display.print(targetWeight * 0.01, 1);
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE); // Draw white text
  display.setCursor(0, 22);
  display.print("Weight");
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE); // Draw white text
  display.setCursor(80, 22);
  display.print(lastWeight * 0.01, 1);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE); // Draw white text
  display.setCursor(80, 34);
  display.print(lastFlow * 0.01, 1);
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE); // Draw white text
  display.setCursor(0, 46);
  display.print("Time");
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE); // Draw white text
  display.setCursor(80, 46);
  display.print(tV / 1000.0, 1);

}


/*
   This is just that text "starting" that appears after you click the encoder but before the pump is on.
*/
void startingDisplay() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE); // Draw white text
  display.setCursor(15, 18);
  display.print("Starting");
  display.display();
}


/*
   This function adds or subtracts .1 to the target weight if you turn left or right
*/
void increment() {
  // NOTE: What is this for? Does not seem to be used anywhere.
  turnDetected = true;

  // Rotation
  pinAStateCurrent = digitalRead(pinA);
  if ((pinAStateLast == LOW) && (pinAStateCurrent == HIGH)) {
    if (digitalRead(pinB) == HIGH) {
      targetWeight -= 100;
      delay(15);
    } else {
      targetWeight += 100;
      delay(15);
    }
  }
  pinAStateLast = pinAStateCurrent;
}

/*
   This function checks to see if the button was pressed
*/
void buttonDetect() {
  buttonPinState = digitalRead(buttonPin);

  if (buttonPinState != buttonPinLast) {
    // Extraction just started
    if ((buttonPinState == 1) && (!started)) {
      preStarted = false;
      startingDisplay();
      tareScales();
      delay(250);
      tV = 0;
      currentWeight = 0;
      lastFlow = 0;
      elapsedTime = 0;
      preStarted = false;
      started = true;
      extractOn();
      startCounting = millis();
      display.clearDisplay();
    }

    // Manual stop on extraction
    if ((elapsedTime > 1.0) && (buttonPinState == 1) && (started)) {
      weightReached = true;
    }
    buttonPinLast = buttonPinState;
  }
}


/*
   This is the interupt that we create in our setup, the conditional statement means if you rotate the encoder
   while a shot is extracting it wont run any code and cause lag
*/
void update() {
  if (preStarted) {
    increment();
  }
}

// --------------//--------------//--------------//--------------//--------------//--------------//--------------//--------------//--------------
// --------------//--------------//--------------//--------------//--------------//--------------//--------------//--------------//--------------

void setup() {
  Serial.begin(9600);

  // display setup
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3C for 128x32
    Serial.println(F("SSD1306 allocation failed"));
    for (;;); // Don't proceed, loop forever

  }

  // This is seeing if this is the first time you are writing to the eeprom and if it is just put a 0 for now
  if (EEPROM.read(256) != 123) {
    EEPROM.write(256, 123);
    storedWeight = 0;
  }

  // This code runs every time the arduino powers back on... it sets the target weight to the last weight used so that it "remembers"
  else {
    EEPROM.get(0, storedWeight);
  }

  targetWeight = storedWeight;

  // setting up the relays
  pinMode(pump, OUTPUT);
  pinMode(valve, OUTPUT);

  // making sure the pump and valve are off, probably dont need but just in case
  extractOff();

  // initial display
  display.clearDisplay();
  preStartedDisplay();
  display.display();

  // This is our rotary encoder setup
  pinMode(buttonPin, INPUT_PULLUP); // initialize the button pin as a input
  pinMode(pinA, INPUT_PULLUP);
  pinMode(pinB, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(pinA), update, CHANGE);


  // This initializes the scales
  scale.begin(dataPin, clockPin);
  scale2.begin(dataPin2, clockPin2);

  // Calibrate to initially weighted value
  scale.set_scale(scaleCalibration);
  scale2.set_scale(scaleCalibration2);

  // set the scales to 0
  scale.tare();
  scale2.tare();

  // Set the state of the system
  preStarted = true;
}

// --------------//--------------//--------------//--------------//--------------//--------------//--------------//--------------//--------------//--------------
// --------------//--------------//--------------//--------------//--------------//--------------//--------------//--------------//--------------//--------------

void loop() {

  // check to see if extraction is running and perform a get weight if it is
  if ((preStarted == false) && (!weightReached)) {
    scaleFunc();
  }
  // check to see if the shot just ended and switch to doing multiple averaged readings for more accuracy
  else if ((preStarted == false) && (weightReached)) {
    precisionScale();
  }

  currentTime = millis();

  // check if button is pressed and run the button detect code we defined earlier
  buttonDetect();

  // Updates the time and weight value while the extraction is happening
  if (started) {
    updateWeightFlow();
    
    // this is to only start counting extraction time from the first drips into the cup
    if (currentWeight < 1) {
      startCounting = currentTime;
    }
    
    if (!weightReached) {
      elapsedTime = currentTime - startCounting;
      tV = elapsedTime;
      previousTime = currentTime;
      display.clearDisplay();
      startedDisplay();
      display.display();
    }
  }

  // This is the code to stop the extraction once the target weight is reached.
  // I added the conditional to only runs 5s after extraction starts because
  // sometimes you need to reposition your cup and that would trip
  // the weight and stop extraction and ruin your shot :(

  if (tV > 2000) {
    readWeightOffset();
    if ((currentWeight) > (targetWeight - weightOffset)) {
      lastWeight = currentWeight;
      if (lastFlow == 0) {
        lastFlow = currentFlow;
      }
      weightReached = true;
      if (storedWeight != targetWeight) {
        storedWeight = targetWeight;
        EEPROM.put(0, storedWeight);
      }
    }
  }

  // Updates the weight for a few more moments just after the shot has stopped extracting and timer has stopped
  if (weightReached && started) {
    extractOff();
    display.clearDisplay();
    justEndedDisplay();
    display.display();
    if (currentWeight > lastWeight) {
      lastWeight = currentWeight;
    }
  }

  /* this code runs after your weight is reached AND a certain extra
     amount of time has passed, you know for the last few drips...
  */
  if (weightReached && currentFlow < 20) {
    started = 0;
    weightReached = false;
    preStarted = true;
    writeWeightOffset();
    clearWeightFlow();
    currentWeight = 0;
  }

  // Goes back to the initial standby screen when you are finished extracting so you can set another weight
  if (!started) {
    display.clearDisplay();
    preStartedDisplay();
    display.display();
  }

}

// --------------//--------------//--------------//--------------//--------------//--------------//--------------//--------------//--------------//--------------
// --------------//--------------//--------------//--------------//--------------//--------------//--------------//--------------//--------------//--------------

/***
   A few helper functions
*/

/*
   This asks the scales to get the weight and add them up
*/
void scaleFunc() {
  w1 = scale.get_units() + 0.6;
  w2 = scale2.get_units();
  if ((w1 + w2) > 1) {
    currentWeight = round((w1 * 100 + w2 * 100));
  }
}

/*
   This asks the scales to check the weight twice so in theory its more accurate, this is how the shot is weighed just after the pump is off
*/
void precisionScale() {
  w1 = scale.get_units(2) + 0.6;
  w2 = scale2.get_units(2);
  currentWeight = round((w1 * 100 + w2 * 100));
}

/*
   We tare the cells just after the rotary knob is clicked
*/
void tareScales() {
  scale.tare();
  scale2.tare();
}

/*
   Keeps track of the last N seconds of weights and recalculateds the flow
*/
void updateWeightFlow() {
  if (currentTime / 1000 != currentSecond) {
    currentSecond = currentTime / 1000;
    weightFlow[currentSecond % flowTime] = currentWeight;
    calcCurrentWeightFlow();
  }
}

/*
   Clears the weightFlow array for a new extraction
*/
void clearWeightFlow() {
  for (int i = 0; i < flowTime; i++) {
    weightFlow[i] = 0;
  }
}

/*
   Returns the flow over an interval of N seconds (measurements taken at the start of each second)
*/
void calcCurrentWeightFlow() {
  int minVal = weightFlow[0];
  int maxVal = weightFlow[0];

  for (int i = 1; i < flowTime; i++) {
    minVal = min(minVal, weightFlow[i]);
    maxVal = max(maxVal, weightFlow[i]);
  }

  // Average weight difference over interval flowTime
  // (e.g. when flowTime is 2 you have measured 1 second of weight difference)
  currentFlow = (maxVal - minVal) / (flowTime - 1);
}

/*
   Calculates an index in EEPROM memory based on the current flowrate
*/
uint8_t hashFlow() {
  uint8_t hash = (currentFlow / hashStep);
  return (hash < hashMax ? hash : hashMax) + hashOffset;
}

/*
   Calculates an index in EEPROM memory based on the last flowrate during extraction
*/
uint8_t hashLastFlow() {
  uint8_t hash = (lastFlow / hashStep);
  return (hash < hashMax ? hash : hashMax) + hashOffset;
}

/*
   Reads the weightOffset from EEPROM memory related to the current flowrate
*/
void readWeightOffset() {
  int offset = EEPROM.read(hashFlow());
  // Convert offset from decigrams to centigrams
  weightOffset = offset != 255 ? offset * 10 : 0;
}

/*
   Writes the final weight overflow (drip) to EEPROM memory based on the flowrate
   before target weight was reached
*/
void writeWeightOffset() {
  uint8_t offset = EEPROM.read(hashLastFlow());
  int currentOffset = currentWeight - targetWeight;

  /* The final weight was less than the weight during extraction (e.g. someone took away the cup)
     Or the overflow too great (e.g. someone leaned on the scale)
  */
  if (currentWeight < lastWeight - offsetPrecision || currentOffset > 1.5 * targetWeight) {
    return;
  }

  int deltaOffset = currentOffset - offset;

  // Write the offset to EEPROM in decigrams
  if (abs(deltaOffset) > offsetPrecision) {
    // Convert offset to decigrams
    currentOffset = currentOffset / 10;

    // 255 is reserved for unset values so 254 is chosen if the value is 255 or higher due to EEPROM limitations
    currentOffset = currentOffset < 255 ? currentOffset : 254;

    // if extraction stopped too early subtract it (add a negative value) to the offset
    if (currentOffset < 0) {
      currentOffset = max(0, offset + currentOffset);
    }
    
    EEPROM.write(hashLastFlow(), currentOffset);
  }
}
