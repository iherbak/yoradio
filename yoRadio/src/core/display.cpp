#include "options.h"

#include "WiFi.h"
#include "time.h"
#include "display.h"
#include "player.h"
#include "network.h"

Display display;

DspCore dsp;

Page *pages[] = {new Page(), new Page(), new Page()};

#ifndef DSQ_SEND_DELAY
#define DSQ_SEND_DELAY portMAX_DELAY
#endif

#ifndef CORE_STACK_SIZE
#define CORE_STACK_SIZE 1024 * 3
#endif
#ifndef DSP_TASK_DELAY
#define DSP_TASK_DELAY 2
#endif
#undef BITRATE_FULL
#define BITRATE_FULL false
TaskHandle_t DspTask;
QueueHandle_t displayQueue;

void returnPlayer()
{
  display.putRequest(NEWMODE, PLAYER);
}

void Display::_createDspTask()
{
  xTaskCreatePinnedToCore(loopDspTask, "DspTask", CORE_STACK_SIZE, NULL, 4, &DspTask, !xPortGetCoreID());
}

void loopDspTask(void *pvParameters)
{
  while (true)
  {
    if (displayQueue == NULL)
      break;
    display.loop();
    vTaskDelay(DSP_TASK_DELAY);
  }
  vTaskDelete(NULL);
  DspTask = NULL;
}

void Display::init()
{
  Serial.print("##[BOOT]#\tdisplay.init\t");
  _bootStep = 0;
  dsp.initDisplay();
  //displayQueue = NULL;
  displayQueue = xQueueCreate(5, sizeof(requestParams_t));
  // while (displayQueue == NULL)
  // {
  //   ;
  // }
  _createDspTask();
  while (!_bootStep == 0)
  {
    delay(10);
  }
  Serial.println("done");
}

void Display::_bootScreen()
{
  _boot = new Page();
  _boot->addWidget(new ProgressWidget(bootWdtConf, bootPrgConf, BOOT_PRG_COLOR, 0));
  _bootstring = (TextWidget *)&_boot->addWidget(new TextWidget(bootstrConf, 50, true, BOOT_TXT_COLOR, 0));
  _pager.addPage(_boot);
  _pager.setPage(_boot, true);
  dsp.drawLogo(bootLogoTop);
  _bootStep = 1;
}

void Display::_buildPager()
{
  _meta.init("*", metaConf, config.theme.meta, config.theme.metabg);
  _title1.init("*", title1Conf, config.theme.title1, config.theme.background);
  _clock.init(clockConf, 0, 0);
  _plcurrent.init("*", playlistConf, config.theme.plcurrent, config.theme.plcurrentbg);
  //_nums.init(numConf, 10, false, config.theme.digit, config.theme.background);

  if (_volbar)
    _footer.addWidget(_volbar);
  if (_voltxt)
    _footer.addWidget(_voltxt);
  if (_volip)
    _footer.addWidget(_volip);
  if (_rssi)
    _footer.addWidget(_rssi);
  if (_heapbar)
    _footer.addWidget(_heapbar);

  if (_metabackground)
    pages[PG_PLAYER]->addWidget(_metabackground);
  pages[PG_PLAYER]->addWidget(&_meta);
  pages[PG_PLAYER]->addWidget(&_title1);
  if (_title2)
    pages[PG_PLAYER]->addWidget(_title2);
  if (_weather)
    pages[PG_PLAYER]->addWidget(_weather);
#if BITRATE_FULL
  _fullbitrate = new BitrateWidget(fullbitrateConf, config.theme.bitrate, config.theme.background);
  pages[PG_PLAYER]->addWidget(_fullbitrate);
#else
  _bitrate = new TextWidget(bitrateConf, 30, false, config.theme.bitrate, config.theme.background);
  pages[PG_PLAYER]->addWidget(_bitrate);
#endif
  if (_vuwidget)
    pages[PG_PLAYER]->addWidget(_vuwidget);
  pages[PG_PLAYER]->addWidget(&_clock);
  pages[PG_PLAYER]->addPage(&_footer);

  if (_metabackground)
    pages[PG_DIALOG]->addWidget(_metabackground);
  pages[PG_DIALOG]->addWidget(&_meta);
  pages[PG_PLAYLIST]->addWidget(&_plcurrent);

  for (const auto &p : pages)
    _pager.addPage(p);
}

void Display::_apScreen()
{
  if (_boot)
    _pager.removePage(_boot);
  dsp.apScreen();
}

void Display::_start()
{
  if (_boot)
    _pager.removePage(_boot);
  if (network.status != CONNECTED && network.status != SDREADY)
  {
    _apScreen();
    _bootStep = 2;
    return;
  }
  _buildPager();
  _mode = PLAYER;
  config.setTitle(const_PlReady);

  if (_heapbar)
    _heapbar->lock(!config.store.audioinfo);

  if (_weather)
    _weather->lock(!config.store.showweather);
  if (_weather && config.store.showweather)
    _weather->setText(const_getWeather);

  if (_vuwidget)
    _vuwidget->lock();
  if (_rssi)
    _setRSSI(WiFi.RSSI());
  _pager.setPage(pages[PG_PLAYER]);
  _volume();
  _station();
  _time(false);
  _bootStep = 2;
  pm.on_display_player();
}

