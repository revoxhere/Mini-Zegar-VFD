// Mini zegar VFD na lampie IV-21 (IW-21) 2.2
// Edycja z auto zmiana czasu 04.2023
// 02.2023 Robert "revox" Piotrowski
#include <ESP32Encoder.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
ESP32Encoder encoder;
bool stoj = false;
#include <OneWire.h>
#include <DallasTemperature.h>

// GPIO where the DS18B20 is connected to
const int oneWireBus = 26;

// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(oneWireBus);

// Pass our oneWire reference to Dallas Temperature sensor
DallasTemperature sensor(&oneWire);


#pragma GCC optimize ("-Ofast")

const byte znaki[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };
const byte wysw[9] = {18, 19, 5, 21, 17, 22, 16, 23, 4};
//                     o   1   2   3   4   5   6  7  8
#include <Adafruit_PCF8574.h>
Adafruit_PCF8574 pcf;
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFi.h>
WiFiClientSecure client;
HTTPClient http;
const char* WEATHER_URL = "https://api.openweathermap.org/data/2.5/weather?lat=53.04&lon=18.6&appid=XXXXXXXXXXXX&units=metric";
float temperatura = -100.0;
float temperatura_odczuwalna = -100.0;
unsigned int cisnienie = -100;
unsigned int wilgotnosc = -100;
float temperatureC = 0;

const byte LEDY[5] = {18, 4, 16, 17, 5};
#include <WiFiManager.h>
WiFiManager wifiManager;

#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

#define LEAP_YEAR(Y)     ( (Y>0) && !(Y%4) && ( (Y%100) || !(Y%400) ) )
#include <NTPClient.h>
#include <WiFiUdp.h>
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
String timeStamp;

String wifiText = "internet";
bool czasUstawiony = false;
TaskHandle_t Task1;
TaskHandle_t Task2;
TaskHandle_t Task3;
TaskHandle_t Task4;

byte lastN = 0;
unsigned int li = 0;
unsigned int ustawienieJasnosci = 0;
byte aktualnyLed = 0;

