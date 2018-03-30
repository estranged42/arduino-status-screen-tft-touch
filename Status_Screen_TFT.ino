
/*
 *  Status screen with WiFi and TFT LCD Panel
 *
 */

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

  // WiFi Init
  // We start by connecting to a WiFi network
  // WiFiMulti.addAP("arduino", "arduinotest");
  WiFiMulti.addAP("UAGuest", "arduinotest");

  Serial.println("");
  Serial.println("");
  Serial.println("Starting up");
  Serial.println(WiFi.macAddress());

  Serial.println();
  Serial.print("Wait for WiFi... ");

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
  tft.fillScreen(HX8357_BLACK);

}

/**********************************************
 * Main Loop
 * 
 *********************************************/
void loop()
{
  tft.fillScreen(HX8357_BLACK);
  monitorBattery();
  doStatusMessage();
  doCalendarEvents();
  // Check every 2 minutes to keep weather requests under 1000/day
  int i = 0;
  while (i < 120) {
    drawTimerLine( 24 - (24 * ((float)i/120.0)) );
    delay(1000);
    i++;
  }
  // goToSleep();
}

/**********************************************
 * doStatusMessage
 * 
 * Get current status message from adafruit.io
 * 
 *********************************************/
void doStatusMessage() {
  // Use WiFiClient class to create TCP connections
  HTTPClient http;

  http.begin("http://io.adafruit.com/api/v2/estranged/feeds/mark-status/data/?limit=1&include=value"); //HTTP

  // start connection and send HTTP header
  int httpCode = http.GET();

  // httpCode will be negative on error
  if(httpCode > 0) {
      // HTTP header has been send and Server response header has been handled
      Serial.printf("[HTTP] GET... code: %d\n", httpCode);

      // file found at server
      if(httpCode == HTTP_CODE_OK) {
          String payload = http.getString();
          Serial.println(payload);

          jsonBuffer.clear();
          JsonArray& jsonArray = jsonBuffer.parseArray(payload);
          Serial.println(jsonBuffer.size());
          const char* statusValue = jsonArray[0]["value"];
          Serial.println(statusValue);
          String statusString = String(statusValue);

          displayMessage(statusString);
          
      }
  } else {
      Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
  }

  http.end();
}


/**********************************************
 * displayMessage
 * 
 * Display status message on the screen.
 * 
 *********************************************/
void displayMessage(String statusString) {
  int newLineIndex = statusString.indexOf("|");
  Serial.println(newLineIndex);
  String lineOne = statusString;
  String lineTwo;
  if (newLineIndex > 0) {
    lineOne = statusString.substring(0, newLineIndex);
    lineTwo = statusString.substring(newLineIndex + 1);
  }

  tft.setTextColor( textColor );
  tft.setTextSize(1);
  tft.setFont(&Futura18pt7b);
  printCenteredText(lineOne, 50, 0, screenW);
  
  if (newLineIndex > 0) {
    tft.setFont(&Futura14pt7b);
    printCenteredText(lineTwo, tft.getCursorY(), 0, screenW);
  }

}


/**********************************************
 * doCalendarEvents
 * 
 * Get calendar events from Lambda and draw
 * them on the screen.
 * 
 *********************************************/
void doCalendarEvents() {
  // Use WiFiClient class to create TCP connections
  HTTPClient http;

  http.begin("https://rf9vvsmdld.execute-api.us-west-2.amazonaws.com/Prod/status"); //HTTPS

  // start connection and send HTTP header
  int httpCode = http.GET();

  // httpCode will be negative on error
  if(httpCode > 0) {
    // HTTP header has been send and Server response header has been handled
    Serial.printf("[HTTPS] GET... code: %d\n", httpCode);

    // file found at server
    if(httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      Serial.println(payload);

      jsonBuffer.clear();
      //JsonArray& jsonArray = jsonBuffer.parseArray(payload);
      JsonObject& jsonObject = jsonBuffer.parseObject(payload);
      JsonArray& jsonCalendarArray = jsonObject["meetings"];
      JsonObject& jsonWeather = jsonObject["weather"];

      if (jsonCalendarArray.size() > 0) {
        displayEventCard(jsonCalendarArray[0].as<JsonObject>(), 140);
      }

      if (jsonCalendarArray.size() > 1) {
        displayEventCard(jsonCalendarArray[1].as<JsonObject>(), 230);
      }

      if (jsonCalendarArray.size() < 2) {
        // Display Weather Card
        displayWeatherCard(jsonWeather, 230);
      }
      
    }
  } else {
     Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
  }

  http.end();
}


/**********************************************
 * displayEventCard
 * 
 * Draw an event card on the screen for a 
 * given calendar event.
 * 
 *********************************************/
