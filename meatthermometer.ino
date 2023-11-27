#include <Average.h>
#include <Arduino.h>
#include "SH1106Wire.h"
#include <Adafruit_ADS1X15.h>
#include <BlynkSimpleEsp32.h>
#include <WiFi.h>

#define NUMSAMPLES 100
Average<float> sampleAvg(NUMSAMPLES);

Adafruit_ADS1115 ads;  /* Use this for the 16-bit version */
#define ONE_WIRE_BUS 2       //PIN of the Maxim DS18B20 temperature sensor 

char auth[] = "DU_j5IxaBQ3Dp-joTLtsB0DM70UZaEDd";

const char* ssid = "mikesnet";
const char* password = "springchicken";

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

#define every(interval) \
    static uint32_t __every__##interval = millis(); \
    if (millis() - __every__##interval >= interval && (__every__##interval = millis()))


void setup() {
    analogReadResolution(12);
    Serial.begin(9600);
    while (!Serial) {}
    ads.setGain(GAIN_ONE);  
    ads.begin();
      WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("");
  display.init();
  display.setFont(Monospaced_plain_8);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setBrightness(100);
  display.drawStringMaxWidth(0, 0, 64, "Connecting...");
  display.display();
  while (WiFi.status() != WL_CONNECTED && millis() < 15000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Hello");
  display.setFont(ArialMT_Plain_16);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
      Blynk.config(auth, IPAddress(192, 168, 50, 197), 8080);
    Blynk.connect();
  //display.flipScreenVertically();
}

void loop() {
    Blynk.run();
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
    every(5000){
    Blynk.virtualWrite(V1, R2);
    Blynk.virtualWrite(V2, probetemp);
    }
        //Display all this on an OLED
    String Rstring = "R:";
    String probestring = "T:";
    display.clear();
    display.setFont(ArialMT_Plain_16);
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.drawString(0,0, Rstring);
    display.setFont(ArialMT_Plain_24);
    display.drawString(0,36, probestring);
    display.setFont(ArialMT_Plain_16);
    display.setTextAlignment(TEXT_ALIGN_RIGHT);
    Rstring = String(R2) + "Ω";
    probestring = String(probetemp, 3) + "°C";
    display.drawString(128,0, Rstring); 
    display.setFont(ArialMT_Plain_24);
    display.drawString(128,36, probestring);  
    display.display();                    //End OLED code

      Serial.print(rawReading);  //print the serial stuff only once every DS18B20 change so we don't flood the serial monitor with noise
      Serial.print(F(","));
      Serial.print(R2);
      Serial.print(F(","));
      Serial.println(probetemp);
}
