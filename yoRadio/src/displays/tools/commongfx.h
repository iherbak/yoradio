#ifndef common_gfx_h
#define common_gfx_h
public:
uint16_t plItemHeight, plTtemsCount, plCurrentPos;
int plYStart;

public:
DspCore();
// char plMenu[PLMITEMS][PLMITEMLENGHT];
void initDisplay();
void drawLogo(uint16_t top);
void clearDsp(bool black = false);
void printClock() {}
void printClock(uint16_t top, uint16_t rightspace, uint16_t timeheight, bool redraw);
void clearClock();
char *utf8Rus(const char *str, bool uppercase);
void drawPlaylist(uint16_t currentItem);
void loop(bool force = false);
void charSize(uint8_t textsize, uint8_t &width, uint16_t &height);
uint16_t width();
uint16_t height();
void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {}
void setTextSize(uint8_t s) {}
void setTextSize(uint8_t sx, uint8_t sy) {}
void setTextColor(uint16_t c, uint16_t bg) {}
void setFont() {}
void apScreen();

void flip();
void invert();
void sleep();
void wake();
void writePixel(int16_t x, int16_t y, uint16_t color);
void writeFillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
void setClipping(clipArea ca);
void clearClipping();
void setScrollId(void *scrollid) { _scrollid = scrollid; }
void *getScrollId() { return _scrollid; }
void setNumFont();
uint16_t textWidth(const char *txt);
void printPLitem(uint8_t pos, const char *item, ScrollWidget &current);

private:
char _timeBuf[20], _dateBuf[20], _oldTimeBuf[20], _oldDateBuf[20], _bufforseconds[4], _buffordate[40];
uint16_t _timewidth, _timeleft, _datewidth, _dateleft, _oldtimeleft, _oldtimewidth, _olddateleft, _olddatewidth, clockTop, clockRightSpace, clockTimeHeight, _dotsLeft;
bool _clipping, _printdots;
clipArea _cliparea;
void *_scrollid;
void _getTimeBounds();
void _clockSeconds();
void _clockDate();
void _clockTime();
uint8_t _charWidth(unsigned char c);

#endif
