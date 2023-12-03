#include <Average.h>
#include <Arduino.h>
#include "SH1106Wire.h"
#include <Adafruit_ADS1X15.h>
#include <BlynkSimpleEsp32.h>
#include <WiFi.h>
//#include <OneWire.h>  //All Dallas code has been temporarily removed, to be replaced by Dallas EEPROM calibrator
//#include <DallasTemperature.h>
//#include <NonBlockingDallas.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>
#include "SPIFFS.h"
#include <Arduino_JSON.h>

#include "XT_DAC_Audio.h"
#include "JohnnyCash.h"  

XT_Wav_Class Sound(RingOfFire);     
                                      
XT_DAC_Audio_Class DacAudio(25,0);   //Set up the DAC on pin 25

#define BUZZER_PIN   25   //Easy defines in case things get swapped while mounting
#define button_switch 18
#define button_switch2 5
#define ONE_WIRE_BUS 4  //PIN of the Maxim DS18B20 temperature sensor
#define TIME_INTERVAL 1500
#define TEMP1_ADC 3
#define TEMP2_ADC 0

AsyncWebServer server(80);

// Create an Event Source on /events
AsyncEventSource events("/events");

// Json Variable to Hold Sensor Readings
JSONVar readings;

#define NUMSAMPLES 200  //Start a rolling average of 200 samples
Average<float> sampleAvg(NUMSAMPLES);
Average<float> sampleAvg2(NUMSAMPLES);

Adafruit_ADS1115 ads;   /* Use this for the 16-bit version */

char auth[] = "DU_j5IxaBQ3Dp-joTLtsB0DM70UZaEDd";

bool displayon = true;
const char* ssid = "mikesnet";
const char* password = "springchicken";

SH1106Wire display(0x3c, SDA, SCL, GEOMETRY_128_64, I2C_ONE, 400000);  //Set the OLED to 400khz, we don't need max speed and it causes noise on speaker
float rawReading, rawReading2;
float calibratedReading;
double R2, probetemp, R22, probetemp2;
float V, volts2;

int etamins, etasecs;

double oldtemp, tempdiff, eta, eta2, oldtemp2, tempdiff2;

long double STEINHART_HART_COEF_A = 0.7322291889E-3;  //Steinhart-Hart coefficients
long double STEINHART_HART_COEF_B = 2.132158182E-4;
long double STEINHART_HART_COEF_C = 1.148231681E-7;

//OneWire oneWire(ONE_WIRE_BUS);
//DallasTemperature dallasTemp(&oneWire);
//NonBlockingDallas sensorDs18b20(&dallasTemp);
float dallastemp, ft, fdt, ft2;
int settemp = 145;