void Display::_showDialog(const char *title)
{
  dsp.setScrollId(NULL);
  _pager.setPage(pages[PG_DIALOG]);
#ifdef META_MOVE
  _meta.moveTo(metaMove);
#endif
  _meta.setAlign(WA_CENTER);
  _meta.setText(title);
}

void Display::_setReturnTicker(uint8_t time_s)
{
  _returnTicker.detach();
  _returnTicker.once(time_s, returnPlayer);
}

void Display::_swichMode(displayMode_e newmode)
{
  if (newmode == _mode || (network.status != CONNECTED && network.status != SDREADY))
    return;
  _mode = newmode;
  dsp.setScrollId(NULL);
  if (newmode == PLAYER)
  {
#ifdef DSP_LCD
    dsp.clearDsp();
#endif
    numOfNextStation = 0;
    _returnTicker.detach();
#ifdef META_MOVE
    _meta.moveBack();
#endif
    _meta.setAlign(metaConf.widget.align);
    _meta.setText(config.station.name);
    //_nums.setText("");
    _pager.setPage(pages[PG_PLAYER]);
    pm.on_display_player();
  }
  if (newmode == VOL)
  {
#ifndef HIDE_IP
    _showDialog(const_DlgVolume);
#else
    _showDialog(WiFi.localIP().toString().c_str());
#endif
    //_nums.setText(config.store.volume, numtxtFmt);
  }
  if (newmode == LOST)
    _showDialog(const_DlgLost);
  if (newmode == UPDATING)
    _showDialog(const_DlgUpdate);
  if (newmode == SLEEPING)
    _showDialog("SLEEPING");
  if (newmode == SDCHANGE)
    _showDialog(const_waitForSD);
  if (newmode == INFO || newmode == SETTINGS || newmode == TIMEZONE || newmode == WIFI)
    _showDialog(const_DlgNextion);
  if (newmode == NUMBERS)
    _showDialog("");
  if (newmode == STATIONS)
  {
    _pager.setPage(pages[PG_PLAYLIST]);
    _plcurrent.setText("");
    currentPlItem = config.store.lastStation;
    _drawPlaylist();
  }
}

void Display::resetQueue()
{
  if (displayQueue != NULL)
    xQueueReset(displayQueue);
}

void Display::_drawPlaylist()
{
  dsp.drawPlaylist(currentPlItem);
  _setReturnTicker(30);
}

void Display::_drawNextStationNum(uint16_t num)
{
  _setReturnTicker(30);
  _meta.setText(config.stationByNum(num));
  //_nums.setText(num, "%d");
}

void Display::printPLitem(uint8_t pos, const char *item)
{
  dsp.printPLitem(pos, item, _plcurrent);
}

void Display::putRequest(displayRequestType_e type, int payload)
{
  if (displayQueue == NULL)
    return;
  requestParams_t request;
  request.type = type;
  request.payload = payload;
  xQueueSend(displayQueue, &request, DSQ_SEND_DELAY);
}

