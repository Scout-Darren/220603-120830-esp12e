/*
 * Measurement Firmware for Project Ikepili.
 * Designed to connect with Blynk framework to trigger tank level notifications.
 * The device will sleep during non-active hours.
 * @author: Darren De Guzman
 * @date: 6/2/2022
 * Scout Solutions
*/
// Fill-in information from your Blynk Template here
#define BLYNK_TEMPLATE_ID           "TMPL9i2N_Ai_"
#define BLYNK_DEVICE_NAME           "Test Device"

#define BLYNK_FIRMWARE_VERSION        "0.1.1"

#define BLYNK_PRINT Serial
//#define BLYNK_DEBUG

#define APP_DEBUG

// Uncomment your board, or configure a custom board in Settings.h
//#define USE_SPARKFUN_BLYNK_BOARD
#define USE_NODE_MCU_BOARD
//#define USE_WITTY_CLOUD_BOARD
//#define USE_WEMOS_D1_MINI

#include "BlynkEdgent.h"
#include <RTCmemory.h>
#include <Ultrasonic.h>

// Deep Sleep functionality
// sleep time in seconds
#define DEEP_SLEEP_TIME 1800 //30 minutes
#define MAX_SLEEPNUM 4 //number of 30 minute sleeps

//struct to hold RTC Memory data
typedef struct {
  int count;
  int first;
} MyData;

RTCMemory<MyData> rtcMemory;

#define BLYNK_PRINT Serial
//firmware specific globals
//GPIO 5 => D1
Ultrasonic ultrasonic(5);
float batteryThreshold = 3.5;
bool send = true;
int InitVal = 0;  
double Height = 0.0;  //controlled by V6
double threshold = 0; //controlled by V7
int updateMode = 0; //controlled by V1

void setup()
{
  Serial.begin(74880);
  delay(100);
  
  //check sleep count before connection
  Serial.println("Woke up! Testing Sleep...");
  if(rtcMemory.begin()){
    Serial.println("We found Data!");
  }
  else{
    Serial.println("No data found, resetting...");
  }
  MyData *data = rtcMemory.getData();
//  check if it is first time start up
  if(data->first == 0){
    data->count++;
    Serial.println(data->count);
    if(data->count <MAX_SLEEPNUM){
      rtcMemory.save();
      goToSleep();
    }
    else{
      data->count = 0;
    }
    rtcMemory.save();
  }
  else{
    Serial.println("First Time Start!");
    //if its first time, clear the counts
    data->first = 0;
    data->count = 0;
    rtcMemory.save();
  }
  Serial.println("=== Initiating Blynk Connection ===");
  //Blynk.begin(auth, ssid, pass);
  BlynkEdgent.begin();
  while(!Blynk.connected()){}
}

/*
 * Activates once Blynk is connected
 * The function is blocking and will not let other operations continue like saving credentials to memory
*/
BLYNK_CONNECTED(){
  //maintains widget values after device reset or shutoff
  Serial.println("=== Blynk Connected ===");
  Blynk.syncVirtual(V6,V7,V1,V9);
}

//OTA switch
//decides if sleep should be overridden or not.
//Device should not sleep if being reprovisioned or OTA flashed.
BLYNK_WRITE(V1){
  updateMode = param.asInt();
  if(updateMode == 0){
    Blynk.syncVirtual(V9);
  }
  Serial.println("Im in V1");
  
}

//Tank Height
//sets the tank's total height
BLYNK_WRITE(V6){
   Height = param.asInt();
   Serial.println("Im in V6");
}

//Tank Fill Percentage
//multiplies percentage by tank height to get alert level height
BLYNK_WRITE(V7){
  InitVal = param.asInt();
  threshold = Height - (((InitVal/25.5)/10.0)*Height);
  Blynk.virtualWrite(V8, threshold);
  Serial.println("Im in V7");
}

//Disconnect Button
//Takes reading after all the checks
BLYNK_WRITE(V9){
  if(param.asInt()){
    Blynk.disconnect();
  }
  else{
    Serial.println("Im in V9");
    timedSend();
  }
  
}

/*
 * Initiates deep sleep based on defined sleep time
*/
void goToSleep(){
  Serial.println("Going to Sleep");
  ESP.deepSleep(DEEP_SLEEP_TIME*1000000);
  delay(10);
}

/*
 * Initiates ultrasonic reading.
 * Uses virtual pin values to decide if barrel is low and to send a notification.
*/
void timedSend(){
    Serial.println("Checking Height...");
    delay(20);
    long distcm = ultrasonic.MeasureInCentimeters();
    Serial.println(distcm);
    //send current reading to app
    Blynk.virtualWrite(V5, distcm);
    //has the threshold not been set?
    if(threshold == 0){
      Blynk.logEvent("advnotif", String("Please Enter a tank height and notification percentage"));
    }
    //is the reading above the threshold?
    else if(distcm >= threshold){
        if(send){
            Blynk.logEvent("advnotif", String("Barrel is low level"));
            //prevents from sending multiple of the same notification...
            send = false;
        }
    }
    //is the reading below the threshold?
    else if(distcm < threshold){
        send = true;
    }
    //check if OTA is active
    Blynk.syncVirtual(V1);
    Serial.println(updateMode);
    if(!updateMode){
      Blynk.disconnect();
      goToSleep();
      delay(10);
    }
    else{
      Serial.println("=== OTA mode active ===");
      //reset the sleep numbers
      rtcMemory.begin();
      //clear the saved data since we are reseting the firmware
      MyData *data = rtcMemory.getData();
      data->first = 0;
      data->count = 0;
      rtcMemory.save();
    }
}

void loop() {
  BlynkEdgent.run();
}
