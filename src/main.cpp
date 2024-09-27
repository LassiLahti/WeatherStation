#include <Arduino.h>
#include <inttypes.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <SPIFFS.h>
#include <Wire.h>
#include <GxEPD.h>
#include <GxIO/GxIO_SPI/GxIO_SPI.h>
#include <GxGDEW042T2/GxGDEW042T2.h>
#include <GxIO/GxIO.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include "bitmap.h"
#include "weathericons.h"

#define CS 5
#define DC 17
#define RST 16
#define BUSY  4

// DONT RUN LIKE THIS!! DO SOMETHING ABOUT THE PASSWORDS AND API KEYS
#define WIFI_NAME 
#define PASSWORD 
#define API_KEY
///////////////////////////////
#define BASE_URL "http://api.weatherapi.com/v1"
#define CURRENT_WEATHER "/current.json"
#define MY_CITY "Turku"
#define SAVEFILE "data.txt"


// prototypes for proper call order
char* generateUrl();
void freeUrl(char* url);
void makeApiRequest(const char *url);
void connectWiFi();
void drawScreen();
void WifiConnected();
void displayWeatherData(String city, float latitude, float longitude, float windSpd, String windDir, float temperature, String condition, String last_updated_time);

int testflag = 0;
int no_connection = 0;
const uint64_t wakeupIntervalSeconds = 5 * 60 * 60;
const GFXfont* f = &FreeMonoBold12pt7b;
const GFXfont* windspeed = &FreeMonoBold9pt7b;

//dont know dont care
GxIO_Class io(SPI, SS, DC, RST);
GxEPD_Class display(io, RST, BUSY);

void setup() {

  testflag = 0;
  display.init(115200);
  display.setTextColor(GxEPD_BLACK);
  display.fillScreen(GxEPD_WHITE);
  display.update();
  
  connectWiFi();

  while (WiFi.status() != WL_CONNECTED){

  }

  WifiConnected();

  delay(10);
}

void loop() {

  if (testflag == 0) {
    char *request_url = generateUrl();
    display.fillScreen(GxEPD_BLACK);
    display.update();
    display.fillScreen(GxEPD_WHITE);
    display.update();
    // Serial.println(request_url);
    makeApiRequest(request_url);
    freeUrl(request_url);
    display.update();
    display.powerDown();
    testflag = 1;
    // Sleep for 5 hours before waking up again
    esp_sleep_enable_timer_wakeup(wakeupIntervalSeconds * 1000000);
  }

}

void makeApiRequest(const char* url) {

  HTTPClient http;

  Serial.print("\n\nMaking API request to: ");
  Serial.println(url);
  Serial.print("\n");

  http.begin(url);

  int httpCode = http.GET();

  if (httpCode > 0) {

    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();

      // Parse JSON data
      DynamicJsonDocument doc(1024);
      deserializeJson(doc, payload);

      // Access JSON data
      String city = doc["location"]["name"].as<String>();
      float latitude = doc["location"]["lat"];
      float longitude = doc["location"]["lon"];
      float temperature = doc["current"]["temp_c"];
      String condition = doc["current"]["condition"]["text"].as<String>();
      float windSpd = doc["current"]["wind_kph"];
      String windDir = doc["current"]["wind_dir"].as<String>();
      String last_update = doc["current"]["last_updated"].as<String>();

      String timePart = last_update.substring(11, 16);

      displayWeatherData(city, latitude, longitude, windSpd, windDir, temperature, condition, timePart);

    } else {

      Serial.printf("HTTP request failed with error code: %d\n", httpCode);

    }
  } else {
    Serial.println("HTTP request failed");
  }

  http.end();
}