unsigned long lastWeather = 1;
bool pogodaPobrana = false;
void setup() {
  Serial.begin(115200);
  Serial.println("Mini zegar VFD by revox, 02.2023 v2.2");

  sensor.begin();
  sensor.requestTemperatures();
  float temperatureC = sensor.getTempCByIndex(0);
  Serial.print(temperatureC);
  Serial.println("*C");

  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  Wire.begin(13, 14);

  pinMode(25, INPUT_PULLUP);
  pinMode(27, INPUT_PULLUP);

  encoder.attachHalfQuad(25, 27);
  encoder.clearCount();
  encoder.setFilter(1023);

  Wire.setClock(400000);
  pinMode(15, OUTPUT);
  pinMode(33, INPUT_PULLUP);

  digitalWrite(15, HIGH);

  if (!pcf.begin(0x38, &Wire)) {
    Serial.println("Nie znaleziono PCF8574");
    while (1);
  }
  for (uint8_t p = 0; p < 8; p++) {
    pcf.pinMode(znaki[p], OUTPUT);
  }

  for (uint8_t p = 0; p < 5; p++) {
    pinMode(LEDY[p], OUTPUT);
    digitalWrite(LEDY[p], LOW);
  }

  for (int i = 0; i < 9; i++) {
    pinMode(wysw[i], OUTPUT);
  }

  unsigned long animationStart = millis();
  while (millis() - animationStart < 1000) {
    displayText("czesc");
  }

  Serial.println("Lacznie z WiFi...");
  wifiManager.setConfigPortalBlocking(false);
  xTaskCreatePinnedToCore(wlaczManagera, "wlaczManagera", 20000, NULL, 1, &Task1, 0);

  animationStart = millis();
  aktualnyLed = 0;
  while (wifiManager.getWLStatusString() != "WL_CONNECTED") {
    displayText(wifiText);

    if (wifiText != "internet") {
      if (millis() - animationStart > 500) {
        animationStart = millis();
        if (aktualnyLed == 0) {
          aktualnyLed = 1;
        } else if (aktualnyLed == 1) {
          aktualnyLed = 0;
        }
      }
      if (aktualnyLed == 0) {
        zapalLed(0);
        zapalLed(1);
        zapalLed(2);
        zapalLed(3);
        zapalLed(4);
      }
    } else {
      zapalLed(aktualnyLed);
      if (millis() - animationStart > 500) {
        animationStart = millis();
        aktualnyLed++;
        if (aktualnyLed > 4) {
          aktualnyLed = 0;
        }
      }
    }
  }

  ArduinoOTA
  .onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  })
  .onEnd([]() {
    Serial.println("\nEnd");
  })
  .onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  })
  .onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });

  ArduinoOTA.setHostname("Mini zegar VFD v2.2");
  ArduinoOTA.begin();

  Serial.println("Pobieranie czasu...");
  animationStart = millis();
  aktualnyLed = 0;
  xTaskCreatePinnedToCore(ustawCzas, "ustawCzas", 10000, NULL, 1, &Task3, 0);
  while (!czasUstawiony) {
    displayText("czas");
    if (millis() - animationStart > 500) {
      animationStart = millis();
      if (aktualnyLed == 0) {
        aktualnyLed = 1;
      } else if (aktualnyLed == 1) {
        aktualnyLed = 0;
      }
    }
    if (aktualnyLed == 0) {
      zapalLed(0);
      zapalLed(4);
    }
  }

  Serial.println("Ustawianie pogody...");
  animationStart = millis();
  aktualnyLed = 0;
  xTaskCreatePinnedToCore(updateWeather, "updateWeather", 10000, NULL, 1, &Task4, 0);
  while (!pogodaPobrana) {
    displayText("pogoda");
    if (millis() - animationStart > 500) {
      animationStart = millis();
      if (aktualnyLed == 0) {
        aktualnyLed = 1;
      } else if (aktualnyLed == 1) {
        aktualnyLed = 0;
      }
    }
    if (aktualnyLed == 0) {
      zapalLed(1);
      zapalLed(3);
    }
  }
}

void ustawCzas(void * pvParameters ) {
  timeClient.begin();
  timeClient.forceUpdate();
  if (czasLetni()) {
    timeClient.setTimeOffset(7200);
  } else {
    timeClient.setTimeOffset(3600);
  }
  czasUstawiony = true;
  for (;;) {
    sensor.requestTemperatures();
    temperatureC = sensor.getTempCByIndex(0);
    delay(1000);
  }
}

void wlaczManagera( void * pvParameters ) {
  wifiManager.autoConnect("Mini zegar VFD v2.2");
  wifiText = "192.168.4.1";
  for (;;) {
    yield();
    wifiManager.process();
    delay(10);
  }
}

void updateWeather(void * pvParameters) {
  client.setInsecure();
  http.useHTTP10(true);

  for (;;) {
    Serial.println("Aktualizacja pogody");

    http.begin(client, WEATHER_URL);
    http.GET();

    StaticJsonDocument<2048> doc;
    DeserializationError error = deserializeJson(doc, http.getStream());
    if (error) {
      Serial.print("Blad przy deserializeJson(): ");
      Serial.println(error.c_str());
    } else {
      JsonObject main = doc["main"];
      temperatura = main["temp"];
      cisnienie = main["pressure"];
      Serial.println("Pogoda zaktualizowana");
      pogodaPobrana = true;
      delay(110 * 1000);
    }

    http.end();
    delay(10 * 1000);
  }
}

void display(String znak, int wyswietlacz) {
  for (int i = 0; i < 9; i++) {
    digitalWrite(wysw[i], HIGH);
  }

  for (int i = 0; i < 8; i++) {
    pcf.digitalWrite(znaki[i], HIGH);
  }
  zapalSegmenty(znak);
  if (znak != "") digitalWrite(wysw[wyswietlacz], LOW);
}

