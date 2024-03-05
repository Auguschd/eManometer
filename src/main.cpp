#include "Globals.h"
//#include "WifiManagerKT.h"
#include <ArduinoJson.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <ESP_DoubleResetDetector.h>
#include <FS.h>
#include <LittleFS.h>

iData myCFG;
DoubleResetDetector drd(DRD_TIMEOUT, DRD_ADDRESS);

float readPressureVoltage()
{
  int pressureValue = analogRead(myCFG.pressurePin);        // Read the sensor value
  return (pressureValue * (5.0 / 1023.0));     // Convert to voltage
}

float getPressureforUnit(float p, uint8_t pressureUnit)
{
  // Assuming 0.5V = 0 PSI and 4.5V = 80 PSI or MaxPSI
  if(pressureUnit == PRESSURE_PSI)
    return ((p - myCFG.baselineVoltage) * (myCFG.pressureTransducerMaxPSI / (4.5 - myCFG.baselineVoltage)));
  else    // default to bar
    return ((p - myCFG.baselineVoltage) * (myCFG.pressureTransducerMaxPSI / (4.5 - myCFG.baselineVoltage)) * 0.0689);
}

String pressureScaleLabel(uint8_t pressureUnit)
{
  if(pressureUnit == PRESSURE_PSI)
    return "PSI";
  else
    return "BAR";
}

bool readConfig()
{
  CONSOLE(F("mounting FS..."));

  if(!LittleFS.begin())
  {
    CONSOLELN(F(" ERROR: failed to mount FS!"));
    return false;
  }
  else
  {
    CONSOLELN(F(" mounted!"));
    if(!LittleFS.exists(CFGFILE))
    {
      CONSOLELN(F(" ERROR: failed to load json config"));
      return false;
    }
    else
    {
      CONSOLELN(F("reading config file"));
      File configFile = LittleFS.open(CFGFILE, "r");
      if(!configFile)
      {
        CONSOLELN(F(" ERROR: unable to open config file"));
      }
      else
      {
        size_t size = configFile.size();
        DynamicJsonDocument doc(size * 3);
        DeserializationError error = deserializeJson(doc, configFile);

        if(error)
        {
          CONSOLE(F("deserializeJson() failed: "));
          CONSOLELN(error.c_str());
        }
        else
        {
          if(doc.containsKey("Name"))
            strcpy(myCFG.name, doc["Name"]);

          CONSOLELN(F("parsed config:"));
#ifdef DEBUG
          serializeJson(doc, Serial);
          CONSOLELN();
#endif                    
        }
      }
    }
  }
  return true;
}

bool shouldBootConfig(bool validConfig)
{
  rst_info *_reset_info = ESP.getResetInfoPtr();
  uint8_t _reset_reason = _reset_info->reason;

  CONSOLE(F("Boot-Mode: "));
  CONSOLELN(ESP.getResetReason());

  bool _poweredOnOffOn = _reset_reason == REASON_DEFAULT_RST || _reset_reason == REASON_EXT_SYS_RST;

  if(_poweredOnOffOn)
  {
    CONSOLELN(F("power-cycle or reset detected, config mode"));

    bool _dblreset = drd.detectDoubleReset();
    if(_dblreset)
    {
      CONSOLELN(F("\nDouble Reset detected"));

      bool _wifiCred = (WiFi.SSID() != "");
      uint8_t c = 0;
      if(!_wifiCred)
      {
        if(strlen(myCFG.name) != 0)
          WiFi.hostname(myCFG.name);

        WiFi.begin();
      }

      while(!_wifiCred)
      {
        if(c > 10)
          break;
        CONSOLE('.');
        delay(100);
        c++;
        _wifiCred = (WiFi.SSID() != "");
      }

      if(!_wifiCred)
        CONSOLELN(F("\nERROR no Wifi credentials"));

      if(validConfig && !_dblreset && _wifiCred && !_poweredOnOffOn)
      {
        CONSOLELN(F("\nwoken from deepsleep, normal mode"));
        return false;
      }
      else
      {
        CONSOLELN(F("\ngoing to Config Mode"));
        return true;
      }
    }
  }
}

