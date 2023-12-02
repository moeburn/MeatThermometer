#include <Average.h>
#include <Arduino.h>
#include "SH1106Wire.h"
#include <Adafruit_ADS1X15.h>
#include <BlynkSimpleEsp32.h>
#include <WiFi.h>
//#include <OneWire.h>
//#include <DallasTemperature.h>
//#include <NonBlockingDallas.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>
#include "SPIFFS.h"
#include <Arduino_JSON.h>
#include <ezBuzzer.h> // ezBuzzer library

#define BUZZER_PIN   23 
#define button_switch 5
#define button_switch2 18
#define ONE_WIRE_BUS 4  //PIN of the Maxim DS18B20 temperature sensor
#define TIME_INTERVAL 1500
#define TEMP1_ADC 3
#define TEMP2_ADC 0

ezBuzzer buzzer(BUZZER_PIN); 

int melody[] = {
  NOTE_E5, NOTE_E5, NOTE_E5,
  NOTE_E5, NOTE_E5, NOTE_E5,
  NOTE_E5, NOTE_G5, NOTE_C5, NOTE_D5,
  NOTE_E5,
  NOTE_F5, NOTE_F5, NOTE_F5, NOTE_F5,
  NOTE_F5, NOTE_E5, NOTE_E5, NOTE_E5, NOTE_E5,
  NOTE_E5, NOTE_D5, NOTE_D5, NOTE_E5,
  NOTE_D5, NOTE_G5
};

// note durations: 4 = quarter note, 8 = eighth note, etc, also called tempo:
int noteDurations[] = {
  8, 8, 4,
  8, 8, 4,
  8, 8, 8, 8,
  2,
  8, 8, 8, 8,
  8, 8, 8, 16, 16,
  8, 8, 8, 8,
  4, 4
};

AsyncWebServer server(80);

// Create an Event Source on /events
AsyncEventSource events("/events");

// Json Variable to Hold Sensor Readings
JSONVar readings;

// Timer variables
unsigned long lastTime = 0;
unsigned long timerDelay = 30000;



#define NUMSAMPLES 200
Average<float> sampleAvg(NUMSAMPLES);
Average<float> sampleAvg2(NUMSAMPLES);

Adafruit_ADS1115 ads;   /* Use this for the 16-bit version */

char auth[] = "DU_j5IxaBQ3Dp-joTLtsB0DM70UZaEDd";

bool displayon = true;
const char* ssid = "mikesnet";
const char* password = "springchicken";

SH1106Wire display(0x3c, SDA, SCL, GEOMETRY_128_64, I2C_ONE, 400000);
float rawReading, rawReading2;
float calibratedReading;
double R2, probetemp, R22, probetemp2;
float V, volts2;

int etamins, etasecs;

double oldtemp, tempdiff, eta, eta2, oldtemp2, tempdiff2;

unsigned long debouncetime;

long double STEINHART_HART_COEF_A = 0.7322291889E-3;
long double STEINHART_HART_COEF_B = 2.132158182E-4;
long double STEINHART_HART_COEF_C = 1.148231681E-7;

//OneWire oneWire(ONE_WIRE_BUS);
//DallasTemperature dallasTemp(&oneWire);
//NonBlockingDallas sensorDs18b20(&dallasTemp);
float dallastemp, ft, fdt, ft2;
int settemp = 145;

String getSensorReadings(){

  readings["sensor1"] = String(ft);
  readings["sensor2"] = String(ft2);
  readings["sensor3"] = String(settemp);
  readings["sensor4"] = String(etamins);

  String jsonString = JSON.stringify(readings);
  return jsonString;
}

void initSPIFFS() {
  if (!SPIFFS.begin()) {
    Serial.println("An error has occurred while mounting SPIFFS");
  }
  else{
    Serial.println("SPIFFS mounted successfully");
  }
}

void handleTemperatureChange(float temperature, bool valid, int deviceIndex) {  //When DS18B20 temperature has changed...
  dallastemp = temperature;
}

