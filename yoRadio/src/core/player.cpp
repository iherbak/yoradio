#include "options.h"

#include "player.h"

#include "config.h"
#include "display.h"
#include "netserver.h"

Player player;
QueueHandle_t playerQueue;

Player::Player() {}

void Player::init()
{
  Serial.print("##[BOOT]#\tplayer.init\t");
  playerQueue = NULL;
  _resumeFilePos = 0;
  playerQueue = xQueueCreate(5, sizeof(playerRequestParams_t));
  setOutputPins(false);
  delay(50);
  memset(_plError, 0, PLERR_LN);
  setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  setBalance(config.store.balance);
  setTone(config.store.bass, config.store.middle, config.store.trebble);
  setVolume(0);
  _status = STOPPED;
  _volTimer = false;
  _loadVol(config.store.volume);
  setConnectionTimeout(1700, 3700);
  Serial.println("done");
}

void Player::sendCommand(playerRequestParams_t request)
{
  if (playerQueue == NULL)
    return;
  xQueueSend(playerQueue, &request, PLQ_SEND_DELAY);
}

void Player::resetQueue()
{
  if (playerQueue != NULL)
    xQueueReset(playerQueue);
}

void Player::stopInfo()
{
  config.setSmartStart(0);
  netserver.requestOnChange(MODE, 0);
}

void Player::setError(const char *e)
{
  strlcpy(_plError, e, PLERR_LN);
  if (hasError())
  {
    config.setTitle(_plError);
  }
}

void Player::_stop(bool alreadyStopped)
{
  if (config.getMode() == PM_SDCARD && !alreadyStopped)
    config.sdResumePos = player.getFilePos();
  _status = STOPPED;
  setOutputPins(false);
  if (!hasError())
    config.setTitle((display.mode() == LOST || display.mode() == UPDATING) ? "" : const_PlStopped);
  config.station.bitrate = 0;
  config.setBitrateFormat(BF_UNCNOWN);
  netserver.requestOnChange(BITRATE, 0);
  display.putRequest(DBITRATE);
  display.putRequest(PSTOP);
  setDefaults();
  if (!alreadyStopped)
    stopSong();
  if (!lockOutput)
    stopInfo();
  if (player_on_stop_play)
    player_on_stop_play();
  pm.on_stop_play();
}

void Player::initHeaders(const char *file)
{
  if (strlen(file) == 0 || true)
    return; // TODO Read TAGs
  eofHeader = false;
  while (!eofHeader)
    Audio::loop();
  // netserver.requestOnChange(SDPOS, 0);
  setDefaults();
}

#ifndef PL_QUEUE_TICKS
#define PL_QUEUE_TICKS 0
#endif

void Player::loop()
{
  if (playerQueue == NULL)
    return;
  playerRequestParams_t requestP;
  if (xQueueReceive(playerQueue, &requestP, PL_QUEUE_TICKS))
  {
    switch (requestP.type)
    {
    case PR_STOP:
      _stop();
      break;
    case PR_PLAY:
    {
      if (requestP.payload > 0)
      {
        config.setLastStation((uint16_t)requestP.payload);
      }
      _play((uint16_t)abs(requestP.payload));
      if (player_on_station_change)
        player_on_station_change();
      pm.on_station_change();
      break;
    }
    case PR_VOL:
    {
      config.setVolume(requestP.payload);
      Audio::setVolume(volToI2S(requestP.payload));
      break;
    }
    default:
      break;
    }
  }
  Audio::loop();
  if (!isRunning() && _status == PLAYING)
    _stop(true);
  if (_volTimer)
  {
    if ((millis() - _volTicks) > 3000)
    {
      config.saveVolume();
      _volTimer = false;
    }
  }
#ifdef MQTT_ROOT_TOPIC
  if (strlen(burl) > 0)
  {
    browseUrl();
  }
#endif
}

