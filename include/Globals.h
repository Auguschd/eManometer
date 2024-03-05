#ifndef _GLOBALS_H
#define _GLOBALS_H

#pragma once

#include <Arduino.h>
#include <Ticker.h>

extern Ticker flasher;

#define FIRMWAREVERSION "1.0.0"

#define PRESSURE_BAR 0
#define PRESSURE_PSI 1

// Number of seconds after reset during which a
// subseqent reset will be considered a double reset.
#define DRD_TIMEOUT 1
// RTC Memory Address for the DoubleResetDetector to use
#define DRD_ADDRESS 0

#define WIFIENADDR 1
#define RTCVALIDFLAG 0xCAFEBABE

// sleep management
#define RTCSLEEPADDR 5
#define MAXSLEEPTIME 3600UL //TODO
#define EMERGENCYSLEEP (myCFG.sleeptime * 3 < MAXSLEEPTIME ? MAXSLEEPTIME : myCFG.sleeptime * 3)
#define LOWBATT 3.3

#ifndef DEBUG
#define DEBUG true
#endif

#define CONSOLE(...)                                                                                                   \
  do                                                                                                                   \
  {                                                                                                                    \
    Serial.print(__VA_ARGS__);                                                                                         \
  } while (0)
#define CONSOLELN(...)                                                                                                 \
  do                                                                                                                   \
  {                                                                                                                    \
    Serial.println(__VA_ARGS__);                                                                                       \
  } while (0)

  #define BAUDRATE 115200
  #define TKIDSIZE 40
  #define CFGFILE "/config.json"

  extern bool saveConfig();
  extern bool saveConfig(int16_t Offset[6]);
  extern bool formatLittleFS();
  extern void flash();

  struct iData
  {
    //General
    char token[TKIDSIZE * 2];
    char name[33] = "";
    char instance[TKIDSIZE] = "000";
    
    //WIFI
    String ssid;
    String psk;
    uint32_t channel;
    uint32_t sleeptime = 15 * 60;
  
    //Pressure
    uint8_t pressurescale = PRESSURE_BAR;
    uint8_t pressurePin = A0;
    uint8_t pressureTransducerMaxPSI = 80;
    float baselineVoltage = 0.48;
  };

  extern iData myCFG;

#endif