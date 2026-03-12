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
// WIRING (matches config.example.h):
//   GC9A01 SCL  → GPIO4   (SPI clock)
//   GC9A01 SDA  → GPIO6   (SPI MOSI)
//   GC9A01 CS   → GPIO5
//   GC9A01 DC   → GPIO7
//   GC9A01 RES  → GPIO8
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
            cfg.spi_3wire   = true;        // MOSI-only (no MISO) — write-only display
            cfg.use_lock    = true;        // Thread-safe bus access
            cfg.dma_channel = SPI_DMA_CH_AUTO; // Let IDF pick the DMA channel

            cfg.pin_sclk = 4;   // SCL
            cfg.pin_mosi = 6;   // SDA
            cfg.pin_miso = -1;  // Not connected
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
            cfg.readable         = false; // Write-only — no read-back
            cfg.invert           = true;  // GC9A01 modules typically need
                                          // colour inversion for correct output
            cfg.rgb_order   = false;      // BGR panel order
            cfg.dlen_16bit  = false;
            cfg.bus_shared  = false;      // SPI bus is dedicated to this display

            _panel.config(cfg);
        }

        setPanel(&_panel);
    }
};