void Display::_layoutChange(bool played)
{
  if (config.store.vumeter)
  {
    if (played)
    {
      if (_vuwidget)
        _vuwidget->unlock();
      _clock.moveTo(clockMove);
      if (_weather)
        _weather->moveTo(weatherMoveVU);
    }
    else
    {
      if (_vuwidget)
        if (!_vuwidget->locked())
          _vuwidget->lock();
      _clock.moveBack();
      if (_weather)
        _weather->moveBack();
    }
  }
  else
  {
    if (played)
    {
      if (_weather)
        _weather->moveTo(weatherMove);
      _clock.moveBack();
    }
    else
    {
      if (_weather)
        _weather->moveBack();
      _clock.moveBack();
    }
  }
}
#ifndef DSP_QUEUE_TICKS
#define DSP_QUEUE_TICKS 0
#endif
void Display::loop()
{
  if (_bootStep == 0)
  {
    _pager.begin();
    _bootScreen();
    return;
  }
  if (displayQueue == NULL)
    return;
  _pager.loop();
  requestParams_t request;
  if (xQueueReceive(displayQueue, &request, DSP_QUEUE_TICKS))
  {
    bool pm_result = true;
    pm.on_display_queue(request, pm_result);
    if (pm_result)
      switch (request.type)
      {
      case NEWMODE:
        _swichMode((displayMode_e)request.payload);
        break;
      case CLOCK:
        if (_mode == PLAYER)
          _time();
        break;
      case NEWTITLE:
        _title();
        break;
      case NEWSTATION:
        _station();
        break;
      case NEXTSTATION:
        _drawNextStationNum(request.payload);
        break;
      case DRAWPLAYLIST:
        _drawPlaylist();
        break;
      case DRAWVOL:
        _volume();
        break;
      case DBITRATE:
      {
        char buf[20];
        snprintf(buf, 20, bitrateFmt, config.station.bitrate);
        if (_bitrate)
        {
          _bitrate->setText(config.station.bitrate == 0 ? "" : buf);
        }
        if (_fullbitrate)
        {
          _fullbitrate->setBitrate(config.station.bitrate);
          _fullbitrate->setFormat(config.configFmt);
        }
      }
      break;
      case AUDIOINFO:
        if (_heapbar)
        {
          _heapbar->lock(!config.store.audioinfo);
          _heapbar->setValue(player.inBufferFilled());
        }
        break;
      case SHOWVUMETER:
      {
        if (_vuwidget)
        {
          _vuwidget->lock(!config.store.vumeter);
          _layoutChange(player.isRunning());
        }
        break;
      }
      case SHOWWEATHER:
      {
        if (_weather)
          _weather->lock(!config.store.showweather);
        if (!config.store.showweather)
        {
#ifndef HIDE_IP
          if (_volip)
            _volip->setText(WiFi.localIP().toString().c_str(), iptxtFmt);
#endif
        }
        else
        {
          if (_weather)
            _weather->setText(const_getWeather);
        }
        break;
      }
      case NEWWEATHER:
      {
        if (_weather && network.weatherBuf)
          _weather->setText(network.weatherBuf);
        break;
      }
      case BOOTSTRING:
      {
        if (_bootstring)
          _bootstring->setText(config.ssids[request.payload].ssid, bootstrFmt);
        break;
      }
      case WAITFORSD:
      {
        if (_bootstring)
          _bootstring->setText(const_waitForSD);
        break;
      }
      case SDFILEINDEX:
      {
        // if(_mode == SDCHANGE) _nums.setText(request.payload, "%d");
        break;
      }
      case DSPRSSI:
        if (_rssi)
        {
          _setRSSI(request.payload);
        }
        if (_heapbar && config.store.audioinfo)
          _heapbar->setValue(player.isRunning() ? player.inBufferFilled() : 0);
        break;
      case PSTART:
        _layoutChange(true);
        break;
      case PSTOP:
        _layoutChange(false);
        break;
      case DSP_START:
        _start();
        break;
      case NEWIP:
      {
#ifndef HIDE_IP
        if (_volip)
          _volip->setText(WiFi.localIP().toString().c_str(), iptxtFmt);
#endif
        break;
      }
      default:
        break;
      }
  }
  dsp.loop();
}

void Display::_setRSSI(int rssi)
{
  if (!_rssi)
    return;
#if RSSI_DIGIT
  _rssi->setText(rssi, rssiFmt);
  return;
#endif
  char rssiG[3];
  int rssi_steps[] = {RSSI_STEPS};
  if (rssi >= rssi_steps[0])
    strlcpy(rssiG, "\004\006", 3);
  if (rssi >= rssi_steps[1] && rssi < rssi_steps[0])
    strlcpy(rssiG, "\004\005", 3);
  if (rssi >= rssi_steps[2] && rssi < rssi_steps[1])
    strlcpy(rssiG, "\004\002", 3);
  if (rssi >= rssi_steps[3] && rssi < rssi_steps[2])
    strlcpy(rssiG, "\003\002", 3);
  if (rssi < rssi_steps[3] || rssi >= 0)
    strlcpy(rssiG, "\001\002", 3);
  _rssi->setText(rssiG);
}

void Display::_station()
{
  _meta.setAlign(metaConf.widget.align);
  _meta.setText(config.station.name);
}

char *split(char *str, const char *delim)
{
  char *dmp = strstr(str, delim);
  if (dmp == NULL)
    return NULL;
  *dmp = '\0';
  return dmp + strlen(delim);
}

void Display::_title()
{
  if (strlen(config.station.title) > 0)
  {
    char tmpbuf[strlen(config.station.title) + 1];
    strlcpy(tmpbuf, config.station.title, strlen(config.station.title) + 1);
    char *stitle = split(tmpbuf, " - ");
    if (stitle && _title2)
    {
      _title1.setText(tmpbuf);
      _title2->setText(stitle);
    }
    else
    {
      _title1.setText(config.station.title);
      if (_title2)
        _title2->setText("");
    }
  }
  else
  {
    _title1.setText("");
    if (_title2)
      _title2->setText("");
  }
  if (player_on_track_change)
    player_on_track_change();
  pm.on_track_change();
}

void Display::_time(bool redraw)
{
  _clock.draw();
}

void Display::_volume()
{
  if (_volbar)
    _volbar->setValue(config.store.volume);
#ifndef HIDE_VOL
  if (_voltxt)
    _voltxt->setText(config.store.volume, voltxtFmt);
#endif
  if (_mode == VOL)
  {
    _setReturnTicker(3);
    //_nums.setText(config.store.volume, numtxtFmt);
  }
}

void Display::flip() { dsp.flip(); }

void Display::invert() { dsp.invert(); }

void Display::setContrast()
{
#if DSP_MODEL == DSP_NOKIA5110
  dsp.setContrast(config.store.contrast);
#endif
}

bool Display::deepsleep()
{
  dsp.sleep();
  return true;
}

void Display::wakeup()
{
  dsp.wake();
}
