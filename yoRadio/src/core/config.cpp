#include "config.h"
#include <EEPROM.h>
// #include <SPIFFS.h>
#include "display.h"
#include "player.h"
#include "network.h"
#include "netserver.h"
Config config;

void u8fix(char *src)
{
  char last = src[strlen(src) - 1];
  if ((uint8_t)last >= 0xC2)
    src[strlen(src) - 1] = '\0';
}

bool Config::_isFSempty()
{
  const char *reqiredFiles[] = {"dragpl.js.gz", "elogo.png", "elogo84.png", "index.html", "ir.css.gz", "ir.html", "ir.js.gz", "script.js.gz", "settings.css.gz", "settings.html", "style.css.gz", "update.html"};
  const uint8_t reqiredFilesSize = 12;
  char fullpath[28];
  for (uint8_t i = 0; i < reqiredFilesSize; i++)
  {
    sprintf(fullpath, "/www/%s", reqiredFiles[i]);
    if (!SPIFFS.exists(fullpath))
      return true;
  }
  return false;
}

void Config::init()
{
  EEPROM.begin(EEPROM_SIZE);
  bootInfo();
  emptyFS = true;
#if IR_PIN != 255
  irindex = -1;
#endif
  eepromRead(EEPROM_START, store);
  if (store.config_set != 4262)
    setDefaults();
  backupLastStation = store.lastStation;
  if (store.play_mode == 80)
    store.play_mode = 0b100;
  sdSnuffle = bitRead(store.play_mode, 2);
  store.play_mode = store.play_mode & 0b11;
  _initHW();
  if (!SPIFFS.begin(true))
  {
    Serial.println("##[ERROR]#\tSPIFFS Mount Failed");
    return;
  }
  BOOTLOG("SPIFFS mounted");
  emptyFS = _isFSempty();
  if (emptyFS)
    BOOTLOG("SPIFFS is empty!");
  ssidsCount = 0;
  _SDplaylistFS = &SPIFFS;
  randomSeed(esp_random());
  backupSDStation = 0;
  _bootDone = false;
}

bool Config::spiffsCleanup()
{
  bool ret = (SPIFFS.exists(PLAYLIST_SD_PATH)) || (SPIFFS.exists(INDEX_SD_PATH)) || (SPIFFS.exists(INDEX_PATH));
  if (SPIFFS.exists(PLAYLIST_SD_PATH))
    SPIFFS.remove(PLAYLIST_SD_PATH);
  if (SPIFFS.exists(INDEX_SD_PATH))
    SPIFFS.remove(INDEX_SD_PATH);
  if (SPIFFS.exists(INDEX_PATH))
    SPIFFS.remove(INDEX_PATH);
  return ret;
}

void Config::initPlaylistMode()
{
  sdResumePos = 0;

  store.play_mode = PM_WEB;
  if (getMode() == PM_WEB && !emptyFS)
    initPlaylist();

  if (store.lastStation == 0 && store.countStation > 0)
  {
    store.lastStation = getMode() == PM_WEB ? 1 : random(1, store.countStation);
  }
  save();
  _bootDone = true;
  loadStation(store.lastStation);
}

void Config::_initHW()
{
  loadTheme();
#if IR_PIN != 255
  eepromRead(EEPROM_START_IR, ircodes);
  if (ircodes.ir_set != 4224)
  {
    ircodes.ir_set = 4224;
    memset(ircodes.irVals, 0, sizeof(ircodes.irVals));
  }
#endif
#if BRIGHTNESS_PIN != 255
  pinMode(BRIGHTNESS_PIN, OUTPUT);
  setBrightness(false);
#endif
}

