#include <Adafruit_GFX.h>
#include <Adafruit_SSD1351.h>
#include <SPI.h>

// CA settings
#define CA_WIDTH    112 // must be a multiple of 8 for bit packing
#define CA_HEIGHT   112 // must be a multiple of 8 for bit packing
#define REPETITIONS 4   // number of full-screen generations for each rule

// CA parameters
#define X ((SCREEN_WIDTH - CA_WIDTH) / 2)
#define Y ((SCREEN_HEIGHT - CA_HEIGHT) / 2)
#define DEPTH (REPETITIONS * CA_HEIGHT)

// SSD1351 parameters
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 128
#define RST_PIN       9
#define DC_PIN        8
#define CS_PIN        10

// Colors
#define	BLACK 0x0000
#define GRAY  0x4a49
#define CYAN  0x07FF

// SPI parameters and macros
#define TFT_CS  10
#define TFT_DC  8
#define SPI_WRITE(value) while (!(SPSR & (1 << SPIF))); SPDR = (value);

// Macros for bit manipulation of the CA state
#define DATA(i, j) ((data[(CA_WIDTH * (j) + (i)) / 8] >> ((7 - (i)) % 8)) & 1)
#define ON(i, j)   (data[(CA_WIDTH * (j) + (i)) / 8] |= (1 << ((7 - (i)) % 8))) 
#define OFF(i, j)  (data[(CA_WIDTH * (j) + (i)) / 8] &= ~(1 << ((7 - (i)) % 8)))

// Selection of interesting rules under random initial state
const uint8_t rules[] = { 26, 30, 41, 45, 54, 60, 73, 75, 82, 86, 89, 90, 101,
                          102, 105, 106, 107, 109, 110, 120, 121, 124, 135, 137,
                          147, 149, 150, 153, 154, 165, 166, 167, 169, 180, 181,
                          193, 195, 210, 225 };

// Global variables
uint8_t old_rule;
uint8_t new_rule;
uint8_t data[(CA_HEIGHT * CA_WIDTH) / 8] = { 0 };
Adafruit_SSD1351 tft = Adafruit_SSD1351(SCREEN_WIDTH, SCREEN_HEIGHT, &SPI,
                                        CS_PIN, DC_PIN, RST_PIN);

void setup(void) {

  // Set up the display
  tft.begin();
  tft.fillScreen(BLACK);

  // Set random initial state
  for (size_t i = 0; i < CA_WIDTH; ++i) {
    if (rand() % 2) ON(i, 0);
    else OFF(i, 0);
  }

  // Start with nicest rule (use rules[rand() % sizeof(rules)] for random rule)
  new_rule = 105;

  // Run the CA
  for (size_t j = 1; j < CA_HEIGHT; ++j) {
    updateRow(j, new_rule);
  }
  
  // Draw the CA on the display
  fastDrawBitmap(X, Y, data, CA_WIDTH, CA_HEIGHT, CYAN, BLACK);
}

void loop() {

  // Print rule number under the CA
  tft.fillRect(X, SCREEN_HEIGHT - 7, 20, 7, BLACK);
  tft.setCursor(X, SCREEN_HEIGHT - 7);
  tft.setTextColor(GRAY);
  tft.setTextSize(1);
  tft.println(new_rule);

  // Update rules
  old_rule = new_rule;
  new_rule = rules[rand() % sizeof(rules)];
  
  // Run the CA for DEPTH generations
  for (size_t k = 0; k < DEPTH; ++k) {

    // Move the CA state up by one row
    for (size_t j = 0; j < CA_HEIGHT - 1; ++j) {
      for (size_t i = 0; i < CA_WIDTH / 8; ++i) {
        data[(CA_WIDTH * j) / 8 + i] = data[(CA_WIDTH * (j + 1)) / 8 + i];
      }
    }

    // Introduce random state at end of current generation if needed
    if (DEPTH - k + 2 == CA_HEIGHT) {

      // Check whether last row is all on or all off
      uint8_t on = 1, off = 1;
      for (size_t i = 0; i < CA_WIDTH; ++i) {
        on &= DATA(i, CA_HEIGHT - 2), off &= !DATA(i, CA_HEIGHT - 2);
      }

      // If so, re-randomize it to prevent stagnation
      if (on || off) {
        for (size_t i = 0; i < CA_WIDTH; ++i) {
          if (rand() % 2) ON(i, CA_HEIGHT - 2);
          else OFF(i, CA_HEIGHT - 2);
        }
      }
    }

    // Update the last row based on the current rule
    updateRow(CA_HEIGHT - 1, DEPTH - k + 1 > CA_HEIGHT ? old_rule : new_rule);

    // Draw the whole CA on the display
    fastDrawBitmap(X, Y, data, CA_WIDTH, CA_HEIGHT, CYAN, BLACK);
  }
}

// Update the CA state for row j based on the rule and the previous row
void updateRow(size_t j, uint8_t rule) {
  for (size_t i = 0; i < CA_WIDTH; ++i) {
    uint8_t conf = 0;
    conf |= DATA((i + 1) % CA_WIDTH, j - 1);
    conf |= (DATA(i, j - 1) << 1);
    conf |= (DATA((i + CA_WIDTH - 1) % CA_WIDTH, j - 1) << 2);
    if ((rule >> conf) & 1) ON(i, j);
    else OFF(i, j);
  }
}

// Use direct SPI communication to draw the bitmap faster than Adafruit's library allows
// See https://forums.adafruit.com/viewtopic.php?t=198726
void fastDrawBitmap(int16_t x, int16_t y, const uint8_t bitmap[], int16_t w, int16_t h, uint16_t fg, uint16_t bg) {

  // Set up the SPI transaction
  digitalWrite(TFT_CS, LOW);
  
  // Set column address (0x15 = SSD1351 column address command)
  digitalWrite(TFT_DC, LOW);
  SPI_WRITE(0x15);
  digitalWrite(TFT_DC, HIGH);
  SPI_WRITE(x);
  SPI_WRITE(x + w - 1);

  // Set row address (0x75 = SSD1351 row address command)
  digitalWrite(TFT_DC, LOW);
  SPI_WRITE(0x75);
  digitalWrite(TFT_DC, HIGH);
  SPI_WRITE(y);
  SPI_WRITE(y + h - 1);

  // Write RAM (0x5C = SSD1351 write RAM command)
  digitalWrite(TFT_DC, LOW);
  SPI_WRITE(0x5C);
  digitalWrite(TFT_DC, HIGH);

  // Set bit packing parameters
  int16_t byteWidth = (w + 7) >> 3;
  uint8_t bits8 = 0;

  // Draw the bitmap
  for (int16_t j = 0; j < h; ++j) {
    for (int16_t i = 0; i < w; ++i) {
      if (i & 7) bits8 <<= 1;
      else bits8 = bitmap[j * byteWidth + (i >> 3)];
      uint16_t c = (bits8 & 0x80) ? fg : bg;
      SPI_WRITE(c >> 8);
      SPI_WRITE(c);
    }
  }

  // Finish the SPI transaction
  digitalWrite(TFT_CS, HIGH);
}