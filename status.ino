
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


