
/*
 *  Status screen with WiFi and TFT LCD Panel
 *
 */


#include <list>
#include "button.h"
#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include "Adafruit_GFX.h"
#include "Adafruit_HX8357.h"
#include <Adafruit_STMPE610.h>
#include <Fonts/Futura32ptCond7b.h>
#include <Fonts/Futura18pt7b.h>
#include <Fonts/Futura14pt7b.h>
#include <Fonts/Futura10pt7b.h>
#include "bitmaps.h"

#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  10        /* Time ESP32 will go to sleep (in seconds) */

/**********************************************
 * TFT LCD SETUP
 * 
 */
#ifdef ESP32
   #define STMPE_CS 32
   #define TFT_CS   15
   #define TFT_DC   33
   #define SD_CS    14
#endif

#define TFT_RST -1

// Use hardware SPI and the above for CS/DC
Adafruit_HX8357 tft = Adafruit_HX8357(TFT_CS, TFT_DC, TFT_RST);

/**********************************************
 * Touchscreen Setup
 * 
 */
Adafruit_STMPE610 ts = Adafruit_STMPE610(STMPE_CS);

// This is calibration data for the raw touch data to the screen coordinates
#define TS_MINX 3800
#define TS_MAXX 100
#define TS_MINY 100
#define TS_MAXY 3750


/**********************************************
 * WiFi Setup
 * 
 */
WiFiMulti WiFiMulti;


/**********************************************
 * JSON Buffer
 */
StaticJsonBuffer<2000> jsonBuffer;


/**********************************************
 * Screen Modes
 */
enum screenMode {SCREEN_STATUS, SCREEN_WEATHER};
screenMode currentMode = SCREEN_STATUS;

/**********************************************
 * Button List
 */
std::list<button> buttons;

/**********************************************
 * Screen Dimensions
 */
int margin = 5;
int screenW = 480;
int screenH = 320;
int frameW = screenW;
int frameH = screenH;

/**********************************************
 * Colors
 */
uint16_t textColor;
uint16_t cardColor;
uint16_t cardTimeColor;
uint16_t cardTextColor;
uint16_t cardTimeTextColor;
uint16_t cardWeatherColor;


/**********************************************
 * Variable to hold the time we start each loop
 * Check every 2 minutes to keep weather requests under 1000/day
 */
unsigned long startLoopTime;
unsigned long loopDelay = 1000 * 60 * 2;  // 2 minutes = 120000 milliseconds

/**********************************************
 * Wait 1 second between firing actions for button taps
 */
unsigned long buttonIgnoreStartTime;
uint16_t buttonIgnoreDelay = 1000;




/**********************************************
 * Setup Function
 * 
 *********************************************/
void setup()
{
  Serial.begin(115200);
  delay(10);
  Serial.println();
  Serial.println();

  // Touchscreen Init
  if (!ts.begin()) {
    Serial.println("Couldn't start touchscreen controller");
    while (1);
  }
  Serial.println("Touchscreen started");

  // TFT LCD Init
  setbacklight(0);
  tft.begin(HX8357D);
  tft.setRotation(3);
  tft.fillScreen(HX8357_BLACK);
  tft.setFont(&Futura18pt7b);
  tft.setTextSize(1);
  tft.setTextWrap(false);

  textColor = tft.color565(200, 200, 200);
  cardColor = tft.color565(160, 160, 225);
  cardTextColor = tft.color565(32, 32, 32);
  cardTimeColor = tft.color565(80, 80, 128);
  cardTimeTextColor = tft.color565(200, 200, 200);
  cardWeatherColor = tft.color565(255, 255, 150);

  // Show Power Icon
  int posX = (screenW / 2) - 32;
  int posY = (screenH / 2) - 32;
  tft.drawBitmap(posX, posY, power, 64, 64, HX8357_GREEN);
  setbacklight(1);
  delay(2000);

  // Present a set of buttons on screen to choose which WiFi
  // network to join.
  chooseNetwork();

  tft.fillScreen(HX8357_BLACK);

}

/**********************************************
 * Main Loop
 * 
 *********************************************/