uint16_t Config::color565(uint8_t r, uint8_t g, uint8_t b)
{
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

void Config::loadTheme()
{
  theme.background = color565(COLOR_BACKGROUND);
  theme.meta = color565(COLOR_STATION_NAME);
  theme.metabg = color565(COLOR_STATION_BG);
  theme.metafill = color565(COLOR_STATION_FILL);
  theme.title1 = color565(COLOR_SNG_TITLE_1);
  theme.title2 = color565(COLOR_SNG_TITLE_2);
  theme.digit = color565(COLOR_DIGITS);
  theme.div = color565(COLOR_DIVIDER);
  theme.weather = color565(COLOR_WEATHER);
  theme.vumax = color565(COLOR_VU_MAX);
  theme.vumin = color565(COLOR_VU_MIN);
  theme.clock = color565(COLOR_CLOCK);
  theme.clockbg = color565(COLOR_CLOCK_BG);
  theme.seconds = color565(COLOR_SECONDS);
  theme.dow = color565(COLOR_DAY_OF_W);
  theme.date = color565(COLOR_DATE);
  theme.heap = color565(COLOR_HEAP);
  theme.buffer = color565(COLOR_BUFFER);
  theme.ip = color565(COLOR_IP);
  theme.vol = color565(COLOR_VOLUME_VALUE);
  theme.rssi = color565(COLOR_RSSI);
  theme.bitrate = color565(COLOR_BITRATE);
  theme.volbarout = color565(COLOR_VOLBAR_OUT);
  theme.volbarin = color565(COLOR_VOLBAR_IN);
  theme.plcurrent = color565(COLOR_PL_CURRENT);
  theme.plcurrentbg = color565(COLOR_PL_CURRENT_BG);
  theme.plcurrentfill = color565(COLOR_PL_CURRENT_FILL);
  theme.playlist[0] = color565(COLOR_PLAYLIST_0);
  theme.playlist[1] = color565(COLOR_PLAYLIST_1);
  theme.playlist[2] = color565(COLOR_PLAYLIST_2);
  theme.playlist[3] = color565(COLOR_PLAYLIST_3);
  theme.playlist[4] = color565(COLOR_PLAYLIST_4);
}

template <class T>
int Config::eepromWrite(int ee, const T &value)
{
  const uint8_t *p = (const uint8_t *)(const void *)&value;
  int i;
  for (i = 0; i < sizeof(value); i++)
    EEPROM.write(ee++, *p++);
  EEPROM.commit();
  return i;
}

template <class T>
int Config::eepromRead(int ee, T &value)
{
  uint8_t *p = (uint8_t *)(void *)&value;
  int i;
  ;
  for (i = 0; i < sizeof(value); i++)
    *p++ = EEPROM.read(ee++);
  return i;
}

void Config::setDefaults()
{
  store.config_set = 4262;
  store.volume = 12;
  store.balance = 0;
  store.trebble = 0;
  store.middle = 0;
  store.bass = 0;
  store.lastStation = 0;
  store.countStation = 0;
  store.lastSSID = 0;
  store.audioinfo = false;
  store.smartstart = 2;
  store.tzHour = 3;
  store.tzMin = 0;
  store.timezoneOffset = 0;

  store.vumeter = false;
  store.softapdelay = 0;
  store.flipscreen = false;
  store.invertdisplay = false;
  store.numplaylist = false;
  store.fliptouch = false;
  store.dbgtouch = false;
  store.dspon = true;
  store.brightness = 100;
  store.contrast = 55;
  strlcpy(store.sntp1, "pool.ntp.org", 35);
  strlcpy(store.sntp2, "1.ru.pool.ntp.org", 35);
  store.showweather = false;
  strlcpy(store.weatherlat, "55.7512", 10);
  strlcpy(store.weatherlon, "37.6184", 10);
  strlcpy(store.weatherkey, "", 64);
  store.volsteps = 1;
  store.encacc = 200;
  store.play_mode = 0b100;
  store.irtlp = 35;
  store.btnpullup = true;
  store.btnlongpress = 200;
  store.btnclickticks = 300;
  store.btnpressticks = 500;
  store.encpullup = false;
  store.enchalf = false;
  store.enc2pullup = false;
  store.enc2half = false;
  store.forcemono = false;
  store.i2sinternal = false;
  store.rotate90 = false;
}

void Config::setTimezone(int8_t tzh, int8_t tzm)
{
  store.tzHour = tzh;
  store.tzMin = tzm;
  save();
}

void Config::setTimezoneOffset(uint16_t tzo)
{
  store.timezoneOffset = tzo;
  save();
}

uint16_t Config::getTimezoneOffset()
{
  return 0; // TODO
}

void Config::save()
{
  uint16_t ls = store.lastStation;
  uint8_t pm = store.play_mode;
  if (getMode() == PM_SDCARD)
    store.lastStation = backupLastStation;
  if (getMode() == PM_WEB)
    backupLastStation = store.lastStation;
  bitWrite(store.play_mode, 2, sdSnuffle);
  eepromWrite(EEPROM_START, store);
  store.lastStation = ls;
  store.play_mode = pm;
}

void Config::setSnuffle(bool sn)
{
  sdSnuffle = sn;
  save();
  if (sdSnuffle)
    player.next();
}

#if IR_PIN != 255
void Config::saveIR()
{
  eepromWrite(EEPROM_START_IR, ircodes);
}
#endif

void Config::saveVolume()
{
  EEPROM.write(EEPROM_START + sizeof(store.config_set), store.volume);
  EEPROM.commit();
}

uint8_t Config::setVolume(uint8_t val)
{
  store.volume = val;
  display.putRequest(DRAWVOL);
  netserver.requestOnChange(VOLUME, 0);
  return store.volume;
}

void Config::setTone(int8_t bass, int8_t middle, int8_t trebble)
{
  store.bass = bass;
  store.middle = middle;
  store.trebble = trebble;
  save();
}

void Config::setSmartStart(uint8_t ss)
{
  if (store.smartstart < 2)
  {
    store.smartstart = ss;
    save();
  }
}

void Config::setBalance(int8_t balance)
{
  store.balance = balance;
  save();
}

uint8_t Config::setLastStation(uint16_t val)
{
  store.lastStation = val;
  save();
  return store.lastStation;
}

uint8_t Config::setCountStation(uint16_t val)
{
  store.countStation = val;
  save();
  return store.countStation;
}

uint8_t Config::setLastSSID(uint8_t val)
{
  store.lastSSID = val;
  save();
  return store.lastSSID;
}

void Config::setTitle(const char *title)
{
  memset(config.station.title, 0, BUFLEN);
  strlcpy(config.station.title, title, BUFLEN);
  u8fix(config.station.title);
  netserver.requestOnChange(TITLE, 0);
  netserver.loop();
  display.putRequest(NEWTITLE);
}

void Config::setStation(const char *station)
{
  memset(config.station.name, 0, BUFLEN);
  strlcpy(config.station.name, station, BUFLEN);
  u8fix(config.station.title);
}

void Config::indexPlaylist()
{
  File playlist = SPIFFS.open(PLAYLIST_PATH, "r");
  if (!playlist)
  {
    return;
  }
  char sName[BUFLEN], sUrl[BUFLEN];
  int sOvol;
  File index = SPIFFS.open(INDEX_PATH, "w");
  while (playlist.available())
  {
    uint32_t pos = playlist.position();
    if (parseCSV(playlist.readStringUntil('\n').c_str(), sName, sUrl, sOvol))
    {
      index.write((uint8_t *)&pos, 4);
    }
  }
  index.close();
  playlist.close();
}

void Config::initPlaylist()
{
  store.countStation = 0;
  if (!SPIFFS.exists(INDEX_PATH))
    indexPlaylist();

  if (SPIFFS.exists(INDEX_PATH))
  {
    File index = SPIFFS.open(INDEX_PATH, "r");
    store.countStation = index.size() / 4;
    index.close();
    save();
  }
}

void Config::loadStation(uint16_t ls)
{
  char sName[BUFLEN], sUrl[BUFLEN];
  int sOvol;
  if (store.countStation == 0)
  {
    memset(station.url, 0, BUFLEN);
    memset(station.name, 0, BUFLEN);
    strncpy(station.name, "ёRadio", BUFLEN);
    station.ovol = 0;
    return;
  }
  if (ls > store.countStation)
  {
    ls = 1;
  }
  File playlist = SDPLFS()->open(REAL_PLAYL, "r");

  File index = SDPLFS()->open(REAL_INDEX, "r");
  index.seek((ls - 1) * 4, SeekSet);
  uint32_t pos;

  index.readBytes((char *)&pos, 4);

  index.close();
  playlist.seek(pos, SeekSet);
  if (parseCSV(playlist.readStringUntil('\n').c_str(), sName, sUrl, sOvol))
  {
    memset(station.url, 0, BUFLEN);
    memset(station.name, 0, BUFLEN);
    strncpy(station.name, sName, BUFLEN);
    strncpy(station.url, sUrl, BUFLEN);
    station.ovol = sOvol;
    setLastStation(ls);
  }
  playlist.close();
}

char *Config::stationByNum(uint16_t num)
{
  File playlist = SDPLFS()->open(REAL_PLAYL, "r");
  File index = SDPLFS()->open(REAL_INDEX, "r");
  index.seek((num - 1) * 4, SeekSet);
  uint32_t pos;
  memset(_stationBuf, 0, BUFLEN / 2);
  index.readBytes((char *)&pos, 4);
  index.close();
  playlist.seek(pos, SeekSet);
  strncpy(_stationBuf, playlist.readStringUntil('\t').c_str(), BUFLEN / 2);
  playlist.close();
  return _stationBuf;
}

uint8_t Config::fillPlMenu(int from, uint8_t count, bool fromNextion)
{
  int ls = from;
  uint8_t c = 0;
  bool finded = false;
  if (store.countStation == 0)
  {
    return 0;
  }
  File playlist = SDPLFS()->open(REAL_PLAYL, "r");
  File index = SDPLFS()->open(REAL_INDEX, "r");
  while (true)
  {
    if (ls < 1)
    {
      ls++;
      if (!fromNextion)
        display.printPLitem(c, "");
      c++;
      continue;
    }
    if (!finded)
    {
      index.seek((ls - 1) * 4, SeekSet);
      uint32_t pos;
      index.readBytes((char *)&pos, 4);
      finded = true;
      index.close();
      playlist.seek(pos, SeekSet);
    }
    bool pla = true;
    while (pla)
    {
      pla = playlist.available();
      String stationName = playlist.readStringUntil('\n');
      stationName = stationName.substring(0, stationName.indexOf('\t'));
      if (config.store.numplaylist && stationName.length() > 0)
        stationName = String(from + c) + " " + stationName;
      if (!fromNextion)
        display.printPLitem(c, stationName.c_str());
      c++;
      if (c >= count)
        break;
    }
    break;
  }
  playlist.close();
  return c;
}

bool Config::parseCSV(const char *line, char *name, char *url, int &ovol)
{
  char *tmpe;
  const char *cursor = line;
  char buf[5];
  tmpe = strstr(cursor, "\t");
  if (tmpe == NULL)
    return false;
  strlcpy(name, cursor, tmpe - cursor + 1);
  if (strlen(name) == 0)
    return false;
  cursor = tmpe + 1;
  tmpe = strstr(cursor, "\t");
  if (tmpe == NULL)
    return false;
  strlcpy(url, cursor, tmpe - cursor + 1);
  if (strlen(url) == 0)
    return false;
  cursor = tmpe + 1;
  if (strlen(cursor) == 0)
    return false;
  strlcpy(buf, cursor, 4);
  ovol = atoi(buf);
  return true;
}

bool Config::parseJSON(const char *line, char *name, char *url, int &ovol)
{
  char *tmps, *tmpe;
  const char *cursor = line;
  char port[8], host[246], file[254];
  tmps = strstr(cursor, "\":\"");
  if (tmps == NULL)
    return false;
  tmpe = strstr(tmps, "\",\"");
  if (tmpe == NULL)
    return false;
  strlcpy(name, tmps + 3, tmpe - tmps - 3 + 1);
  if (strlen(name) == 0)
    return false;
  cursor = tmpe + 3;
  tmps = strstr(cursor, "\":\"");
  if (tmps == NULL)
    return false;
  tmpe = strstr(tmps, "\",\"");
  if (tmpe == NULL)
    return false;
  strlcpy(host, tmps + 3, tmpe - tmps - 3 + 1);
  if (strlen(host) == 0)
    return false;
  if (strstr(host, "http://") == NULL && strstr(host, "https://") == NULL)
  {
    sprintf(file, "http://%s", host);
    strlcpy(host, file, strlen(file) + 1);
  }
  cursor = tmpe + 3;
  tmps = strstr(cursor, "\":\"");
  if (tmps == NULL)
    return false;
  tmpe = strstr(tmps, "\",\"");
  if (tmpe == NULL)
    return false;
  strlcpy(file, tmps + 3, tmpe - tmps - 3 + 1);
  cursor = tmpe + 3;
  tmps = strstr(cursor, "\":\"");
  if (tmps == NULL)
    return false;
  tmpe = strstr(tmps, "\",\"");
  if (tmpe == NULL)
    return false;
  strlcpy(port, tmps + 3, tmpe - tmps - 3 + 1);
  int p = atoi(port);
  if (p > 0)
  {
    sprintf(url, "%s:%d%s", host, p, file);
  }
  else
  {
    sprintf(url, "%s%s", host, file);
  }
  cursor = tmpe + 3;
  tmps = strstr(cursor, "\":\"");
  if (tmps == NULL)
    return false;
  tmpe = strstr(tmps, "\"}");
  if (tmpe == NULL)
    return false;
  strlcpy(port, tmps + 3, tmpe - tmps - 3 + 1);
  ovol = atoi(port);
  return true;
}

bool Config::parseWsCommand(const char *line, char *cmd, char *val, uint8_t cSize)
{
  char *tmpe;
  tmpe = strstr(line, "=");
  if (tmpe == NULL)
    return false;
  memset(cmd, 0, cSize);
  strlcpy(cmd, line, tmpe - line + 1);
  if (strlen(tmpe + 1) == 0)
    return false;
  memset(val, 0, cSize);
  strlcpy(val, tmpe + 1, strlen(line) - strlen(cmd) + 1);
  return true;
}

bool Config::parseSsid(const char *line, char *ssid, char *pass)
{
  char *tmpe;
  tmpe = strstr(line, "\t");
  if (tmpe == NULL)
    return false;
  uint16_t pos = tmpe - line;
  if (pos > 29 || strlen(line) > 71)
    return false;
  memset(ssid, 0, 30);
  strlcpy(ssid, line, pos + 1);
  memset(pass, 0, 40);
  strlcpy(pass, line + pos + 1, strlen(line) - pos);
  return true;
}

bool Config::saveWifiFromNextion(const char *post)
{
  File file = SPIFFS.open(SSIDS_PATH, "w");
  if (!file)
  {
    return false;
  }
  else
  {
    file.print(post);
    file.close();
    ESP.restart();
    return true;
  }
}

bool Config::saveWifi()
{
  if (!SPIFFS.exists(TMP_PATH))
    return false;
  SPIFFS.remove(SSIDS_PATH);
  SPIFFS.rename(TMP_PATH, SSIDS_PATH);
  ESP.restart();
  return true;
}

bool Config::initNetwork()
{
  File file = SPIFFS.open(SSIDS_PATH, "r");
  if (!file || file.isDirectory())
  {
    return false;
  }
  char ssidval[30], passval[40];
  uint8_t c = 0;
  while (file.available())
  {
    if (parseSsid(file.readStringUntil('\n').c_str(), ssidval, passval))
    {
      strlcpy(ssids[c].ssid, ssidval, 30);
      strlcpy(ssids[c].password, passval, 40);
      ssidsCount++;
      c++;
    }
  }
  file.close();
  return true;
}

void Config::setBrightness(bool dosave)
{
}

void Config::setDspOn(bool dspon)
{
  store.dspon = dspon;
  save();
  if (!dspon)
  {
    display.deepsleep();
  }
  else
  {
    display.wakeup();
  }
}

void Config::doSleep()
{
  if (BRIGHTNESS_PIN != 255)
    analogWrite(BRIGHTNESS_PIN, 0);
  display.deepsleep();
#if !defined(ARDUINO_ESP32C3_DEV)
  if (WAKE_PIN != 255)
    esp_sleep_enable_ext0_wakeup((gpio_num_t)WAKE_PIN, LOW);
  esp_sleep_enable_timer_wakeup(config.sleepfor * 60 * 1000000ULL);
  esp_deep_sleep_start();
#endif
}

void Config::doSleepW()
{
  if (BRIGHTNESS_PIN != 255)
    analogWrite(BRIGHTNESS_PIN, 0);
  display.deepsleep();
#if !defined(ARDUINO_ESP32C3_DEV)
  if (WAKE_PIN != 255)
    esp_sleep_enable_ext0_wakeup((gpio_num_t)WAKE_PIN, LOW);
  esp_deep_sleep_start();
#endif
}

void Config::sleepForAfter(uint16_t sf, uint16_t sa)
{
  sleepfor = sf;
  if (sa > 0)
    _sleepTimer.attach(sa * 60, doSleep);
  else
    doSleep();
}

void Config::bootInfo()
{
  BOOTLOG("************************************************");
  BOOTLOG("*               ёPadio v%s                *", YOVERSION);
  BOOTLOG("************************************************");
  BOOTLOG("------------------------------------------------");
  BOOTLOG("arduino:\t%d", ARDUINO);
  BOOTLOG("compiler:\t%s", __VERSION__);
  BOOTLOG("esp32core:\t%d.%d.%d", ESP_ARDUINO_VERSION_MAJOR, ESP_ARDUINO_VERSION_MINOR, ESP_ARDUINO_VERSION_PATCH);
  uint32_t chipId = 0;
  for (int i = 0; i < 17; i = i + 8)
  {
    chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
  }
  BOOTLOG("chip:\t\tmodel: %s | rev: %d | id: %d | cores: %d | psram: %d", ESP.getChipModel(), ESP.getChipRevision(), chipId, ESP.getChipCores(), ESP.getPsramSize());
  BOOTLOG("display:\t%d", DSP_MODEL);
  if (VS1053_CS == 255)
  {
    BOOTLOG("audio:\t\t%s (%d, %d, %d)", "I2S", I2S_DOUT, I2S_BCLK, I2S_LRC);
  }
  else
  {
    BOOTLOG("audio:\t\t%s (%d, %d, %d, %d, %s)", "VS1053", VS1053_CS, VS1053_DCS, VS1053_DREQ, VS1053_RST, VS_HSPI ? "true" : "false");
  }
  BOOTLOG("audioinfo:\t%s", store.audioinfo ? "true" : "false");
  BOOTLOG("smartstart:\t%d", store.smartstart);
  BOOTLOG("vumeter:\t%s", store.vumeter ? "true" : "false");
  BOOTLOG("softapdelay:\t%d", store.softapdelay);
  BOOTLOG("flipscreen:\t%s", store.flipscreen ? "true" : "false");
  BOOTLOG("invertdisplay:\t%s", store.invertdisplay ? "true" : "false");
  BOOTLOG("showweather:\t%s", store.showweather ? "true" : "false");
  BOOTLOG("buttons:\tleft=%d, center=%d, right=%d, up=%d, down=%d, mode=%d, pullup=%s", BTN_LEFT, BTN_CENTER, BTN_RIGHT, BTN_UP, BTN_DOWN, BTN_MODE, BTN_INTERNALPULLUP ? "true" : "false");
  BOOTLOG("encoders:\tl1=%d, b1=%d, r1=%d, pullup=%s, l2=%d, b2=%d, r2=%d, pullup=%s", ENC_BTNL, ENC_BTNB, ENC_BTNR, ENC_INTERNALPULLUP ? "true" : "false", ENC2_BTNL, ENC2_BTNB, ENC2_BTNR, ENC2_INTERNALPULLUP ? "true" : "false");
  BOOTLOG("ir:\t\t%d", IR_PIN);
  if (SDC_CS != 255)
    BOOTLOG("SD:\t\t%d", SDC_CS);
  BOOTLOG("------------------------------------------------");
}
