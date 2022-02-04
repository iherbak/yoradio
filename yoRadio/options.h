#ifndef options_h
#define options_h

#define VERSION "0.4.170"

/*
 * TFT DISPLAY
 */
/**************
 * GND  | GND *
 * VCC  | +5v *
 * SCL  | D18 *
 * SDA  | D23 *
 * ************
 */
#define TFT_CS        5
#define TFT_RST       15   // Or set to -1 and connect to Arduino RESET pin
//#define TFT_RST    -1    // we use the seesaw for resetting to save a pin
#define TFT_DC        4

/*
 * I2S DAC
 */
#define I2S_DOUT      27  // DIN connection
#define I2S_BCLK      26  // BCLK Bit clock
#define I2S_LRC       25  // WSEL Left Right Clock

/*
 * ENCODER
 */
#define ENC_BTNL      13
#define ENC_BTNB      12
#define ENC_BTNR      14

/*
 * BUTTONS
 */
#define BTN_LEFT      32
#define BTN_CENTER    12
#define BTN_RIGHT     33

/*
 * ESP DEVBOARD
 */
#define LED_BUILTIN 2

#endif