#define every(interval) \
  static uint32_t __every__##interval = millis(); \
  if (millis() - __every__##interval >= interval && (__every__##interval = millis()))


bool initialisation_complete = false;
bool is2connected = false;

//
BLYNK_WRITE(V40) {
  int pinValue = param.asInt();  // assigning incoming value from pin V1 to a variable
  if (pinValue == 1) {
    displayon = false;
  } else {
    displayon = true;
  }
}  // end of button_interrupt_handler





void setup() {
  pinMode(BUZZER_PIN,OUTPUT);
  analogReadResolution(12);
  Serial.begin(9600);
  while (!Serial) {}
  ads.setGain(GAIN_ONE);
  ads.begin();
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  Serial.println("");
  display.init();
  //display.flipScreenVertically();
  display.setFont(Monospaced_plain_8);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setBrightness(200);
  display.drawStringMaxWidth(0, 0, 64, "Connecting...");
  display.display();
  while (WiFi.status() != WL_CONNECTED && millis() < 15000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(WiFi.localIP());
    initSPIFFS();
  Serial.println("Hello");
  display.setFont(ArialMT_Plain_16);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  Blynk.config(auth, IPAddress(192, 168, 50, 197), 8080);
  Blynk.connect();
 // sensorDs18b20.begin(NonBlockingDallas::resolution_12, NonBlockingDallas::unit_C, TIME_INTERVAL);  //use non-blocking DS18b20 library

 // sensorDs18b20.onTemperatureChange(handleTemperatureChange);  //only notice when DS18B20 changes
  display.clear();
  display.display();
  pinMode(button_switch, INPUT_PULLUP);
  //attachInterrupt(button_switch, Ext_INT1_ISR, LOW);
  pinMode(button_switch2, INPUT_PULLUP);
  //attachInterrupt(button_switch2, Ext_INT2_ISR, LOW);
  //display.flipScreenVertically();
server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.html", "text/html");
  });

  server.serveStatic("/", SPIFFS, "/");

  // Request for the latest sensor readings
  server.on("/readings", HTTP_GET, [](AsyncWebServerRequest *request){
    String json = getSensorReadings();
    request->send(200, "application/json", json);
    json = String();
  });

  events.onConnect([](AsyncEventSourceClient *client){
    if(client->lastId()){
      Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
    }
    // send event with message "hello!", id current millis
    // and set reconnect delay to 1 second
    client->send("hello!", NULL, millis(), 10000);
  });
  server.addHandler(&events);

  AsyncElegantOTA.begin(&server);
  server.begin();

      int length = sizeof(noteDurations) / sizeof(int);
      buzzer.playMelody(melody, noteDurations, length); // playing
}

