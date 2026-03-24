// =============================================================================
// TFT_eSPI User Setup — DeskPet GC9A01 on ESP32-C3 Mini
// =============================================================================
// How this file is loaded by TFT_eSPI:
//   platformio.ini adds -I include to build_flags, so ALL compilation units
//   (including TFT_eSPI itself) search our include/ directory first.
//   TFT_eSPI.h includes <User_Setup.h> via User_Setup_Select.h — the compiler
//   finds THIS file instead of the one bundled inside the library.
//   USER_SETUP_LOADED is defined at the BOTTOM of this file as a guard so
//   TFT_eSPI skips the include on subsequent passes (do not define it in
//   build_flags, that would cause TFT_eSPI to skip loading the setup entirely).
//
// Reference: https://github.com/Bodmer/TFT_eSPI/blob/master/User_Setup.h
// =============================================================================

// --- Setup info string (used by TFT_eSPI diagnostics) ---
// Must be defined here as a quoted string — defining it via -D in platformio.ini
// strips the quotes when the preprocessor expands it, causing a compile error.
#define USER_SETUP_INFO "DeskPet GC9A01 Config"

// --- SPI port selection ---
// USE_FSPI_PORT is intentionally NOT set here. With it enabled, TFT_eSPI
// creates its own separate SPIClass(FSPI) object. If we also call SPI.begin()
// ourselves first (to pre-init the bus), both objects race to own SPI2_HOST
// and one of them ends up with an uninitialised peripheral — causing the hang.
// Without it, TFT_eSPI uses a reference to the global SPI object (SPIClass& spi = SPI),
// so our SPI.begin() call in display.cpp and TFT_eSPI's own begin() both operate
// on the same object, which is safe.
// #define USE_FSPI_PORT

// --- Display driver ---
// Uncomment ONLY the driver for your display. All others must stay commented.
#define GC9A01_DRIVER       // Round 240x240 display

// --- Display resolution ---
#define TFT_WIDTH  240
#define TFT_HEIGHT 240

// --- SPI Pin definitions for ESP32-C3 ---
// These match the wiring in the README. Change if you used different pins.
#define TFT_MOSI  6    // SPI Data (labelled SDA on the GC9A01 board)
#define TFT_MISO  3    // GC9A01 is write-only so MISO is unused, but MUST be a
                       // real GPIO — NOT -1. On ESP32-C3, TFT_eSPI has a fixup
                       // (TFT_eSPI_ESP32_C3.h:316) that silently reassigns MISO
                       // to the same pin as MOSI when MISO == -1. The GPIO matrix
                       // then routes both input and output to GPIO6, MOSI never
                       // drives, every SPI transfer hangs, and the watchdog fires.
                       // GPIO2 is free (BL is disabled) and safe to use as a dummy.
#define TFT_SCLK  4    // SPI Clock (labelled SCL)
#define TFT_CS    5    // Chip Select (active LOW)
#define TFT_DC    7    // Data/Command select
#define TFT_RST   8    // Reset (active LOW)
// #define TFT_BL    2    // Backlight — uncomment if your display has a BL pin wired to GPIO2
                           // Leave commented if the display has no BL pin (backlight always on)

// --- SPI frequency ---
// Start conservatively at 10 MHz to rule out signal integrity issues.
// Once display is confirmed working, increase toward 40 MHz.
#define SPI_FREQUENCY       40000000
#define SPI_READ_FREQUENCY  20000000

// --- Fonts to load ---
// Loading all built-in fonts costs flash space. Load only what you need.
// We include a couple of small ones for potential debug text on screen.
#define LOAD_GLCD    // Font 1 — original Adafruit 8-pixel font
#define LOAD_FONT2   // Font 2 — small 16-pixel high font

// --- Smooth fonts (anti-aliased, from SPIFFS/LittleFS) ---
// Disabled for now — we draw the face programmatically, not with text fonts.
// #define SMOOTH_FONT

// --- Colour order ---
// GC9A01 typically uses BGR internally. TFT_eSPI handles this.
// If colours look inverted, try uncommenting: #define TFT_RGB_ORDER TFT_RGB

// --- DMA ---
// ESP32-C3 supports DMA for SPI, which can speed up full-screen redraws.
// Enable once the basic display is working.
#define USE_DMA_TO_TFT

// --- Guard ---
// Must be at the END of this file. Tells TFT_eSPI that setup is complete so
// it doesn't include its own User_Setup.h on top of ours.
#define USER_SETUP_LOADED