void displayEventCard(ArduinoJson::JsonObject &event, int offsetY) {
  int cardMargin = 5;
  int cardW = screenW - (cardMargin * 2);
  int cardH = 80;

  int timeW = 125;

  int descOffset = cardMargin + timeW;
  int descW = cardW - timeW;

  int line1Baseline = 30;
  int line2Baseline = 65;

  // Time Background
  tft.fillRect(cardMargin, offsetY, timeW, cardH, cardTimeColor);
  // Description Background
  tft.fillRect(descOffset, offsetY, descW, cardH, cardColor);

  // Print Start Time
  tft.setFont(&Futura14pt7b);
  tft.setTextColor(cardTimeTextColor);
  tft.setCursor(cardMargin + cardMargin, offsetY + line1Baseline);
  const char* startChar = event["start"];
  String startString = String(startChar);
  printCenteredText(startString, offsetY + line1Baseline, cardMargin, timeW + cardMargin);

  tft.drawFastHLine(cardMargin*3, offsetY + line1Baseline + 8, timeW - cardMargin*4, cardTimeTextColor);

  // Print End Time
  tft.setFont(&Futura14pt7b);
  tft.setTextColor(cardTimeTextColor);
  tft.setCursor(cardMargin + cardMargin, offsetY + line2Baseline);
  const char* endChar = event["end"];
  String endString = String(endChar);
  printCenteredText(endString, offsetY + line2Baseline, cardMargin, timeW + cardMargin);

  // Print Subject
  tft.setFont(&Futura14pt7b);
  tft.setTextColor(cardTextColor);
  tft.setCursor(descOffset + cardMargin, offsetY + line1Baseline);
  const char* subjChar = event["subject"];
  String subjString = String(subjChar);
  tft.println(subjString);

  // Print Location
  tft.setFont(&Futura10pt7b);
  tft.setTextColor(cardTextColor);
  tft.setCursor(descOffset + cardMargin, offsetY + line2Baseline);
  const char* locChar = event["location"];
  String locString = String(locChar);
  tft.println(locString);
}


/**********************************************
 * displayWeatherCard
 * 
 * Draw a weather card on the screen.
 * 
 *********************************************/
void displayWeatherCard(ArduinoJson::JsonObject &weather, int offsetY) {
  int16_t  x1, y1;
  uint16_t w, h;
  
  int cardMargin = 5;
  int cardW = screenW - (cardMargin * 2);
  int cardH = 80;

  int timeW = 125;

  int descOffset = cardMargin + timeW;
  int descW = cardW - timeW;

  int line1Baseline = 30;
  int line2Baseline = 65;

  int posX = cardMargin;
  int posY = offsetY;
  
  // Weather Background
  tft.fillRect(cardMargin, offsetY, cardW, cardH, cardWeatherColor);

  // Weather Icon
  const char* icon = weather["icon"];
  uint8_t* currentIcon;
  if (strcmp(icon, "clear-day") == 0) {
    currentIcon = (uint8_t*)clear_day;
  } else if (strcmp(icon, "clear-night") == 0) {
    currentIcon = (uint8_t*)clear_night;
  } else if (strcmp(icon, "rain") == 0) {
    currentIcon = (uint8_t*)rain;
  } else if (strcmp(icon, "snow") == 0) {
    currentIcon = (uint8_t*)snow;
  } else if (strcmp(icon, "sleet") == 0) {
    currentIcon = (uint8_t*)sleet;
  } else if (strcmp(icon, "wind") == 0) {
    currentIcon = (uint8_t*)wind;
  } else if (strcmp(icon, "fog") == 0) {
    currentIcon = (uint8_t*)fog;
  } else if (strcmp(icon, "cloudy") == 0) {
    currentIcon = (uint8_t*)cloudy;
  } else if (strcmp(icon, "partly-cloudy-day") == 0) {
    currentIcon = (uint8_t*)partly_cloudy_day;
  } else if (strcmp(icon, "partly-cloudy-night") == 0) {
    currentIcon = (uint8_t*)partly_cloudy_night;
  } else {
    Serial.printf("Unknown Weather Icon: %s \n", icon);
    currentIcon = (uint8_t*)clear_day;
  }
  posX = cardMargin;
  posY = offsetY;
  tft.drawBitmap(posX, posY, currentIcon, 80, 80, HX8357_BLACK);

  // Current Temp
  const char* temp = weather["temperature"];
  posX = cardMargin + 80;
  posY = offsetY + 52;
  tft.setFont(&Futura32ptCond7b);
  tft.setTextColor(HX8357_BLACK);
  tft.getTextBounds( (char*)temp, posX, posY, &x1, &y1, &w, &h);
  int curTempW = w;
  tft.setCursor(posX, posY);
  tft.println(temp);

  // High Temp
  const char* tempH = weather["high"];
  tft.setFont(&Futura10pt7b);
  tft.setTextColor(HX8357_RED);
  tft.getTextBounds( (char*)tempH, posX, posY, &x1, &y1, &w, &h);
  posX = cardMargin + 80 + (curTempW/2) - (w/2);
  posY = offsetY + 74;
  tft.setCursor(posX, posY);
  tft.println(tempH);

  // Summary
  JsonArray& summaryLines = weather["summary"];
  tft.setFont(&Futura10pt7b);
  tft.setTextColor(HX8357_BLACK);
  posX = cardMargin + 80 + curTempW + 12;
  if (summaryLines.size() == 1) {
    posY = offsetY + 45;
    tft.setCursor(posX, posY);
    tft.println(summaryLines[0].as<const char*>());
  } else if (summaryLines.size() == 2) {
    posY = offsetY + 30;
    tft.setCursor(posX, posY);
    tft.println(summaryLines[0].as<const char*>());
    posY = offsetY + 60;
    tft.setCursor(posX, posY);
    tft.println(summaryLines[1].as<const char*>());
  } else {
    posY = offsetY + 22;
    tft.setCursor(posX, posY);
    tft.println(summaryLines[0].as<const char*>());
    posY = offsetY + 48;
    tft.setCursor(posX, posY);
    tft.println(summaryLines[1].as<const char*>());
    posY = offsetY + 73;
    tft.setCursor(posX, posY);
    tft.println(summaryLines[2].as<const char*>());
  }

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


