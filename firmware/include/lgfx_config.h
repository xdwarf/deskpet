// =============================================================================
// DeskPet — LovyanGFX display configuration
// =============================================================================
// Defines the LGFX class that wires together the GC9A01 panel driver and the
// SPI bus driver for our specific hardware (ESP32-WROOM-32).
//
// SPI BUS SELECTION (display + SD card):
//   Display: VSPI (SPI3) for GC9A01 communication
//   SD Card: SDMMC hardware accelerated controller (uses fixed pins 14, 15, 2)
//   No conflict — both use separate controllers.
//
// WIRING (ESP32-WROOM-32, VSPI pins):
//   GC9A01 SCL  → GPIO18  (VSPI clock)
//   GC9A01 SDA  → GPIO23  (VSPI MOSI)
//   GC9A01 CS   → GPIO5   (Chip Select)
//   GC9A01 DC   → GPIO21  (Data/Command)
//   GC9A01 RES  → GPIO22  (Reset)
//   GC9A01 BL   → GPIO4   (Backlight)
//   SDMMC CLK   → GPIO14  (Fixed hardware pins)
//   SDMMC CMD   → GPIO15
//   SDMMC D0    → GPIO2
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

            // SPI3_HOST (VSPI) for display — separate from SDMMC controller.
            cfg.spi_host = SPI3_HOST;

            cfg.spi_mode    = 0;           // GC9A01 uses SPI mode 0
            cfg.freq_write  = 40000000;    // 40 MHz as requested for stable display DMA transfer
            cfg.freq_read   = 16000000;
            cfg.spi_3wire   = false;       // 4-wire SPI
            cfg.use_lock    = true;        // Thread-safe bus access
            cfg.dma_channel = SPI_DMA_CH_AUTO; // Let IDF pick the DMA channel

            cfg.pin_sclk = 18;  // VSPI SCK
            cfg.pin_mosi = 23;  // VSPI MOSI
            cfg.pin_miso = -1;  // VSPI MISO — not used for write-only display
            cfg.pin_dc   = 21;  // DC / RS

            _bus.config(cfg);
            _panel.setBus(&_bus);
        }

        // -----------------------------------------------------------------
        // Panel configuration
        // -----------------------------------------------------------------
        {
            auto cfg = _panel.config();

            cfg.pin_cs   =  5;   // Chip select
            cfg.pin_rst  = 22;   // Reset
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
            cfg.bus_shared  = false;      // Display and SD card use separate controllers

            _panel.config(cfg);
        }

        setPanel(&_panel);
    }
};