void loop()
{
  drawScreen();
  
  // Wait for loopDelay so we're not refreshing the screen and loading data too often.
  startLoopTime = millis();
  int lastTimerLineLen = 0;
  while ( millis() < (startLoopTime + loopDelay) ) {
    // Serial.printf("m: %d, s: %d, d: %d, t: %d\n", millis(), startLoopTime, loopDelay, startLoopTime + loopDelay);
    int curTimerLineLen = 24 - (24 * ( ((float)millis() - (float)startLoopTime) / (float)loopDelay));
    if (curTimerLineLen != lastTimerLineLen) {
      drawTimerLine(curTimerLineLen);
      lastTimerLineLen = curTimerLineLen;
    }
    checkButtonTaps();
    delay(10);
  }
}

/**********************************************
 * Draw A Screen
 * 
 *********************************************/
void drawScreen() {
  tft.fillScreen(HX8357_BLACK);
  monitorBattery();

  switch (currentMode) {
    case SCREEN_STATUS:
      doStatusMessage();
      doCalendarEvents();
      break;

    case SCREEN_WEATHER:
      doWeatherScreen();
      break;
  }

}

/**********************************************
 * Check every button for a tap, and fire its
 * action if pressed.
 *********************************************/
bool checkButtonTaps() {
  // If nothing's being touched, then we're done.
  if (!ts.touched()) {
    return false;
  }

  // If it hasn't been long enough since our last tap action fired, skip this tap
  if ( millis() < (buttonIgnoreStartTime + buttonIgnoreDelay) ) {
    return false;
  }
  
  // Retrieve a point  
  TS_Point p = ts.getPoint();

  // Scale from ~0->4000 to tft.width using the calibration #'s
  p.x = map(p.x, TS_MINY, TS_MAXY, 0, tft.height());
  p.y = map(p.y, TS_MINX, TS_MAXX, 0, tft.width());

  // Rotate Coordinates ccw
  int tx = p.x;
  p.x = p.y;
  p.y = tx;

  // Serial.println();
  // Serial.print("X = "); Serial.print(p.x); Serial.print("\tY = "); Serial.print(p.y);  Serial.print("\tPressure = "); Serial.println(p.z); 
  // tft.fillCircle(p.x-10, p.y-10, 20, HX8357_WHITE);

  for (std::list<button>::iterator it=buttons.begin(); it != buttons.end(); ++it) {
    button b = (*it);

    // Serial.printf("x1: %d, y1: %d, \t x2: %d, y2: %d\n", b.x1, b.y1, b.x2, b.y2);    

    if (   p.x >= b.x1
        && p.x <  b.x2
        && p.y >= b.y1
        && p.y <  b.y2 )
    {
      Serial.println("Button Pressed");
      b.action();
      buttonIgnoreStartTime = millis();
      buttons.clear();
      return true;
    }
  }

  return false;
}

void doHomeNetwork() {
  Serial.println("Home Network");
  WiFiMulti.addAP("arduino", "arduinotest");
  finishNetworkSetup();
}

void doWorkNetwork() {
  Serial.println("Work Network");
  WiFiMulti.addAP("UAGuest", "arduinotest");
  finishNetworkSetup();
}

/**********************************************
 * Draw some network choise buttons and poll for
 * button presses.
 *********************************************/
void chooseNetwork() {
  bool buttonPressed = false;
  int buttonW = 150;
  int buttonH = 100;
  int margin = 10;
  int posX = 0;
  int posY = 0;

  tft.fillScreen(HX8357_BLACK);
  
  tft.setFont(&Futura32ptCond7b);
  tft.setTextColor(cardTimeTextColor);
  
  // Home WiFi Button
  posY = (screenH / 2) - (buttonH / 2);
  posX = (screenW / 2) - (buttonW) - (margin / 2);
  buttons.push_back( { posX, posY, posX+buttonW, posY+buttonH, doHomeNetwork } );
  tft.fillRect(posX, posY, buttonW, buttonH, cardTimeColor);
  tft.setCursor(posX + 16, posY + 75);
  tft.println("Home");

  // Work WiFi Button
  posX = (screenW / 2) + (margin / 2);
  buttons.push_back( { posX, posY, posX+buttonW, posY+buttonH, doWorkNetwork } );
  //Serial.printf("x1: %d, y1: %d, w: %d, h: %d, x2: %d, y2: %d\n", posX, posY, buttonW, buttonH, posX+buttonW, posY+buttonH);
  tft.fillRect(posX, posY, buttonW, buttonH, cardTimeColor);
  tft.setCursor(posX + 18, posY + 75);
  tft.println("Work");

  while ( !buttonPressed ) {
    buttonPressed = checkButtonTaps();
    delay(10);
  }
}

