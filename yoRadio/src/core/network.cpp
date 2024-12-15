#include "network.h"
#include "display.h"
#include "options.h"
#include "config.h"
#include "netserver.h"
#include "player.h"

#ifndef WIFI_ATTEMPTS
#define WIFI_ATTEMPTS 16
#endif

MyNetwork network;

TaskHandle_t syncTaskHandle;
// TaskHandle_t reconnectTaskHandle;

bool getWeather(char *wstr);
void doSync(void *pvParameters);

void ticks()
{
  if (!display.ready())
    return; // waiting for SD is ready
  pm.on_ticker();
  static const uint16_t weatherSyncInterval = 1800;
  // static const uint16_t weatherSyncIntervalFail=10;
  static const uint16_t timeSyncInterval = 3600;
  static uint16_t timeSyncTicks = 0;
  static uint16_t weatherSyncTicks = 0;
  static bool divrssi;
  timeSyncTicks++;
  weatherSyncTicks++;
  divrssi = !divrssi;
  if (network.status == CONNECTED)
  {
    if (network.forceTimeSync || network.forceWeather)
    {
      xTaskCreatePinnedToCore(doSync, "doSync", 1024 * 4, NULL, 0, &syncTaskHandle, 0);
    }
    if (timeSyncTicks >= timeSyncInterval)
    {
      timeSyncTicks = 0;
      network.forceTimeSync = true;
    }
    if (weatherSyncTicks >= weatherSyncInterval)
    {
      weatherSyncTicks = 0;
      network.forceWeather = true;
    }
  }
  if (network.timeinfo.tm_year > 100 || network.status == SDREADY)
  {
    network.timeinfo.tm_sec++;
    mktime(&network.timeinfo);
    display.putRequest(CLOCK);
  }
  if (player.isRunning() && config.getMode() == PM_SDCARD)
    netserver.requestOnChange(SDPOS, 0);
  if (divrssi)
  {
    if (network.status == CONNECTED)
    {
      netserver.setRSSI(WiFi.RSSI());
      netserver.requestOnChange(NRSSI, 0);
      display.putRequest(DSPRSSI, netserver.getRSSI());
    }
  }
}

void MyNetwork::WiFiReconnected(WiFiEvent_t event, WiFiEventInfo_t info)
{
  network.beginReconnect = false;
  player.lockOutput = false;
  delay(100);
  display.putRequest(NEWMODE, PLAYER);
  if (config.getMode() == PM_SDCARD)
  {
    network.status = CONNECTED;
    display.putRequest(NEWIP, 0);
  }
  else
  {
    display.putRequest(NEWMODE, PLAYER);
    if (network.lostPlaying)
      player.sendCommand({PR_PLAY, config.store.lastStation});
  }
#ifdef MQTT_ROOT_TOPIC
  connectToMqtt();
#endif
}

void MyNetwork::WiFiLostConnection(WiFiEvent_t event, WiFiEventInfo_t info)
{
  if (!network.beginReconnect)
  {
    Serial.printf("Lost connection, reconnecting to %s...\n", config.ssids[config.store.lastSSID - 1].ssid);
    if (config.getMode() == PM_SDCARD)
    {
      network.status = SDREADY;
      display.putRequest(NEWIP, 0);
    }
    else
    {
      network.lostPlaying = player.isRunning();
      if (network.lostPlaying)
      {
        player.lockOutput = true;
        player.sendCommand({PR_STOP, 0});
      }
      display.putRequest(NEWMODE, LOST);
    }
  }
  network.beginReconnect = true;
  WiFi.reconnect();
}

bool MyNetwork::wifiBegin(bool silent)
{
  uint8_t ls = (config.store.lastSSID == 0 || config.store.lastSSID > config.ssidsCount) ? 0 : config.store.lastSSID - 1;
  uint8_t startedls = ls;
  uint8_t errcnt = 0;
  WiFi.mode(WIFI_STA);
  while (true)
  {
    if (!silent)
    {
      Serial.printf("##[BOOT]#\tAttempt to connect to %s\n", config.ssids[ls].ssid);
      Serial.print("##[BOOT]#\t");
      display.putRequest(BOOTSTRING, ls);
    }
    WiFi.begin(config.ssids[ls].ssid, config.ssids[ls].password);
    while (WiFi.status() != WL_CONNECTED)
    {
      if (!silent)
        Serial.print(".");
      delay(500);
      if (REAL_LEDBUILTIN != 255 && !silent)
        digitalWrite(REAL_LEDBUILTIN, !digitalRead(REAL_LEDBUILTIN));
      errcnt++;
      if (errcnt > WIFI_ATTEMPTS)
      {
        errcnt = 0;
        ls++;
        if (ls > config.ssidsCount - 1)
          ls = 0;
        if (!silent)
          Serial.println();
        break;
      }
    }
    if (WiFi.status() != WL_CONNECTED && ls == startedls)
    {
      return false;
      break;
    }
    if (WiFi.status() == WL_CONNECTED)
    {
      config.setLastSSID(ls + 1);
      return true;
      break;
    }
  }
  return false;
}