void loop() {
  buzzer.loop();
  int buttonfreq = 100;
  if (digitalRead(button_switch) == LOW){
      if (buzzer.getState() != BUZZER_IDLE) {
        buzzer.stop() ; // stop
      }
      every(buttonfreq) {
    settemp--;
  }
  }

  if (digitalRead(button_switch2) == LOW){
      if (buzzer.getState() != BUZZER_IDLE) {
        buzzer.stop() ; // stop
      }
      every(buttonfreq) {
    settemp++;
  }
  }
    Blynk.run();
  //sensorDs18b20.update();
  //for (int i = 0; i <= NUMSAMPLES; i++) { //take NUMSAMPLES samples of analog reader...
  sampleAvg.push(ads.readADC_SingleEnded(TEMP1_ADC));
  if (ads.readADC_SingleEnded(TEMP2_ADC) > 300) {is2connected = true;} else {is2connected = false;}
  if (is2connected) {sampleAvg2.push(ads.readADC_SingleEnded(TEMP2_ADC));
  rawReading2 = sampleAvg2.mean();}
  //delay(1);
  //}
  rawReading = sampleAvg.mean();             //...and average them.
   
                                             //R1 on the voltage divider circuit is a fixed 21840 ohms.
                                             //R₂ = (-V * 21840) / (V - 3.3)
                                             //32767 is the maximum analog reading, which corresponds to 4.096 volts
  V = 3.3 - ((rawReading / 32767) * 4.096);  //Calculate volts of averaged analog reading
  R2 = (-V * 21840) / (V - 3.3);             //Use volts to calculate resistance of thermistor probe
  double log_r = log(R2);                    //Use resistance of thermistor probe to calculate temperature...
  double log_r3 = log_r * log_r * log_r;
  //..using the Steinhart-Hart equation:
  probetemp = 1.0 / (STEINHART_HART_COEF_A + STEINHART_HART_COEF_B * log_r + STEINHART_HART_COEF_C * log_r3) - 273.15;
  if (is2connected) {
    volts2 = 3.3 - ((rawReading2 / 32767) * 4.096);  //Calculate volts of averaged analog reading
    R22 = (-volts2 * 21840) / (volts2 - 3.3);             //Use volts to calculate resistance of thermistor probe
    log_r = log(R22);                    //Use resistance of thermistor probe to calculate temperature...
    log_r3 = log_r * log_r * log_r;
    probetemp2 = 1.0 / (STEINHART_HART_COEF_A + STEINHART_HART_COEF_B * log_r + STEINHART_HART_COEF_C * log_r3) - 273.15;
    if ((ft >= settemp) ||  (ft2 >= settemp)) {
      if (buzzer.getState() == BUZZER_IDLE) {
        int length = sizeof(noteDurations) / sizeof(int);
        buzzer.playMelody(melody, noteDurations, length); // playing
      }
    }
    else {
      if (buzzer.getState() != BUZZER_IDLE) {
        buzzer.stop() ; // stop
      }
    }
  }
  else {
    if (ft >= settemp) {
      if (buzzer.getState() == BUZZER_IDLE) {
        int length = sizeof(noteDurations) / sizeof(int);
        buzzer.playMelody(melody, noteDurations, length); // playing
      }      
    }
    else {
      if (buzzer.getState() != BUZZER_IDLE) {
        buzzer.stop() ; // stop
      }
    }
  }
  every(10000) {

        events.send("ping",NULL,millis());
    events.send(getSensorReadings().c_str(),"new_readings" ,millis());
  }

  every(30000) {
        tempdiff = ft - oldtemp;
    if (is2connected) {
      tempdiff2 = ft2 - oldtemp2;
       eta = (((settemp - ft)/tempdiff) * 5);
        eta2 = (((settemp - ft2)/tempdiff2) * 5);
        if ((eta2 > 0) && (eta2 < 1000) && (eta2 < eta)) {eta = eta2;}
      Blynk.virtualWrite(V4, probetemp2);
      oldtemp2 = ft2;
    }
    else
    {
      eta = (((settemp - ft)/tempdiff) * 5);
    }
    etamins = eta / 60;
    Blynk.virtualWrite(V1, R2);
    Blynk.virtualWrite(V2, probetemp);
    //Blynk.virtualWrite(V3, dallastemp);
    
    oldtemp = ft;
  }
  //Display all this on an OLED
  String settempstring = "Set:";
  String probestring = "T1:";
  String probestring2 = "T2:";
  String etastring = "ETA:";
  
  
    every(250) {
          ft = (probetemp * 1.8) + 32;
          if (is2connected) {ft2 = (probetemp2 * 1.8) + 32;} else {ft2 = 0;}
          fdt = (dallastemp * 1.8) + 32;
          
        if (displayon) {
          display.clear();
          if (is2connected) {
            display.setTextAlignment(TEXT_ALIGN_LEFT);
            display.setFont(ArialMT_Plain_16);
            display.drawString(0, 24, settempstring);
            display.setFont(ArialMT_Plain_24);
            display.drawString(0, 40, etastring);
            display.drawHorizontalLine(0, 24, 128);
            display.drawVerticalLine(64, 0, 24);
            display.setTextAlignment(TEXT_ALIGN_RIGHT);
            settempstring = String(settemp) + "°F";
            probestring = String(ft, 1) + "¹";
            probestring2 = String(ft2, 1) + "²";
            //if (eta < 60){etastring = String(eta, 0) + "s";}
            //else { etasecs = int(eta) % 60; 
            if ((etamins < 1000) && (etamins > 0)) {etastring = String(etamins) + "min";}
            else {etastring = "^^^min";}
               // + String(etasecs) + "s";
            //}
            display.setFont(ArialMT_Plain_16);
            display.drawString(128, 24, settempstring);
            display.setFont(ArialMT_Plain_24);
            display.drawString(128, 40, etastring);
            display.setTextAlignment(TEXT_ALIGN_CENTER);
            display.drawString(32, 0, probestring);
            display.drawString(96, 0, probestring2);
            
          }
          else {
            display.setTextAlignment(TEXT_ALIGN_LEFT);
            display.setFont(ArialMT_Plain_16);
            display.drawString(0, 24, settempstring);
            display.setFont(ArialMT_Plain_24);
            display.drawString(0, 40, etastring);

            display.setTextAlignment(TEXT_ALIGN_RIGHT);
            settempstring = String(settemp) + "°F";
            probestring = String(ft, 1) + "°F";
            //if (eta < 60){etastring = String(eta, 0) + "s";}
            //else { etasecs = int(eta) % 60; 
            if ((etamins < 1000) && (etamins > 0)) {etastring = String(etamins) + "min";}
            else {etastring = "^^^min";}
            //}
            display.setFont(ArialMT_Plain_16);
            display.drawString(128, 24, settempstring);
            display.setFont(ArialMT_Plain_24);
            display.drawString(128, 40, etastring);
            display.setTextAlignment(TEXT_ALIGN_CENTER);
            display.drawString(64, 0, probestring);
          }
        display.display();
        }  //End OLED code  */
        else {
          display.clear();
          display.display();
        }
      }
  Serial.print(probetemp);  //print the serial stuff only once every DS18B20 change so we don't flood the serial monitor with noise
  Serial.print(F(","));
  Serial.print(settemp);
  Serial.print(F(","));
  Serial.print(tempdiff);
  Serial.print(F(","));
  Serial.println(eta);
}
