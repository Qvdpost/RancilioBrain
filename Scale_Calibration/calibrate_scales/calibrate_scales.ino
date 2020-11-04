//
//    FILE: HX_kitchen_scale.ino
//  AUTHOR: Rob Tillaart
// VERSION: 0.1.0
// PURPOSE: HX711 demo
//     URL: https://github.com/RobTillaart/HX711
//
// HISTORY:
// 0.1.0    2020-06-16 initial version
//

#include "HX711.h"

HX711 scale;

uint8_t dataPin = 11;
uint8_t clockPin = 12;

float callibration_weight = 30.8;


void setup()
{
  Serial.begin(9600);

  scale.begin(dataPin, clockPin);

  scale.set_scale(1000);

  Serial.print("UNITS: ");
  Serial.println(scale.get_units());
  
  Serial.println("\nEmpty the scale, press a key to continue");
  while(!Serial.available());
  while(Serial.available()) Serial.read();
  
  scale.tare();
  Serial.print("UNITS: ");
  Serial.println(scale.get_units());


  Serial.print("\nPut ");
  Serial.print(callibration_weight);
  Serial.println(" grams on the scale, then press a key to continue");
  while(!Serial.available());
  while(Serial.available()) Serial.read();

  scale.callibrate_scale(callibration_weight);
  Serial.print("UNITS: ");
  Serial.println(scale.get_units(10));

  Serial.println("\nScale is callibrated, press a key to continue");
  while(!Serial.available());
  while(Serial.available()) Serial.read();

}

void loop()
{
  Serial.print("UNITS: ");
  Serial.print(scale.get_units());
  Serial.print("\tSCALE: ");
  Serial.print(scale.get_scale());
  Serial.print("\tOFFSET: ");
  Serial.println(scale.get_offset());
  delay(250);
}

// END OF FILE