void zapalSegmenty(String tekst) {
  String znak = String(tekst.charAt(0));
  String kropka = String(tekst.charAt(1));

  if (znak == "0") {
    pcf.digitalWrite(znaki[1], LOW);
    pcf.digitalWrite(znaki[2], LOW);
    pcf.digitalWrite(znaki[3], LOW);
    pcf.digitalWrite(znaki[5], LOW);
    pcf.digitalWrite(znaki[6], LOW);
    pcf.digitalWrite(znaki[7], LOW);
  } else if (znak == "1") {
    pcf.digitalWrite(znaki[2], LOW);
    pcf.digitalWrite(znaki[5], LOW);
  }  else if (znak == "2") {
    pcf.digitalWrite(znaki[1], LOW);
    pcf.digitalWrite(znaki[3], LOW);
    pcf.digitalWrite(znaki[4], LOW);
    pcf.digitalWrite(znaki[5], LOW);
    pcf.digitalWrite(znaki[7], LOW);
  } else if (znak == "3") {
    pcf.digitalWrite(znaki[1], LOW);
    pcf.digitalWrite(znaki[2], LOW);
    pcf.digitalWrite(znaki[4], LOW);
    pcf.digitalWrite(znaki[5], LOW);
    pcf.digitalWrite(znaki[7], LOW);
  } else if (znak == "4") {
    pcf.digitalWrite(znaki[2], LOW);
    pcf.digitalWrite(znaki[4], LOW);
    pcf.digitalWrite(znaki[5], LOW);
    pcf.digitalWrite(znaki[6], LOW);
  } else if (znak == "5") {
    pcf.digitalWrite(znaki[1], LOW);
    pcf.digitalWrite(znaki[2], LOW);
    pcf.digitalWrite(znaki[4], LOW);
    pcf.digitalWrite(znaki[6], LOW);
    pcf.digitalWrite(znaki[7], LOW);
  } else if (znak == "6") {
    pcf.digitalWrite(znaki[1], LOW);
    pcf.digitalWrite(znaki[2], LOW);
    pcf.digitalWrite(znaki[3], LOW);
    pcf.digitalWrite(znaki[4], LOW);
    pcf.digitalWrite(znaki[6], LOW);
    pcf.digitalWrite(znaki[7], LOW);
  } else if (znak == "7") {
    pcf.digitalWrite(znaki[2], LOW);
    pcf.digitalWrite(znaki[5], LOW);
    //pcf.digitalWrite(znaki[6], LOW);
    pcf.digitalWrite(znaki[7], LOW);
  } else if (znak == "8") {
    pcf.digitalWrite(znaki[1], LOW);
    pcf.digitalWrite(znaki[2], LOW);
    pcf.digitalWrite(znaki[3], LOW);
    pcf.digitalWrite(znaki[4], LOW);
    pcf.digitalWrite(znaki[5], LOW);
    pcf.digitalWrite(znaki[6], LOW);
    pcf.digitalWrite(znaki[7], LOW);
  } else if (znak == "9") {
    pcf.digitalWrite(znaki[1], LOW);
    pcf.digitalWrite(znaki[2], LOW);
    pcf.digitalWrite(znaki[4], LOW);
    pcf.digitalWrite(znaki[5], LOW);
    pcf.digitalWrite(znaki[6], LOW);
    pcf.digitalWrite(znaki[7], LOW);
  } else if (znak == "-") {
    pcf.digitalWrite(znaki[4], LOW);
  } else if (znak == "*") {
    pcf.digitalWrite(znaki[4], LOW);
    pcf.digitalWrite(znaki[5], LOW);
    pcf.digitalWrite(znaki[6], LOW);
    pcf.digitalWrite(znaki[7], LOW);
  } else if (znak == "C" || znak == "c") {
    pcf.digitalWrite(znaki[1], LOW);
    pcf.digitalWrite(znaki[3], LOW);
    pcf.digitalWrite(znaki[6], LOW);
    pcf.digitalWrite(znaki[7], LOW);
  } else if (znak == "z") {
    pcf.digitalWrite(znaki[1], LOW);
    pcf.digitalWrite(znaki[3], LOW);
    pcf.digitalWrite(znaki[4], LOW);
    pcf.digitalWrite(znaki[5], LOW);
    pcf.digitalWrite(znaki[7], LOW);
  } else if (znak == "s") {
    pcf.digitalWrite(znaki[1], LOW);
    pcf.digitalWrite(znaki[2], LOW);
    pcf.digitalWrite(znaki[4], LOW);
    pcf.digitalWrite(znaki[6], LOW);
    pcf.digitalWrite(znaki[7], LOW);
  } else if (znak == "r") {
    pcf.digitalWrite(znaki[3], LOW);
    pcf.digitalWrite(znaki[4], LOW);
  } else if (znak == "h") {
    pcf.digitalWrite(znaki[2], LOW);
    pcf.digitalWrite(znaki[3], LOW);
    pcf.digitalWrite(znaki[4], LOW);
    pcf.digitalWrite(znaki[6], LOW);
  } else if (znak == "H") {
    pcf.digitalWrite(znaki[2], LOW);
    pcf.digitalWrite(znaki[3], LOW);
    pcf.digitalWrite(znaki[4], LOW);
    pcf.digitalWrite(znaki[5], LOW);
    pcf.digitalWrite(znaki[6], LOW);
  } else if (znak == "P") {
    pcf.digitalWrite(znaki[3], LOW);
    pcf.digitalWrite(znaki[4], LOW);
    pcf.digitalWrite(znaki[5], LOW);
    pcf.digitalWrite(znaki[6], LOW);
    pcf.digitalWrite(znaki[7], LOW);
  } else if (znak == "a") {
    pcf.digitalWrite(znaki[2], LOW);
    pcf.digitalWrite(znaki[3], LOW);
    pcf.digitalWrite(znaki[4], LOW);
    pcf.digitalWrite(znaki[5], LOW);
    pcf.digitalWrite(znaki[6], LOW);
    pcf.digitalWrite(znaki[7], LOW);
  } else if (znak == "@") {
    pcf.digitalWrite(znaki[6], LOW);
  } else if (znak == "n") {
    pcf.digitalWrite(znaki[2], LOW);
    pcf.digitalWrite(znaki[3], LOW);
    pcf.digitalWrite(znaki[4], LOW);
  } else if (znak == "e") {
    pcf.digitalWrite(znaki[1], LOW);
    pcf.digitalWrite(znaki[3], LOW);
    pcf.digitalWrite(znaki[4], LOW);
    pcf.digitalWrite(znaki[6], LOW);
    pcf.digitalWrite(znaki[7], LOW);
  } else if (znak == "t") {
    pcf.digitalWrite(znaki[1], LOW);
    pcf.digitalWrite(znaki[3], LOW);
    pcf.digitalWrite(znaki[4], LOW);
    pcf.digitalWrite(znaki[6], LOW);
  } else if (znak == "j") {
    pcf.digitalWrite(znaki[1], LOW);
    pcf.digitalWrite(znaki[2], LOW);
    pcf.digitalWrite(znaki[7], LOW);
  } else if (znak == "v") {
    pcf.digitalWrite(znaki[1], LOW);
    pcf.digitalWrite(znaki[2], LOW);
    pcf.digitalWrite(znaki[3], LOW);
  } else if (znak == "f") {
    pcf.digitalWrite(znaki[3], LOW);
    pcf.digitalWrite(znaki[4], LOW);
    pcf.digitalWrite(znaki[6], LOW);
    pcf.digitalWrite(znaki[7], LOW);
  } else if (znak == "d") {
    pcf.digitalWrite(znaki[1], LOW);
    pcf.digitalWrite(znaki[2], LOW);
    pcf.digitalWrite(znaki[3], LOW);
    pcf.digitalWrite(znaki[4], LOW);
    pcf.digitalWrite(znaki[5], LOW);
  } else if (znak == "p") {
    pcf.digitalWrite(znaki[7], LOW);
    pcf.digitalWrite(znaki[6], LOW);
    pcf.digitalWrite(znaki[3], LOW);
    pcf.digitalWrite(znaki[4], LOW);
    pcf.digitalWrite(znaki[5], LOW);
  } else if (znak == "o") {
    pcf.digitalWrite(znaki[1], LOW);
    pcf.digitalWrite(znaki[2], LOW);
    pcf.digitalWrite(znaki[3], LOW);
    pcf.digitalWrite(znaki[4], LOW);
  } else if (znak == "g") {
    pcf.digitalWrite(znaki[1], LOW);
    pcf.digitalWrite(znaki[2], LOW);
    pcf.digitalWrite(znaki[3], LOW);
    pcf.digitalWrite(znaki[6], LOW);
    pcf.digitalWrite(znaki[7], LOW);
  } else if (znak == "i") {
    pcf.digitalWrite(znaki[2], LOW);
  } else if (znak == ".") {
    pcf.digitalWrite(znaki[0], LOW);
  }

  if (kropka == ".") {
    pcf.digitalWrite(znaki[0], LOW);
  }
}


