// =============================================================================
// DeskPet — LovyanGFX display configuration
// =============================================================================
// Defines the LGFX class that wires together the GC9A01 panel driver and the
// SPI bus driver for our specific hardware (ESP32-C3 SuperMini).
//
// WHY LOVYANGFX INSTEAD OF TFT_eSPI:
//   TFT_eSPI initialises SPI via Arduino's SPIClass wrapper. On ESP32-C3,
//   SPIClass::begin() triggers a TG1WDT_SYS_RST watchdog crash before any
//   display commands are sent — a known incompatibility. LovyanGFX bypasses
//   SPIClass entirely and calls spi_bus_initialize() from the ESP-IDF
//   spi_master driver directly, which works correctly on ESP32-C3.
//
// SHARED SPI BUS (display + SD card):
//   The SD card shares SPI2 with the display. Both devices use the same
//   SCK (GPIO4) and MOSI (GPIO6) lines. MISO (GPIO3) is needed for SD reads.
//   bus_shared=true tells LovyanGFX to properly release the bus between
//   transactions so the SD library's SPIClass instance can take turns.
//   use_lock=true ensures thread-safe handoff.
//
// WIRING (matches config.example.h):
//   GC9A01 SCL  → GPIO4   (SPI clock  — shared with SD)
//   GC9A01 SDA  → GPIO6   (SPI MOSI   — shared with SD)
//   SD MISO     → GPIO3   (SPI MISO   — display doesn't use this)
//   GC9A01 CS   → GPIO5
//   GC9A01 DC   → GPIO7
//   GC9A01 RES  → GPIO8
//   SD CS       → GPIO10
//   No BL pin   — display has backlight tied internally to 3.3V
// =============================================================================

#pragma once

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

class LGFX : public lgfx::LGFX_Device {

    lgfx::Panel_GC9A01  _panel;
    lgfx::Bus_SPI       _bus;

public:
    LGFX() {

        // -----------------------------------------------------------------
        // SPI bus configuration
        // -----------------------------------------------------------------
        {
            auto cfg = _bus.config();

            // SPI2_HOST is the only user-accessible SPI peripheral on ESP32-C3.
            // SPI0 and SPI1 are reserved for the internal flash.
            cfg.spi_host = SPI2_HOST;

            cfg.spi_mode    = 0;           // GC9A01 uses SPI mode 0
            cfg.freq_write  = 40000000;    // 40 MHz — confirmed working via ESPHome
            cfg.freq_read   = 16000000;
            cfg.spi_3wire   = false;       // 4-wire SPI — MISO needed for SD card
            cfg.use_lock    = true;        // Thread-safe bus access (required for shared bus)
            cfg.dma_channel = SPI_DMA_CH_AUTO; // Let IDF pick the DMA channel

            cfg.pin_sclk = 4;   // SCL — shared with SD
            cfg.pin_mosi = 6;   // SDA — shared with SD
            cfg.pin_miso = 3;   // MISO — SD reads; display ignores this line
            cfg.pin_dc   = 7;   // DC / RS

            _bus.config(cfg);
            _panel.setBus(&_bus);
        }

        // -----------------------------------------------------------------
        // Panel configuration
        // -----------------------------------------------------------------
        {
            auto cfg = _panel.config();

            cfg.pin_cs   =  5;   // Chip select
            cfg.pin_rst  =  8;   // Reset
            cfg.pin_busy = -1;   // Not used on GC9A01

            cfg.panel_width   = 240;
            cfg.panel_height  = 240;
            cfg.memory_width  = 240;
            cfg.memory_height = 240;
            cfg.offset_x        = 0;
            cfg.offset_y        = 0;
            cfg.offset_rotation = 0;

            cfg.dummy_read_pixel = 8;
            cfg.dummy_read_bits  = 1;
            cfg.readable         = false; // No read-back from the display itself
            cfg.invert           = true;  // GC9A01 modules typically need
                                          // colour inversion for correct output
            cfg.rgb_order   = false;      // BGR panel order
            cfg.dlen_16bit  = false;
            cfg.bus_shared  = true;       // SD card also uses this SPI bus

            _panel.config(cfg);
        }

        setPanel(&_panel);
    }
};
