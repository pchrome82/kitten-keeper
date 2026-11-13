// Pin map & I2C addresses for the Waveshare ESP32-S3-Touch-AMOLED-1.8
// (SH8601 368x448 QSPI AMOLED, FT3168 touch, QMI8658 IMU, PCF85063 RTC, ES8311 codec, TCA9554 IO expander).
// Source: Waveshare's own demo repo (examples/arduino/libraries/Mylibrary/pin_config.h).
#pragma once

// Two boards are supported; pick one with the PlatformIO environment (see platformio.ini):
//   amoled18  : ESP32-S3-Touch-AMOLED-1.8   (368x448, CST816/FT3168 touch, resets through a TCA9554 expander)
//   amoled216 : ESP32-S3-Touch-AMOLED-2.16  (480x480 CO5300, CST9220 touch, reset lines on GPIOs; -DKK_BOARD_AMOLED216)
// The game always draws a 368x448 picture. On the 2.16" panel it sits 1:1 in the middle (the rest stays black,
// which on an AMOLED is simply off).
#define GAME_W 368
#define GAME_H 448

#if defined(KK_BOARD_AMOLED216)
// Pin map from Waveshare's wiki for the ESP32-S3-Touch-AMOLED-2.16 (docs.waveshare.com/ESP32-S3-Touch-AMOLED-2.16).
#define LCD_SDIO0 4
#define LCD_SDIO1 5
#define LCD_SDIO2 6
#define LCD_SDIO3 7
#define LCD_SCLK 38
#define LCD_CS 12
#define LCD_RESET 39
#define LCD_WIDTH 480
#define LCD_HEIGHT 480
#define LCD_COL_OFFSET 0

#define IIC_SDA 15
#define IIC_SCL 14
#define TP_INT 11
#define TP_RESET 40

#define I2S_MCK_IO 42
#define I2S_BCK_IO 9
#define I2S_WS_IO 45
#define I2S_DI_IO 10
#define I2S_DO_IO 8
#define PA_PIN 46

#define ADDR_CST92XX 0x5A   // touch (CST9220)
// Panel / touch orientation. Not yet checked on real hardware: if the picture or the touch comes out turned or
// mirrored, change these in platformio.ini (build_flags) rather than here.
#ifndef KK_LCD_MADCTL
#define KK_LCD_MADCTL -1    // -1 = leave the panel's default scan direction; Waveshare's demo writes 0xA0
#endif
#ifndef KK_TOUCH_SWAP_XY
#define KK_TOUCH_SWAP_XY 0
#endif
#ifndef KK_TOUCH_MIRROR_X
#define KK_TOUCH_MIRROR_X 0
#endif
#ifndef KK_TOUCH_MIRROR_Y
#define KK_TOUCH_MIRROR_Y 0
#endif
#else

#define LCD_SDIO0 4
#define LCD_SDIO1 5
#define LCD_SDIO2 6
#define LCD_SDIO3 7
#define LCD_SCLK 11
#define LCD_CS 12
#define LCD_WIDTH 368
#define LCD_HEIGHT 448
#define LCD_RESET -1        // (reset lines are on the TCA9554 expander)
#define LCD_COL_OFFSET 16

#define IIC_SDA 15
#define IIC_SCL 14
#define TP_INT 21

#define I2S_MCK_IO 16
#define I2S_BCK_IO 9
#define I2S_WS_IO 45
#define I2S_DI_IO 10
#define I2S_DO_IO 8
#define PA_PIN 46

#endif

#define GAME_X ((LCD_WIDTH - GAME_W) / 2)    // where the game picture sits on the panel
#define GAME_Y ((LCD_HEIGHT - GAME_H) / 2)

#define ADDR_TCA9554 0x20   // IO expander: EXIO0..2 = display / touch / etc. reset lines
#define ADDR_FT3168 0x38    // touch (original board)
#define ADDR_CST816 0x15    // touch (V2 board)
#define ADDR_QMI8658 0x6B   // IMU (0x6A on some revisions)
#define ADDR_PCF85063 0x51  // RTC
#define ADDR_AXP2101 0x34   // power manager (battery %)