String getFormattedDate() {
  unsigned long rawTime = timeClient.getEpochTime() / 86400L;
  unsigned long days = 0, year = 1970;
  uint8_t month;
  static const uint8_t monthDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

  while ((days += (LEAP_YEAR(year) ? 366 : 365)) <= rawTime)
    year++;
  rawTime -= days - (LEAP_YEAR(year) ? 366 : 365);
  days = 0;
  for (month = 0; month < 12; month++) {
    uint8_t monthLength;
    if (month == 1) { // february
      monthLength = LEAP_YEAR(year) ? 29 : 28;
    } else {
      monthLength = monthDays[month];
    }
    if (rawTime < monthLength) break;
    rawTime -= monthLength;
  }
  String monthStr = ++month < 10 ? "0" + String(month) : String(month);
  String dayStr = ++rawTime < 10 ? "0" + String(rawTime) : String(rawTime);

  return dayStr + "." + monthStr + "." + String(year);
}


void displayText(String text) {
  byte dlugosc = text.length();
  byte z = 0;

  if (dlugosc < 9) {
    z = (9 - dlugosc) / 2;
  }

  for (byte i = 0; i < dlugosc; i++) {
    String aktualnyZnak = String(text.charAt(i));
    String nastepnyZnak = String(text.charAt(i + 1));

    if (nastepnyZnak == ".") {
      z++;
      display(aktualnyZnak + ".", z);
    } else if (aktualnyZnak != ".") {
      z++;
      display(aktualnyZnak, z);
    }
    if (aktualnyZnak != " " && aktualnyZnak != "") delayMicroseconds(600);
  }
}