void searchWiFi(void *pvParameters)
{
  if (!network.wifiBegin(true))
  {
    delay(10000);
    xTaskCreatePinnedToCore(searchWiFi, "searchWiFi", 1024 * 4, NULL, 0, NULL, 0);
  }
  else
  {
    network.status = CONNECTED;
    netserver.begin(true);
    network.setWifiParams();
    display.putRequest(NEWIP, 0);
  }
  vTaskDelete(NULL);
}

#define DBGAP false

void MyNetwork::begin()
{
  BOOTLOG("network.begin");
  config.initNetwork();
  ctimer.detach();
  forceTimeSync = forceWeather = true;
  if (config.ssidsCount == 0 || DBGAP)
  {
    raiseSoftAP();
    return;
  }
  if (config.getMode() != PM_SDCARD)
  {
    if (!wifiBegin())
    {
      raiseSoftAP();
      Serial.println("##[BOOT]#\tdone");
      return;
    }
    Serial.println(".");
    status = CONNECTED;
    setWifiParams();
  }
  else
  {
    status = SDREADY;
    xTaskCreatePinnedToCore(searchWiFi, "searchWiFi", 1024 * 4, NULL, 0, NULL, 0);
  }

  Serial.println("##[BOOT]#\tdone");
  if (REAL_LEDBUILTIN != 255)
    digitalWrite(REAL_LEDBUILTIN, LOW);

#if RTCSUPPORTED
  rtc.getTime(&network.timeinfo);
  mktime(&network.timeinfo);
  display.putRequest(CLOCK);
#endif
  ctimer.attach(1, ticks);
  if (network_on_connect)
    network_on_connect();
  pm.on_connect();
}

void MyNetwork::setWifiParams()
{
  WiFi.setSleep(false);
  WiFi.onEvent(WiFiReconnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_GOT_IP);
  WiFi.onEvent(WiFiLostConnection, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  weatherBuf = NULL;
  trueWeather = false;
  if (strlen(config.store.sntp1) > 0 && strlen(config.store.sntp2) > 0)
  {
    configTime(config.store.tzHour * 3600 + config.store.tzMin * 60, config.getTimezoneOffset(), config.store.sntp1, config.store.sntp2);
  }
  else if (strlen(config.store.sntp1) > 0)
  {
    configTime(config.store.tzHour * 3600 + config.store.tzMin * 60, config.getTimezoneOffset(), config.store.sntp1);
  }
}

void MyNetwork::requestTimeSync(bool withTelnetOutput, uint8_t clientId)
{
  if (withTelnetOutput)
  {
    char timeStringBuff[50];
    strftime(timeStringBuff, sizeof(timeStringBuff), "%Y-%m-%dT%H:%M:%S", &timeinfo);
  }
}

void rebootTime()
{
  ESP.restart();
}

void MyNetwork::raiseSoftAP()
{
  WiFi.mode(WIFI_AP);
  WiFi.softAP(apSsid, apPassword);
  Serial.println("##[BOOT]#");
  BOOTLOG("************************************************");
  BOOTLOG("Running in AP mode");
  BOOTLOG("Connect to AP %s with password %s", apSsid, apPassword);
  BOOTLOG("and go to http:/192.168.4.1/ to configure");
  BOOTLOG("************************************************");
  status = SOFT_AP;
  if (config.store.softapdelay > 0)
    rtimer.once(config.store.softapdelay * 60, rebootTime);
}

void MyNetwork::requestWeatherSync()
{
  display.putRequest(NEWWEATHER);
}

void doSync(void *pvParameters)
{
  static uint8_t tsFailCnt = 0;
  // static uint8_t wsFailCnt = 0;
  if (network.forceTimeSync)
  {
    network.forceTimeSync = false;
    if (getLocalTime(&network.timeinfo))
    {
      tsFailCnt = 0;
      network.forceTimeSync = false;
      mktime(&network.timeinfo);
      display.putRequest(CLOCK);
      network.requestTimeSync(true);
#if RTCSUPPORTED
      if (config.isRTCFound())
        rtc.setTime(&network.timeinfo);
#endif
    }
    else
    {
      if (tsFailCnt < 4)
      {
        network.forceTimeSync = true;
        tsFailCnt++;
      }
      else
      {
        network.forceTimeSync = false;
        tsFailCnt = 0;
      }
    }
  }
  if (network.weatherBuf && (strlen(config.store.weatherkey) != 0 && config.store.showweather) && network.forceWeather)
  {
    network.forceWeather = false;
    network.trueWeather = getWeather(network.weatherBuf);
  }
  vTaskDelete(NULL);
}

bool getWeather(char *wstr)
{
  return false;
}