void Player::setOutputPins(bool isPlaying)
{
  if (REAL_LEDBUILTIN != 255)
    digitalWrite(REAL_LEDBUILTIN, LED_INVERT ? !isPlaying : isPlaying);
  bool _ml = MUTE_LOCK ? !MUTE_VAL : (isPlaying ? !MUTE_VAL : MUTE_VAL);
  if (MUTE_PIN != 255)
    digitalWrite(MUTE_PIN, _ml);
}

void Player::_play(uint16_t stationId)
{
  setError("");
  remoteStationName = false;
  config.setDspOn(1);
  // display.putRequest(PSTOP);
  if (config.getMode() != PM_SDCARD)
  {
    display.putRequest(PSTOP);
  }
  setOutputPins(false);
  // config.setTitle(config.getMode()==PM_WEB?const_PlConnect:"");
  config.setTitle(config.getMode() == PM_WEB ? const_PlConnect : "[next track]");
  config.station.bitrate = 0;
  config.setBitrateFormat(BF_UNCNOWN);
  config.loadStation(stationId);
  _loadVol(config.store.volume);
  display.putRequest(DBITRATE);
  display.putRequest(NEWSTATION);
  netserver.requestOnChange(STATION, 0);
  netserver.loop();
  netserver.loop();
  config.setSmartStart(0);
  bool isConnected = false;
  config.store.play_mode = PM_WEB;
  config.save();
  if (config.getMode() == PM_WEB)
    isConnected = connecttohost(config.station.url);
  if (isConnected)
  {
    // if (config.store.play_mode==PM_WEB?connecttohost(config.station.url):connecttoFS(SD,config.station.url,config.sdResumePos==0?_resumeFilePos:config.sdResumePos-player.sd_min)) {
    _status = PLAYING;
    if (config.getMode() == PM_SDCARD)
    {
      config.sdResumePos = 0;
      config.backupSDStation = stationId;
    }
    // config.setTitle("");
    config.setSmartStart(1);
    netserver.requestOnChange(MODE, 0);
    setOutputPins(true);
    display.putRequest(PSTART);
    if (player_on_start_play)
      player_on_start_play();
    pm.on_start_play();
  }
  else
  {
    SET_PLAY_ERROR("Error connecting to %s", config.station.url);
    _stop(true);
  };
}

void Player::prev()
{
  if (config.getMode() == PM_WEB || !config.sdSnuffle)
  {
    if (config.store.lastStation == 1)
      config.store.lastStation = config.store.countStation;
    else
      config.store.lastStation--;
  }
  sendCommand({PR_PLAY, config.store.lastStation});
}

void Player::next()
{
  if (config.getMode() == PM_WEB || !config.sdSnuffle)
  {
    if (config.store.lastStation == config.store.countStation)
      config.store.lastStation = 1;
    else
      config.store.lastStation++;
  }
  else
  {
    config.store.lastStation = random(1, config.store.countStation);
  }
  sendCommand({PR_PLAY, config.store.lastStation});
}

void Player::toggle()
{
  if (_status == PLAYING)
  {
    sendCommand({PR_STOP, 0});
  }
  else
  {
    sendCommand({PR_PLAY, config.store.lastStation});
  }
}

void Player::stepVol(bool up)
{
  if (up)
  {
    if (config.store.volume <= 254 - config.store.volsteps)
    {
      setVol(config.store.volume + config.store.volsteps);
    }
    else
    {
      setVol(254);
    }
  }
  else
  {
    if (config.store.volume >= config.store.volsteps)
    {
      setVol(config.store.volume - config.store.volsteps);
    }
    else
    {
      setVol(0);
    }
  }
}

uint8_t Player::volToI2S(uint8_t volume)
{
  int vol = map(volume, 0, 254 - config.station.ovol * 3, 0, 254);
  if (vol > 254)
    vol = 254;
  if (vol < 0)
    vol = 0;
  return vol;
}

void Player::_loadVol(uint8_t volume)
{
  setVolume(volToI2S(volume));
}

void Player::setVol(uint8_t volume)
{
  _volTicks = millis();
  _volTimer = true;
  player.sendCommand({PR_VOL, volume});
}
