//This sketch is taking Adafruit's Supernova Poi code and is modified to use FastLed with ESP8266. 
//Using #include graphicsNoprogmem.h and using imageinit() in void setup, the poi work but store patterns in RAM, limiting number of patterns. 
//Using #include graphicswithprogmem.h and imageinitwithprogmem in void setup, the poi contiually restart. 
//Seeking help to store patterns in flash memory and increase number of patterns stored in flash.

#include <PGMSPACE.h>
#include <Arduino.h>
#include "FastLED.h"
#define NUM_LEDS 32

CRGB leds[NUM_LEDS];
// #include <SPI.h> // Enable this line on Pro Trinket

//These are attempts to change how progmem stores in memory. 
//#define PROGMEM   ICACHE_RODATA_ATTR
//#define ICACHE_RODATA_ATTR  __attribute__((section(".irom.text")))
//#define PGM_P       const char * 
//#define PGM_VOID_P  const void * 
//#define PSTR(s) (__extension__({static const char __c[] PROGMEM = (s); &__c[0];}))
//#undef pgm_read_byte(addr)

#define pgm_read_byte(addr)                                                \
(__extension__({                                                               \
    PGM_P __local = (PGM_P)(addr);  /* isolate varible for macro expansion */         \
    ptrdiff_t __offset = ((uint32_t)__local & 0x00000003); /* byte aligned mask */            \
    const uint32_t* __addr32 = (const uint32_t*)((const uint8_t*)(__local)-__offset); \
    volatile uint8_t __result = ((*__addr32) >> (__offset * 8));                        \
    __result;                                                                  \
}))




typedef uint16_t line_t; // Bigger images OK on other boards


//Include one of these, but make sure to include the matching part in void setup.
#//include "graphicswithProgmem.h"; //stores patterns using progmem but causes board to constantly restart
#include "graphicsNoprogmem.h";   //works but does not store in flash, see void setup and ccomment out imageinit or imageinitwithprogmem


#define DATA_PIN  7
#define CLOCK_PIN 5



boolean autoCycle = true; // Set to true to cycle images by default
#define CYCLE_TIME 4      // Time, in seconds, between auto-cycle images


void     imageInit(void);
void     imageInitwithProgmem(void);


void setup() {
  FastLED.addLeds<APA102, DATA_PIN, CLOCK_PIN, RGB>(leds, NUM_LEDS);
  FastLED.show(); // before measuring battery
  
   // modify this part along with the #include graphics file part above
   // imageInitwithProgmem();    //use with graphicwithProgmem.h  <<<<<<<<<<<<<<<<<<<<<<<<<include this line and comment out imaaginit or imagewithprogmem.    
      imageInit();               //use with graphicsNoprogmem.h


}
// GLOBAL STATE STUFF ------------------------------------------------------

uint32_t lastImageTime = 0L; // Time of last image change
uint8_t  imageNumber   = 0,  // Current image being displayed
         imageType,          // Image type: PALETTE[1,4,8] or TRUECOLOR
        *imagePalette,       // -> palette data in PROGMEM
        *imagePixels,        // -> pixel data in PROGMEM
         palette[16][3];     // RAM-based color table for 1- or 4-bit images
line_t   imageLines,         // Number of lines in active image
         imageLine;          // Current line number in image


void imageInit() { // Works with graphics.h but does not read from PROGMEM
  imageType    = images[imageNumber].type;
  imageLines   = images[imageNumber].lines;
  imageLine    = 0;
  imagePalette = (uint8_t *)images[imageNumber].palette;
  imagePixels  = (uint8_t *)images[imageNumber].pixels;
  // 1- and 4-bit images have their color palette loaded into RAM both for
  // faster access and to allow dynamic color changing.  Not done w/8-bit
  // because that would require inordinate RAM (328P could handle it, but
  // I'd rather keep the RAM free for other features in the future).
  if(imageType == PALETTE1)      memcpy_P(palette, imagePalette,  2 * 3);
  else if(imageType == PALETTE4) memcpy_P(palette, imagePalette, 16 * 3);
  lastImageTime = millis(); // Save time of image init for next auto-cycle
}


