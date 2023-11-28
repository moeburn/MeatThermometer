#include <Average.h>
#include <Arduino.h>
#include "SH1106Wire.h"
#include <Adafruit_ADS1X15.h>
#include <BlynkSimpleEsp32.h>
#include <WiFi.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <NonBlockingDallas.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "SPIFFS.h"
#include <Arduino_JSON.h>

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

Adafruit_ADS1115 ads;   /* Use this for the 16-bit version */
#define ONE_WIRE_BUS 4  //PIN of the Maxim DS18B20 temperature sensor
#define TIME_INTERVAL 1500
char auth[] = "DU_j5IxaBQ3Dp-joTLtsB0DM70UZaEDd";

bool displayon = true;
const char* ssid = "mikesnet";
const char* password = "springchicken";

SH1106Wire display(0x3c, SDA, SCL);
float rawReading;
float calibratedReading;
double R2, probetemp;
float V;

int settemp, etamins, etasecs;

double oldtemp, tempdiff, eta;

unsigned long debouncetime;

long double STEINHART_HART_COEF_A = 0.7322291889E-3;
long double STEINHART_HART_COEF_B = 2.132158182E-4;
long double STEINHART_HART_COEF_C = 1.148231681E-7;

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature dallasTemp(&oneWire);
NonBlockingDallas sensorDs18b20(&dallasTemp);
float dallastemp, ft, fdt;
int btns = 110;

String getSensorReadings(){

  readings["sensor1"] = String(ft);
  readings["sensor2"] = String(fdt);
  readings["sensor3"] = String(settemp);
  //readings["sensor4"] = String(165);

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

#define button_switch 5
#define button_switch2 18
bool initialisation_complete = false;

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
  Serial.println(WiFi.localIP());
    initSPIFFS();
  Serial.println("Hello");
  display.setFont(ArialMT_Plain_16);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  Blynk.config(auth, IPAddress(192, 168, 50, 197), 8080);
  Blynk.connect();
  sensorDs18b20.begin(NonBlockingDallas::resolution_12, NonBlockingDallas::unit_C, TIME_INTERVAL);  //use non-blocking DS18b20 library

  sensorDs18b20.onTemperatureChange(handleTemperatureChange);  //only notice when DS18B20 changes
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

  // Start server
  server.begin();
}

void loop() {
  int buttonfreq = 100;
  if (digitalRead(button_switch) == LOW){
      every(buttonfreq) {
    btns--;
  }
  }

  if (digitalRead(button_switch2) == LOW){
      every(buttonfreq) {
    btns++;
  }
  }
    Blynk.run();
  sensorDs18b20.update();
  //for (int i = 0; i <= NUMSAMPLES; i++) { //take NUMSAMPLES samples of analog reader...
  sampleAvg.push(ads.readADC_SingleEnded(0));
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
  every(5000) {
    settemp = btns;
    tempdiff = ft - oldtemp;
    eta = ((settemp - ft)/tempdiff) * 5;
    Blynk.virtualWrite(V1, R2);
    Blynk.virtualWrite(V2, probetemp);
    Blynk.virtualWrite(V3, dallastemp);
    oldtemp = ft;
        events.send("ping",NULL,millis());
    events.send(getSensorReadings().c_str(),"new_readings" ,millis());
  }
  //Display all this on an OLED
  String Rstring = "Set:";
  String probestring = "T:";
  String dallasstring = "ETA:";
  if (displayon) {
    every(250) {
      display.clear();
      display.setFont(ArialMT_Plain_16);
      display.setTextAlignment(TEXT_ALIGN_LEFT);
      display.drawString(0, 0, Rstring);
      display.setFont(ArialMT_Plain_16);
      display.drawString(0, 16, probestring);
      display.drawString(0, 40, dallasstring);
      display.setFont(ArialMT_Plain_16);
      display.setTextAlignment(TEXT_ALIGN_RIGHT);
      Rstring = String(btns);
      ft = (probetemp * 1.8) + 32;
      fdt = (dallastemp * 1.8) + 32;
      probestring = String(ft, 3) + "°F";
      if (eta < 60){dallasstring = String(eta, 0) + "s";}
      else { etasecs = int(eta) % 60; 
      etamins = eta / 60;
      dallasstring = String(etamins) + "m" + String(etasecs) + "s";
      }
      
      display.drawString(128, 0, Rstring);
      display.setFont(ArialMT_Plain_24);
      display.drawString(128, 16, probestring);
      display.drawString(128, 40, dallasstring);
      display.display();
    }  //End OLED code  */
  } else {
    display.clear();
    display.display();
  }
  Serial.print(probetemp);  //print the serial stuff only once every DS18B20 change so we don't flood the serial monitor with noise
  Serial.print(F(","));
  Serial.print(settemp);
  Serial.print(F(","));
  Serial.print(tempdiff);
  Serial.print(F(","));
  Serial.println(eta);

}