void finishNetworkSetup() {
  Serial.println();
  Serial.print("Wait for WiFi... ");

  tft.fillScreen(HX8357_BLACK);

  // Show WiFi Icon
  int posX = (screenW / 2) - 32;
  int posY = (screenH / 2) - 32;
  tft.fillScreen(HX8357_BLACK);
  tft.drawBitmap(posX, posY, wifi, 64, 64, HX8357_WHITE);

  int connectAttempts = 0;
  while(WiFiMulti.run() != WL_CONNECTED) {
      Serial.print(".");
      // If we can't connect to WiFi after 10 or so tries, restart and start over.
      if (connectAttempts++ == 10) {
        ESP.restart();
      }
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println(WiFi.macAddress());

  // Show WiFi Icon
  tft.fillScreen(HX8357_BLACK);
  tft.drawBitmap(posX, posY, wifi, 64, 64, HX8357_BLUE);
  delay(2000);
}

/**********************************************
 * printCenteredText
 * 
 * Try and draw text centered within some
 * bounds.
 * 
 *********************************************/
void printCenteredText(String text, int offsetY, int leftBound, int rightBound) {
  int16_t  x1, y1;
  uint16_t w, h;
  char buf[text.length()];
  text.toCharArray(buf, text.length());
  tft.getTextBounds(buf, leftBound, offsetY, &x1, &y1, &w, &h);
  int offsetX = leftBound + (rightBound - leftBound)/2 - (w/2);
  // tft.drawRect(offsetX, y1, w, h, cardTimeTextColor);
  tft.setCursor(offsetX-15, offsetY);
  tft.println(text);

  // Serial.printf("lb: %d   rb: %d  oX: %d  w: %d \n", leftBound, rightBound, offsetX, w);
}

/**********************************************
 * monitorBattery
 * 
 * Measure the battery voltage and draw a 
 * battery meter icon on the screen.
 * 
 *********************************************/
void monitorBattery() {
  int rawBatt = analogRead(A13);
  int battLevel = rawBatt * 2;

  int battW = 24;
  int battH = 12;
  int posX = screenW - 24 - 10;
  int posY = 10;

  tft.fillRect(posX, posY, battW, battH, HX8357_BLACK);

  if (battLevel < 3600) {
    tft.drawBitmap(posX, posY, batt0, battW, battH, HX8357_RED);
  } else if (battLevel < 3700) {
    tft.drawBitmap(posX, posY, batt1, battW, battH, HX8357_YELLOW);
  } else if (battLevel < 3850) {
    tft.drawBitmap(posX, posY, batt2, battW, battH, HX8357_WHITE);
  } else if (battLevel < 4150) {
    tft.drawBitmap(posX, posY, batt3, battW, battH, HX8357_WHITE);
  } else {
    tft.drawBitmap(posX, posY, batt4, battW, battH, HX8357_WHITE);
  }

}

void drawTimerLine(int len) {
  int posX = screenW - 24 - 10;
  int posY = 24;
  tft.fillRect(posX, posY, 24, 2, HX8357_BLACK);
  tft.fillRect(posX, posY, len, 2, HX8357_WHITE);
}

void goToSleep() {
  tft.fillScreen(HX8357_BLACK);
  setbacklight(0);
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  esp_deep_sleep_start();  
}

void wakeUp() {
  setbacklight(1);
}

/**********************************************
 * setbacklight
 * 
 * Use the Touchscreen controller to turn on
 * or off the backlight on the LCD panel.
 * 
 *********************************************/
void setbacklight(uint8_t status) {
  if (status == 0) {
    // Turn backlight off via STMPE
    ts.writeRegister8(STMPE_GPIO_DIR, 1<<2);
    ts.writeRegister8(STMPE_GPIO_ALT_FUNCT, 1<<2);
    ts.writeRegister8(STMPE_GPIO_CLR_PIN, 1<<2);
  } else {
    // turn backlite on via STMPE
    ts.writeRegister8(STMPE_GPIO_SET_PIN, 1<<2);
  }
}


