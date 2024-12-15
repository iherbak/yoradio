#ifndef displayLC1602_h
#define displayLC1602_h
#include "../core/options.h"

#include "Arduino.h"
#include "tools/l10n.h"

#define DSP_NOT_FLIPPED
#define DSP_LCD

#define CHARWIDTH 1
#define CHARHEIGHT 1

#include "widgets/widgets.h"
#include "widgets/pages.h"

#include "../LiquidCrystalI2C/LiquidCrystalI2CEx.h"

#define DSP_INIT LiquidCrystal_I2C(SCREEN_ADDRESS, 40, 2, I2C_SDA, I2C_SCL)

#if __has_include("conf/displayLCD4002conf_custom.h")
#include "conf/displayLCD4002conf_custom.h"
#else
#include "conf/displayLCD4002conf.h"
#endif

#define BOOT_PRG_COLOR 0x1
#define BOOT_TXT_COLOR 0x1
#define PINK 0x1

/* not used required */
#define bootLogoTop 0
const char rssiFmt[] PROGMEM = "";
const MoveConfig clockMove PROGMEM = {0, 0, -1};
const MoveConfig weatherMove PROGMEM = {0, 0, -1};
const MoveConfig weatherMoveVU PROGMEM = {0, 0, -1};
const char const_lcdApMode[] PROGMEM = "YORADIO AP MODE";
const char const_lcdApName[] PROGMEM = "AP NAME: ";
const char const_lcdApPass[] PROGMEM = "PASSWORD: ";

class DspCore : public LiquidCrystal_I2C
{
#include "tools/commongfx.h"
};

extern DspCore dsp;

#endif