// Wtf is this shit, just change to GxEPD2 for partial updates
void displayWeatherData(String city, float latitude, float longitude, float windSpd, String windDir, float temperature, String condition, String last_updated_time) {

  // Display city
  display.setCursor(60, 40);
  display.print("City");
  display.setTextSize(2);
  display.setCursor(25, 90);
  display.print(city);
  display.setTextSize(1);

  // Display latitude and longitude
  display.setCursor(50, 130);
  display.print("LAT/LON");
  display.setCursor(20, 160);
  display.print(String(latitude) + " " + String(longitude));
  display.setTextSize(1);

  // Display wind speed and direction
  display.setCursor(30, 190);
  display.print("Wind Data");
  display.setTextSize(2);
  display.setFont(windspeed);
  display.setCursor(0, 230);
  display.print(String(windSpd) + "km/h");
  display.setFont(f);
  
  if (windDir.length() == 1) {
    display.setCursor(75, 270);
    display.print(windDir);  
  }
  else if (windDir.length() == 2) {
    display.setCursor(65, 270);
    display.print(windDir);  
  }
  else {
    display.setCursor(45, 270);
    display.print(windDir);  
  }

  display.setTextSize(1);

  // Display temperature
  display.setCursor(230, 40);
  display.print("Temperature");
  display.setTextSize(2);
  if (temperature >= 10 || temperature < 0){
    display.setCursor(230, 90);
    display.println(temperature);
  }
  else {
    display.setCursor(250, 90);
    display.print(temperature);
  }
  display.setTextSize(1);

  // Display weather icon
  if (condition == "sunny") {
    display.drawBitmap(230, 110, sunny, 64, 64, GxEPD_BLACK);
  }
  else if (condition == "Cloudy" || condition == "Partly cloudy" || condition == "Mist") {
    display.drawBitmap(230, 110, cloudy, 64, 64, GxEPD_BLACK);
  }
  else if (condition == "Rain" || condition == "Rainy") {
    display.drawBitmap(230, 110, rainy, 64, 64, GxEPD_BLACK);
  }
  else if (condition == "Overcast") {
    display.drawBitmap(230, 110, overcast, 64, 64, GxEPD_BLACK);
  }

  // Display weather condition
  display.setCursor(240, 190);
  display.print("Condition");
  if (condition.length() > 6) {
    display.setTextSize(1);
    display.setCursor(250, 240);
    int spaceIndex = condition.indexOf(" ");
    if (spaceIndex != -1) {
      String part1 = condition.substring(0, spaceIndex);
      String part2 = condition.substring(spaceIndex + 1);

      display.setCursor(260, 220);
      display.print(part1);
      display.setCursor(260, 250);
      display.print(part2);
    }
    else {
      display.print(condition);
    }
  }
  else {
    display.setTextSize(2);
    display.setCursor(230, 240);
    display.print(condition);
  }
  display.setTextSize(1);

  // Display the latest update time 
  display.setCursor(220, 280);
  display.print("updated:" + last_updated_time);

}

void connectWiFi(){

  int32_t giveUpCounter = 0;
  display.setFont(f);
  display.setTextColor(GxEPD_BLACK);
  display.fillScreen(GxEPD_WHITE);
  display.setCursor(100, display.height() / 2);
  display.println("CONNECTING....");
  display.update();
  
  WiFi.begin(WIFI_NAME, PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    if (giveUpCounter >= 10) {
      display.fillScreen(GxEPD_WHITE);
      display.setCursor(50, display.height() / 2);
      display.println("CONNECTION FAILED");
      display.update();
      no_connection = 1;
      delay(1000);
      break;
    }
    delay(1000);
    giveUpCounter++;
  }

}

void WifiConnected(){
  display.fillScreen(GxEPD_WHITE);
  display.setCursor(20, 150);
  display.print("CONNECTED AS ");
  display.println(WiFi.localIP());
  display.update();
  delay(2000); 
}

char* generateUrl() {
  char* url = (char *)malloc(255);

  url[0] = '\0';

  strcat(url, BASE_URL);
  strcat(url, CURRENT_WEATHER);
  strcat(url, "?key=");
  strcat(url, API_KEY);
  strcat(url, "&q=");
  strcat(url, MY_CITY);

  return url;
}

void freeUrl(char* url) {
  free(url);
}
