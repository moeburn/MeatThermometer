#include <Average.h>
#include <Arduino.h>
#include "SH1106Wire.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include <NonBlockingDallas.h>   
#include <Adafruit_ADS1X15.h>

#define NUMSAMPLES 100
Average<float> sampleAvg(NUMSAMPLES);

Adafruit_ADS1115 ads;  /* Use this for the 16-bit version */
#define ONE_WIRE_BUS 2       //PIN of the Maxim DS18B20 temperature sensor 


OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature dallasTemp(&oneWire);
NonBlockingDallas sensorDs18b20(&dallasTemp); 

 SH1106Wire display(0x3c, SDA, SCL);
float  rawReading;
float calibratedReading;
float dallastemp;
double R2, probetemp;
float V;

long double STEINHART_HART_COEF_A = 0.7322291889E-3;
long double STEINHART_HART_COEF_B = 2.132158182E-4;
long double STEINHART_HART_COEF_C = 1.148231681E-7;

// This example uses a DAC output (pin 25) as a source and feed into ADC (pin 35)
// The calibrated value of the ADC is generated through LUT based on raw reading from the ADC


void handleTemperatureChange(float temperature, bool valid, int deviceIndex){  //When DS18B20 temperature has changed...
dallastemp = temperature;

      Serial.print(rawReading);  //print the serial stuff only once every DS18B20 change so we don't flood the serial monitor with noise
      Serial.print(F(","));
      Serial.print(R2);
      Serial.print(F(","));
      Serial.print(dallastemp);
      Serial.print(F(","));
      Serial.println(probetemp);
}

void setup() {
    analogReadResolution(12);
    Serial.begin(9600);
    while (!Serial) {}
    ads.setGain(GAIN_ONE);  
    ads.begin();
      sensorDs18b20.begin(NonBlockingDallas::resolution_12, NonBlockingDallas::unit_C, TIME_INTERVAL);  //use non-blocking DS18b20 library

  sensorDs18b20.onTemperatureChange(handleTemperatureChange);  //only notice when DS18B20 changes
  Serial.println("Hello");
  display.init();
  display.setFont(ArialMT_Plain_16);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  //display.flipScreenVertically();
}

void loop() {
    sensorDs18b20.update(); //Get DS18B20 sample if possible
    
    for (int i = 0; i <= NUMSAMPLES; i++) { //take NUMSAMPLES samples of analog reader...
        sampleAvg.push(ads.readADC_SingleEnded(0)); 
        //delay(1);
    }
    rawReading = sampleAvg.mean();          //...and average them.
                                            //R1 on the voltage divider circuit is a fixed 21840 ohms.
                                            //R₂ = (-V * 21840) / (V - 3.3)
                                            //32767 is the maximum analog reading, which corresponds to 4.096 volts
    V = 3.3-((rawReading/32767)*4.096);    //Calculate volts of averaged analog reading
    R2 = (-V * 21840) / (V - 3.3);        //Use volts to calculate resistance of thermistor probe
    double log_r  = log(R2);              //Use resistance of thermistor probe to calculate temperature...
    double log_r3 = log_r * log_r * log_r;
                                          //..using the Steinhart-Hart equation:
    probetemp = 1.0 / (STEINHART_HART_COEF_A + STEINHART_HART_COEF_B * log_r + STEINHART_HART_COEF_C * log_r3) - 273.15; 
       
    String rawstring = "RAW:";            //Display all this on an OLED
    String Rstring = "R:";
    String dallasstring = "DT:";
    String probestring = "T:";
    display.clear();
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.drawString(0,0, rawstring);
    display.drawString(0,16, Rstring);
    display.drawString(0,32, dallasstring);
    display.drawString(0,48, probestring);
    display.setTextAlignment(TEXT_ALIGN_RIGHT);
    rawstring = String(rawReading);
    Rstring = String(R2) + "Ω";
    dallasstring = String(dallastemp) + "°C";
    probestring = String(probetemp) + "°C";
    display.drawString(128,0, rawstring);
    display.drawString(128,16, Rstring); 
    display.drawString(128,32, dallasstring);  
    display.drawString(128,48, probestring);  
    display.display();                    //End OLED code
}