bool startConfiguration()
{

  WiFiManager wifiManager;

  wifiManager.setConfigPortalTimeout(PORTALTIMEOUT);
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  wifiManager.setBreakAfterConfig(true);

  //WiFiManagerParameter api_list(HTTP_API_LIST);
  //WiFiManagerParameter custom_api("selAPI", "selAPI", String(myData.api).c_str(), 20, TYPE_HIDDEN, WFM_NO_LABEL);

  WiFiManagerParameter custom_name("name", "eManometer Name", htmlencode(myCFG.name).c_str(), TKIDSIZE);
  WiFiManagerParameter custom_sleep("sleep", "Update Interval (s)", String(myCFG.sleeptime).c_str(), 6, TYPE_NUMBER);
  WiFiManagerParameter custom_channel("channel", "Channelnumber", String(myCFG.channel).c_str(), TKIDSIZE,
                                      TYPE_NUMBER);
  WiFiManagerParameter tempscale_list(HTTP_TEMPSCALE_LIST);
  WiFiManagerParameter custom_tempscale("tempscale", "tempscale", String(myData.tempscale).c_str(), 5, TYPE_HIDDEN,
                                        WFM_NO_LABEL);
  
  wifiManager.addParameter(&custom_name);
  wifiManager.addParameter(&custom_sleep);
  
  WiFiManagerParameter custom_tempscale_hint("<label for=\"TS\">Unit of temperature</label>");
  wifiManager.addParameter(&custom_tempscale_hint);
  wifiManager.addParameter(&tempscale_list);
  wifiManager.addParameter(&custom_tempscale);
  wifiManager.addParameter(&custom_channel);
  
  wifiManager.setConfSSID(htmlencode(myData.ssid));
  wifiManager.setConfPSK(htmlencode(myData.psk));

  CONSOLELN(F("started Portal"));
  static char ssid[33] = {0}; //32 char max for SSIDs
  if (strlen(myData.name) == 0)
    snprintf(ssid, sizeof ssid, "iSpindel_%06X", ESP.getChipId());
  else
  {
    snprintf(ssid, sizeof ssid, "iSpindel_%s", myData.name);
    WiFi.hostname(myData.name); //Set DNS hostname
  }

  wifiManager.startConfigPortal(ssid);

  
  validateInput(custom_name.getValue(), myData.name);
  myData.sleeptime = String(custom_sleep.getValue()).toInt();

  myData.channel = String(custom_channel.getValue()).toInt();
  myData.tempscale = String(custom_tempscale.getValue()).toInt();

  // save the custom parameters to FS
  if (shouldSaveConfig)
  {
    // Wifi config
    WiFi.setAutoConnect(true);
    WiFi.setAutoReconnect(true);

    postConfig();

    return saveConfig();
  }
  return false;
}

void setup() {
  Serial.begin(BAUDRATE);

  CONSOLELN(F("\nFW " FIRMWAREVERSION));
  CONSOLELN(ESP.getSdkVersion());

  //sleepManager();

  
  bool validConf = readConfig();
  if(!validConf)
    CONSOLELN(F("\nERROR: config corrupted"));

  if(shouldBootConfig(validConf))
  {
    uint32_t tmp;
    ESP.rtcUserMemoryRead(WIFIENADDR, &tmp, sizeof(tmp));

    if(tmp != RTCVALIDFLAG)
    {
      //drd.setRecentlyResetFlag();
      tmp = RTCVALIDFLAG;
      ESP.rtcUserMemoryWrite(WIFIENADDR, &tmp, sizeof(tmp));
      CONSOLELN(F("reboot RFCAL"));
      ESP.deepSleep(100000, WAKE_RFCAL);
      delay(500);
    }
    else
    {
      tmp = 0;
      ESP.rtcUserMemoryWrite(WIFIENADDR, &tmp, sizeof(tmp));
    }

    flasher.attach(1, flash);

    if(!startConfiguration())
    {
      if(WiFi.SSID() == "" && myCFG.ssid != "" && myCFG.psk != "")
      {
        //connectBackupCredentials(); 
      }

      uint32_t left2sleep = 0;
      ESP.rtcUserMemoryWrite(RTCSLEEPADDR, &left2sleep, sizeof(left2sleep));

      flasher.detach();
    }    

  }
  
}

void loop() {
  
}