static unsigned short days[4][12] = {
  {   0,  31,  60,  91, 121, 152, 182, 213, 244, 274, 305, 335},
  { 366, 397, 425, 456, 486, 517, 547, 578, 609, 639, 670, 700},
  { 731, 762, 790, 821, 851, 882, 912, 943, 974, 1004, 1035, 1065},
  {1096, 1127, 1155, 1186, 1216, 1247, 1277, 1308, 1339, 1369, 1400, 1430},
};

bool czasLetni() {
  long epoch = timeClient.getEpochTime();
  unsigned int years = epoch / (365 * 4 + 1) * 4; epoch %= 365 * 4 + 1;
  unsigned int year;
  for (year = 3; year > 0; year--) {
    if (epoch >= days[year][0])
      break;
  }

  unsigned int month;
  for (month = 11; month > 0; month--) {
    if (epoch >= days[year][month])
      break;
  }

  year  = years + year;
  month = month + 1;
  unsigned int day = epoch - days[year][month] + 1;

  if (month == 3 && day >= 26) return true;
  if (month == 10 && day <= 29) return true;
  if (month > 3 && month < 10) return true;
  return false;
}

void zapalLed(int n) {
  for (int i = 0; i < 9; i++) {
    digitalWrite(wysw[i], HIGH);
  }

  for (int i = 0; i < 8; i++) {
    pcf.digitalWrite(znaki[i], HIGH);
  }

  for (int i = 0; i < 5; i++) {
    digitalWrite(LEDY[i], LOW);
  }

  timeStamp = timeClient.getFormattedTime();
  String hours = timeStamp.substring(0, 2);

  digitalWrite(15, LOW);
  digitalWrite(LEDY[n], HIGH);
  if ((hours.toInt() >= 7) && (hours.toInt() < 22)) {
    delayMicroseconds(300);
  } else {
    delayMicroseconds(10);
  }
  digitalWrite(15, HIGH);
}
long lastEncoder = 0;
long newPosition = 0;
long oldPosition = 0;
bool firstRun = true;
void loop() {

  if (lastWeather == 0) lastWeather = millis();
  if (lastEncoder == 0) lastEncoder = millis();

  newPosition = (encoder.getCount() / 2);
  if (newPosition > 4) {
    newPosition = 4;
    encoder.setCount(8);
  }
  if (newPosition < 0) {
    newPosition = 0;
    encoder.setCount(0);
  }

  if (newPosition != oldPosition) {
    oldPosition = newPosition;
    lastEncoder = millis();
    lastWeather =  millis() + 3000;
    lastN = newPosition;
  }

  if (((millis() - lastWeather < 5000) && !stoj) || (newPosition == 0 && (millis() - lastEncoder < 5000)) || (newPosition == 0 && stoj)) {
    timeStamp = timeClient.getFormattedTime();
    String hours = timeStamp.substring(0, 2);
    String minutes = timeStamp.substring(3, 5);
    String seconds = timeStamp.substring(6, 9);

    if (seconds.toInt() % 2 == 0) {
      display("@", 0);
      delayMicroseconds(400);
    } else {
      delayMicroseconds(400);
    }

    display(String( (hours.toInt() / 10) % 10 ) , 1);
    delayMicroseconds(800);
    display(String( hours.toInt() % 10 ) , 2);
    delayMicroseconds(600);

    display("-", 3);
    delayMicroseconds(100);

    display(String( (minutes.toInt() / 10) % 10 ) , 4);
    delayMicroseconds(400);
    display(String( minutes.toInt() % 10 ) , 5);
    delayMicroseconds(400);

    display("-", 6);
    delayMicroseconds(100);

    display(String( (seconds.toInt() / 10) % 10 ) , 7);
    delayMicroseconds(400);
    display(String( seconds.toInt() % 10 ) , 8);
    delayMicroseconds(600);

    zapalLed(0);
  } else {
    if (lastN == 0 || (newPosition == 1 && millis() - lastEncoder < 5000)  || (newPosition == 1 && stoj ) ) {
      li += 1;
      if (li >= 300 && !stoj) {
        lastWeather = millis();
        li = 0;
        lastN = 1;
      } else {
        displayText(String(temperatura) + "*C");
        zapalLed(1);
      }
    } else if (lastN == 1 || (newPosition == 2 && millis() - lastEncoder < 5000) || (newPosition == 2 && stoj )  ) {
      li += 1;
      if (li >= 300 && !stoj) {
        lastWeather = millis();
        li = 0;
        lastN = 2;
      } else {
        displayText(String(temperatureC) + "*C");
        zapalLed(2);
      }
    } else if (lastN == 2 || (newPosition == 3 && millis() - lastEncoder < 5000) || (newPosition == 3 && stoj )  ) {
      li += 1;
      if (li >= 300 && !stoj) {
        lastWeather = millis();
        li = 0;
        lastN = 3;
      } else {
        displayText(String(cisnienie) + " hPa");
        zapalLed(3);
      }
    } else if (lastN == 3 || (newPosition == 4 && millis() - lastEncoder < 5000) || (newPosition == 4 && stoj )  ) {
      li += 1;
      if (li >= 300 && !stoj) {
        lastWeather = millis();
        li = 0;
        lastN = 0;
      } else {
        displayText(getFormattedDate());
        zapalLed(4);
      }
    }
  }

  if (stoj) {
    display("@", 0);
  }

  if (!digitalRead(33)) {
    Serial.println("Klik!");
    stoj = !stoj;
    while (!digitalRead(33)) {
      display("@", 0);
    }
  }

  ArduinoOTA.handle();
}
