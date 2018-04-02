
void doWeatherScreen() {
  int posX = 100;
  int posY = 100;
  tft.drawBitmap(posX, posY, wind, 80, 80, HX8357_WHITE);

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