String getSensorReadings(){  //JSON constructor

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


//Macro for 'every' 
#define every(interval) \        
  static uint32_t __every__##interval = millis(); \
  if (millis() - __every__##interval >= interval && (__every__##interval = millis()))



bool is2connected = false;
bool isblinking = false;
bool isinverted = true;

BLYNK_WRITE(V40) { //Screen toggle button on Blynk
  int pinValue = param.asInt();  // assigning incoming value from pin V1 to a variable
  if (pinValue == 1) {
    displayon = false;
  } else {
    displayon = true;
  }
}  





void setup() {
  Serial.begin(9600);
  analogReadResolution(12);
  ads.setGain(GAIN_ONE);
  ads.begin();  //Init ADS1115
  WiFi.mode(WIFI_STA); 
  WiFi.begin(ssid, password);
  Serial.println("");
  display.init();
  display.flipScreenVertically();  //Flip the screen
  display.setFont(Monospaced_plain_8);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setBrightness(200);
  display.drawStringMaxWidth(0, 0, 64, "Connecting...");  //Put this on the OLED because connecting to wifi takes a few seconds
  display.display();
  while (WiFi.status() != WL_CONNECTED && millis() < 15000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(WiFi.localIP());
  initSPIFFS();  //Init the SPIFFS for serving the HTML files
  Serial.println("Hello");
  display.setFont(ArialMT_Plain_16);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  Blynk.config(auth, IPAddress(192, 168, 50, 197), 8080);
  Blynk.connect();  //Init Blynk

 // sensorDs18b20.begin(NonBlockingDallas::resolution_12, NonBlockingDallas::unit_C, TIME_INTERVAL);  //use non-blocking DS18b20 library
 // sensorDs18b20.onTemperatureChange(handleTemperatureChange);  //only notice when DS18B20 changes

  display.clear(); //Blank the display
  display.display();

  pinMode(button_switch, INPUT_PULLUP); //Turn on pullup resistors for both buttons
  pinMode(button_switch2, INPUT_PULLUP);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){  //If someone connects to the root of our HTTP site, serve index.html
    request->send(SPIFFS, "/index.html", "text/html");
  });

  server.serveStatic("/", SPIFFS, "/");

  // Request for the latest sensor readings
  server.on("/readings", HTTP_GET, [](AsyncWebServerRequest *request){  //Serve the raw JSON data on /readings
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
  AsyncElegantOTA.begin(&server);  //Start the OTA firmware updater on /update
  server.begin();
}





void loop() {
  DacAudio.FillBuffer();   //Keep audio buffer full so we can play anytime with no delay

  /*every(200) {                        //Commented out old display blink code - inverted OLED causes significant current drain, makes temp readings drop by 3 degrees
    if (isblinking == true) {  
      if (isinverted) {  //Blink
        display.invertDisplay();
      }
      else {
        display.normalDisplay();
      }
      isinverted = !isinverted;
    }
    else {
      display.normalDisplay();
    }
  }*/

  int buttonfreq = 100;
  if (digitalRead(button_switch) == LOW){  //Increase or decrease the set temp depending on each button press, once every 100ms
    every(buttonfreq) {
      settemp--;
    }
  }
  if (digitalRead(button_switch2) == LOW){
    every(buttonfreq) {
      settemp++;
    }
  }

  Blynk.run();
  //sensorDs18b20.update();

  sampleAvg.push(ads.readADC_SingleEnded(TEMP1_ADC));  //Add one sample of the temperature probes to the rolling average
  if (ads.readADC_SingleEnded(TEMP2_ADC) > 300) {is2connected = true;} else {is2connected = false;}  //If probe2 reading is unreasonably low, assume it is disconnected and we are reading noise
  if (is2connected) {sampleAvg2.push(ads.readADC_SingleEnded(TEMP2_ADC));
  rawReading2 = sampleAvg2.mean();}  //Use the rolling average for calculating other variables
  rawReading = sampleAvg.mean();            
   
                                             //R1 on the voltage divider circuit is a fixed 21840 ohms, on both probe inputs, hand-balanced.
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

    if ((ft >= settemp) ||  (ft2 >= settemp)) {  //If 2nd probe is connected and either temp goes above set temp, sound the alarm
      if(Sound.Playing==false)       
      DacAudio.Play(&Sound);
      isblinking = true;
    }
    else {
      isblinking = false;
    }
  }
    else {    //Else if only one probe is connected and it goes above the set temp, sound the alarm
      if (ft >= settemp) {
        if(Sound.Playing==false)       
        DacAudio.Play(&Sound);
        isblinking = true;
      }
      else {
        isblinking = false;
      }
    }

  every(10000) {       //Update web interface once every 10 seconds
    events.send("ping",NULL,millis());  
    events.send(getSensorReadings().c_str(),"new_readings" ,millis());
  }

  every(15000) {  //Update the ETA and Blynk interface once every 15 seconds
    tempdiff = ft - oldtemp;
    if (is2connected) {  //If 2nd probe is connected, calculate whichever ETA is sooner in seconds
      tempdiff2 = ft2 - oldtemp2;
      eta = (((settemp - ft)/tempdiff) * 15);
      eta2 = (((settemp - ft2)/tempdiff2) * 15);
      if ((eta2 > 0) && (eta2 < 1000) && (eta2 < eta)) {eta = eta2;}
      Blynk.virtualWrite(V4, probetemp2);
      oldtemp2 = ft2;
    }
    else  //Else if only one probe is connected, calculate the ETA in seconds
    {
      eta = (((settemp - ft)/tempdiff) * 15);
    }
    etamins = eta / 60;  //cast it to int and divide it by 60 to get minutes with no remainder, ignore seconds because of inaccuracy
    //if (etamins > 4) {etamins += 2;}  //Add 2 extra minutes when reading is over 4 minutes due to typical slowing of heating at the end
    Blynk.virtualWrite(V2, probetemp);
    //Blynk.virtualWrite(V3, dallastemp);
    oldtemp = ft;
  }


every(250) {  //Update OLED display, do math once every 250ms
    ft = (probetemp * 1.8) + 32;  //calculate farenheit temp from C
    if (is2connected) {ft2 = (probetemp2 * 1.8) + 32;} else {ft2 = 0;}  //set probe2 to 0 if disconnected
      //fdt = (dallastemp * 1.8) + 32;
    if (displayon) { //Blynk interface has display toggle
      String settempstring = "Set:";  //Set strings to draw left-justified labels
      String probestring = "T1:";
      String probestring2 = "T2:";
      String etastring = "ETA:";
      display.clear();
      //display.drawRect(0,0,128,64);  //Draw display border, for debugging/mounting purposes
      if (is2connected) {  //If there's two probes connected
        display.setTextAlignment(TEXT_ALIGN_LEFT);  //Set the left-justified strings as the data labels
        display.setFont(ArialMT_Plain_16);
        display.drawString(0, 24, settempstring);
        display.setFont(ArialMT_Plain_24);
        display.drawString(0, 40, etastring);
        display.drawHorizontalLine(0, 24, 128);
        display.drawVerticalLine(64, 0, 24);
        display.setTextAlignment(TEXT_ALIGN_RIGHT); //Right justify
        settempstring = String(settemp) + "°F";  //Set the right-justified strings  as the data plus the unit
        probestring = String(ft, 1) + "¹";  //Use superscript 1 and 2 instead of degree signs when 2 probes are connected
        probestring2 = String(ft2, 1) + "²";
        //if (eta < 60){etastring = String(eta, 0) + "s";}  //Removed seconds display due to inaccuracy
        //else { etasecs = int(eta) % 60; 
        if ((etamins < 1000) && (etamins >= 0)) {etastring = String(etamins) + "min";}  //Only display ETA if it is from 0 to 999
        else {etastring = "^^^min";}  //Else display "^^^"
            // + String(etasecs) + "s";
        //}
        display.setFont(ArialMT_Plain_16);  //Draw the right justified strings
        display.drawString(128, 24, settempstring);
        display.setFont(ArialMT_Plain_24);
        display.drawString(128, 40, etastring);
        display.setTextAlignment(TEXT_ALIGN_CENTER);
        display.drawString(32, 0, probestring);
        display.drawString(96, 0, probestring2);  
      }
      else {  //Else if there's only one probe connected
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
        if ((etamins < 1000) && (etamins >= 0)) {etastring = String(etamins) + "min";}
        else {etastring = "^^^min";}
        //}
        display.setFont(ArialMT_Plain_16);
        display.drawString(128, 24, settempstring);
        display.setFont(ArialMT_Plain_24);
        display.drawString(128, 40, etastring);
        display.setTextAlignment(TEXT_ALIGN_CENTER);
        display.drawString(64, 0, probestring);
      }
    display.display();  //Nothing gets actually drawn on the OLED until this line is run
    }  
    else {  //Else if the display is turned off, draw a blank
      display.clear();
      display.display();
    }
  }
}