void imageInitwithProgmem() { // this version is not working on ESP8266 but is used to read from PROGMEM 
  imageType    = pgm_read_byte(&images[imageNumber].type);
  imageLines   = pgm_read_word(&images[imageNumber].lines);
  imageLine    = 0;
  imagePalette = (uint8_t *)pgm_read_word(&images[imageNumber].palette); 
  imagePixels  = (uint8_t *)pgm_read_word(&images[imageNumber].pixels);
  // 1- and 4-bit images have their color palette loaded into RAM both for
  // faster access and to allow dynamic color changing.  Not done w/8-bit
  // because that would require inordinate RAM (328P could handle it, but
  // I'd rather keep the RAM free for other features in the future).
  if(imageType == PALETTE1)      memcpy_P(palette, imagePalette,  2 * 3);
  else if(imageType == PALETTE4) memcpy_P(palette, imagePalette, 16 * 3);
  lastImageTime = millis(); // Save time of image init for next auto-cycle
}

void nextImage(void) {
  if(++imageNumber >= NUM_IMAGES) imageNumber = 0;
  imageInit();
}

// MAIN LOOP ---------------------------------------------------------------

void loop() {
  uint32_t t = millis();               // Current time, milliseconds
  if(autoCycle) {
    if((t - lastImageTime) >= (CYCLE_TIME * 1000L)) nextImage();
  }


  switch(imageType) {

    case PALETTE1: { // 1-bit (2 color) palette-based image
      uint8_t  pixelNum = 0, byteNum, bitNum, pixels, idx,
              *ptr = (uint8_t *)&imagePixels[imageLine * NUM_LEDS / 8];
      for(byteNum = NUM_LEDS/8; byteNum--; ) { // Always padded to next byte
        pixels = pgm_read_byte(ptr++);  // 8 pixels of data (pixel 0 = LSB)
        for(bitNum = 8; bitNum--; pixels >>= 1) {
          idx = pixels & 1; // Color table index for pixel (0 or 1)
          leds[pixelNum++] = CRGB (palette[idx][0], palette[idx][1], palette[idx][2]);
        }
      }
      break;
    }

    case PALETTE4: { // 4-bit (16 color) palette-based image
      uint8_t  pixelNum, p1, p2,
              *ptr = (uint8_t *)&imagePixels[imageLine * NUM_LEDS / 2];
      for(pixelNum = 0; pixelNum < NUM_LEDS; ) {
        p2  = pgm_read_byte(ptr++); // Data for two pixels...
        p1  = p2 >> 4;              // Shift down 4 bits for first pixel
        p2 &= 0x0F;                 // Mask out low 4 bits for second pixel
       leds[pixelNum++] = CRGB (palette[p1][0], palette[p1][1], palette[p1][2]);
       leds[pixelNum++] = CRGB (palette[p2][0], palette[p2][1], palette[p2][2]);
      }
      break;
    }

#if 0 // Yep, demo images need ALL THE SPACE (see comment above)
    case PALETTE8: { // 8-bit (256 color) PROGMEM-palette-based image
      uint16_t  o;
      uint8_t   pixelNum,
               *ptr = (uint8_t *)&imagePixels[imageLine * NUM_LEDS];
      for(pixelNum = 0; pixelNum < NUM_LEDS; pixelNum++) {
        o = pgm_read_byte(ptr++) * 3; // Offset into imagePalette
      leds[pixelNum++] = CRGB(
          pgm_read_byte(&imagePalette[o]),
          pgm_read_byte(&imagePalette[o + 1]),
          pgm_read_byte(&imagePalette[o + 2]));
      }
      break;
    }

    case TRUECOLOR: { // 24-bit ('truecolor') image (no palette)
      uint8_t  pixelNum, r, g, b,
              *ptr = (uint8_t *)&imagePixels[imageLine * NUM_LEDS * 3];
      for(pixelNum = 0; pixelNum < NUM_LEDS; pixelNum++) {
        r = pgm_read_byte(ptr++);
        g = pgm_read_byte(ptr++);
        b = pgm_read_byte(ptr++);
       leds[pixelNum] = (pixelNum, r, g, b);
      }
      break;
    }
#endif
  }

    FastLED.show(); // Refresh LEDs
#if !defined(LED_DATA_PIN) && !defined(LED_CLOCK_PIN)
  delayMicroseconds(900);  // Because hardware SPI is ludicrously fast
#endif
  if(++imageLine >= imageLines) imageLine = 0; // Next scanline, wrap around
}


