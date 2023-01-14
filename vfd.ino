#pragma GCC optimize ("-Ofast")

const byte znaki[8] = { 4, 16, 17, 5, 18, 23, 19, 22 };
const byte wysw[9] = { 14, 27, 32, 26, 12, 25, 13, 33, 15 };
//                     o   1   2   3   4   5   6   7   8

#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFi.h>
const char* WEATHER_URL = "https://api.openweathermap.org/data/2.5/weather?lat=51.75&lon=19.4375&appid=55ecc8f7be8f6c892a595bbe202d7f2f&units=metric";
float temperatura = -100.0;
float temperatura_odczuwalna = -100.0;
unsigned int cisnienie = -100;
unsigned int wilgotnosc = -100;

#include <WiFiManager.h>
WiFiManager wifiManager;

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

#define REPEAT 30
#define DELAYTOUCHREPEAT 5
unsigned int touchThreshold = 35;

byte lastN = 0;
unsigned int li = 0;
unsigned int ustawienieJasnosci = 0;

unsigned long lastWeather = 1;
unsigned long lastCalibration =  1;
unsigned long lastTouch = 1;

void setup() {
  Serial.begin(115200);
  Serial.println("Mini zegar VFD by revox, 01.2023");

  for (int i = 0; i < 8; i++) {
    pinMode(znaki[i], OUTPUT);
  }

  for (int i = 0; i < 9; i++) {
    pinMode(wysw[i], OUTPUT);
  }

  // led
  pinMode(0, OUTPUT);
  digitalWrite(0, HIGH);

  unsigned long animationStart = millis();
  while (millis() - animationStart < 1000) {
    displayText("hej. vfd");
  }

  Serial.print("Lacznie z WiFi...");
  wifiManager.setConnectTimeout(20);
  wifiManager.setConfigPortalBlocking(false);
  xTaskCreatePinnedToCore(wlaczManagera, "wlaczManagera", 10000, NULL, 1, &Task1, 0);

  while (wifiManager.getWLStatusString() != "WL_CONNECTED") {
    displayText(wifiText);
    if (wifiText != "internet") {
      zapalLed(1, 0);
      delayMicroseconds(600);
    }
  }

  Serial.print("Pobieranie czasu...");
  xTaskCreatePinnedToCore(ustawCzas, "ustawCzas", 10000, NULL, 1, &Task3, 0);
  while (!czasUstawiony) {
    displayText("czas");
  }

  xTaskCreatePinnedToCore(updateWeather, "updateWeather", 10000, NULL, 1, &Task4, 0);

  Serial.print("Kalibracja...");
  calibrateTouch();
  xTaskCreatePinnedToCore(czytajTouch, "czytajTouch", 10000, NULL, 0, &Task2, 0);

}

void ustawCzas(void * pvParameters ) {
  timeClient.begin();
  timeClient.setTimeOffset(3600);
  timeClient.forceUpdate();
  czasUstawiony = true;
  for (;;) {
    delay(10);
  }
}

void wlaczManagera( void * pvParameters ) {
  wifiManager.autoConnect("Mini zegar VFD");
  wifiText = "192.168.1.4";
  for (;;) {
    wifiManager.process();
    delay(10);
  }
}


WiFiClientSecure client;
HTTPClient http;

void updateWeather(void * pvParameters) {
  for (;;) {
    Serial.println("Aktualizacja pogody");

    client.setInsecure();
    http.useHTTP10(true);
    http.begin(client, WEATHER_URL);
    http.GET();

    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, http.getStream());
    if (error) {
      Serial.print("deserializeJson() failed: ");
      Serial.println(error.c_str());
      return;
    }

    JsonObject main = doc["main"];
    temperatura = main["temp"];
    temperatura_odczuwalna = main["feels_like"];
    cisnienie = main["pressure"];
    wilgotnosc = main["humidity"];
    Serial.println("Pogoda zaktualizowana");

    delay(1000 * 120);
  }
}

void display(String znak, int wyswietlacz) {
  // zgas led
  digitalWrite(0, HIGH);

  for (int i = 0; i < 9; i++) {
    digitalWrite(wysw[i], HIGH);
  }

  for (int i = 0; i < 8; i++) {
    digitalWrite(znaki[i], HIGH);
  }
  zapalSegmenty(znak);
  if (znak != "") digitalWrite(wysw[wyswietlacz], LOW);
}

void zapalSegmenty(String tekst) {
  String znak = String(tekst.charAt(0));
  String kropka = String(tekst.charAt(1));

  if (znak == "0") {
    digitalWrite(znaki[1], LOW);
    digitalWrite(znaki[2], LOW);
    digitalWrite(znaki[3], LOW);
    digitalWrite(znaki[5], LOW);
    digitalWrite(znaki[6], LOW);
    digitalWrite(znaki[7], LOW);
  } else if (znak == "1") {
    digitalWrite(znaki[2], LOW);
    digitalWrite(znaki[5], LOW);
  }  else if (znak == "2") {
    digitalWrite(znaki[1], LOW);
    digitalWrite(znaki[3], LOW);
    digitalWrite(znaki[4], LOW);
    digitalWrite(znaki[5], LOW);
    digitalWrite(znaki[7], LOW);
  } else if (znak == "3") {
    digitalWrite(znaki[1], LOW);
    digitalWrite(znaki[2], LOW);
    digitalWrite(znaki[4], LOW);
    digitalWrite(znaki[5], LOW);
    digitalWrite(znaki[7], LOW);
  } else if (znak == "4") {
    digitalWrite(znaki[2], LOW);
    digitalWrite(znaki[4], LOW);
    digitalWrite(znaki[5], LOW);
    digitalWrite(znaki[6], LOW);
  } else if (znak == "5") {
    digitalWrite(znaki[1], LOW);
    digitalWrite(znaki[2], LOW);
    digitalWrite(znaki[4], LOW);
    digitalWrite(znaki[6], LOW);
    digitalWrite(znaki[7], LOW);
  } else if (znak == "6") {
    digitalWrite(znaki[1], LOW);
    digitalWrite(znaki[2], LOW);
    digitalWrite(znaki[3], LOW);
    digitalWrite(znaki[4], LOW);
    digitalWrite(znaki[6], LOW);
    digitalWrite(znaki[7], LOW);
  } else if (znak == "7") {
    digitalWrite(znaki[2], LOW);
    digitalWrite(znaki[5], LOW);
    digitalWrite(znaki[6], LOW);
    digitalWrite(znaki[7], LOW);
  } else if (znak == "8") {
    digitalWrite(znaki[1], LOW);
    digitalWrite(znaki[2], LOW);
    digitalWrite(znaki[3], LOW);
    digitalWrite(znaki[4], LOW);
    digitalWrite(znaki[5], LOW);
    digitalWrite(znaki[6], LOW);
    digitalWrite(znaki[7], LOW);
  } else if (znak == "9") {
    digitalWrite(znaki[1], LOW);
    digitalWrite(znaki[2], LOW);
    digitalWrite(znaki[4], LOW);
    digitalWrite(znaki[5], LOW);
    digitalWrite(znaki[6], LOW);
    digitalWrite(znaki[7], LOW);
  } else if (znak == "-") {
    digitalWrite(znaki[4], LOW);
  } else if (znak == "*") {
    digitalWrite(znaki[4], LOW);
    digitalWrite(znaki[5], LOW);
    digitalWrite(znaki[6], LOW);
    digitalWrite(znaki[7], LOW);
  } else if (znak == "C" || znak == "c") {
    digitalWrite(znaki[1], LOW);
    digitalWrite(znaki[3], LOW);
    digitalWrite(znaki[6], LOW);
    digitalWrite(znaki[7], LOW);
  } else if (znak == "z") {
    digitalWrite(znaki[1], LOW);
    digitalWrite(znaki[3], LOW);
    digitalWrite(znaki[4], LOW);
    digitalWrite(znaki[5], LOW);
    digitalWrite(znaki[7], LOW);
  } else if (znak == "s") {
    digitalWrite(znaki[1], LOW);
    digitalWrite(znaki[2], LOW);
    digitalWrite(znaki[4], LOW);
    digitalWrite(znaki[6], LOW);
    digitalWrite(znaki[7], LOW);
  } else if (znak == "r") {
    digitalWrite(znaki[3], LOW);
    digitalWrite(znaki[4], LOW);
  } else if (znak == "h") {
    digitalWrite(znaki[2], LOW);
    digitalWrite(znaki[3], LOW);
    digitalWrite(znaki[4], LOW);
    digitalWrite(znaki[6], LOW);
  } else if (znak == "H") {
    digitalWrite(znaki[2], LOW);
    digitalWrite(znaki[3], LOW);
    digitalWrite(znaki[4], LOW);
    digitalWrite(znaki[5], LOW);
    digitalWrite(znaki[6], LOW);
  } else if (znak == "P") {
    digitalWrite(znaki[3], LOW);
    digitalWrite(znaki[4], LOW);
    digitalWrite(znaki[5], LOW);
    digitalWrite(znaki[6], LOW);
    digitalWrite(znaki[7], LOW);
  } else if (znak == "a") {
    digitalWrite(znaki[2], LOW);
    digitalWrite(znaki[3], LOW);
    digitalWrite(znaki[4], LOW);
    digitalWrite(znaki[5], LOW);
    digitalWrite(znaki[6], LOW);
    digitalWrite(znaki[7], LOW);
  } else if (znak == "@") {
    digitalWrite(znaki[6], LOW);
  } else if (znak == "n") {
    digitalWrite(znaki[2], LOW);
    digitalWrite(znaki[3], LOW);
    digitalWrite(znaki[4], LOW);
  } else if (znak == "e") {
    digitalWrite(znaki[1], LOW);
    digitalWrite(znaki[3], LOW);
    digitalWrite(znaki[4], LOW);
    digitalWrite(znaki[6], LOW);
    digitalWrite(znaki[7], LOW);
  } else if (znak == "t") {
    digitalWrite(znaki[0], LOW);
    digitalWrite(znaki[1], LOW);
    digitalWrite(znaki[3], LOW);
    digitalWrite(znaki[4], LOW);
    digitalWrite(znaki[6], LOW);
  } else if (znak == "j") {
    digitalWrite(znaki[1], LOW);
    digitalWrite(znaki[2], LOW);
    digitalWrite(znaki[7], LOW);
  } else if (znak == "v") {
    digitalWrite(znaki[1], LOW);
    digitalWrite(znaki[2], LOW);
    digitalWrite(znaki[3], LOW);
  } else if (znak == "f") {
    digitalWrite(znaki[3], LOW);
    digitalWrite(znaki[4], LOW);
    digitalWrite(znaki[6], LOW);
    digitalWrite(znaki[7], LOW);
  } else if (znak == "d") {
    digitalWrite(znaki[1], LOW);
    digitalWrite(znaki[2], LOW);
    digitalWrite(znaki[3], LOW);
    digitalWrite(znaki[4], LOW);
    digitalWrite(znaki[5], LOW);
  } else if (znak == "i") {
    digitalWrite(znaki[2], LOW);
  } else if (znak == ".") {
    digitalWrite(znaki[0], LOW);
  } else {}

  if (kropka == ".") {
    digitalWrite(znaki[0], LOW);
  }
}

void zapalLed(int  r, int g) {
  digitalWrite(0, HIGH);
  for (int i = 0; i < 9; i++) {
    digitalWrite(wysw[i], HIGH);
  }
  for (int i = 0; i < 8; i++) {
    digitalWrite(znaki[i], HIGH);
  }
  digitalWrite(0, LOW);
  digitalWrite(17, r);
  digitalWrite(16, g);
}

void calibrateTouch() {
  int  touch = 0, totalValue = 0;
  for (int counter = 0; counter < REPEAT ; counter++) {
    delay(DELAYTOUCHREPEAT);
    touch = touchRead(2);
    while (touch < 20) {
      touch = touchRead(2);
    }
    totalValue += touch;
  }
  touchThreshold = (totalValue / REPEAT) - 5;
}

int touchReading(int pin) {
  int  touch = 0, totalValue = 0;
  for (int counter = 0; counter < REPEAT; counter++) {
    delay(DELAYTOUCHREPEAT);
    touch = touchRead(pin);
    while (touch < 20) {
      touch = touchRead(pin);
    }
    totalValue += touch;
  }
  Serial.print(totalValue / REPEAT);
  Serial.print(",");
  Serial.println(touchThreshold);
  return totalValue / REPEAT;
}

void czytajTouch( void * pvParameters ) {
  for (;;) {
    Serial.println("does it even run?");
    timeClient.update();
    if (millis() - lastCalibration > 10000 && millis() - lastTouch > 5000) {
      Serial.println("Kalibracja rutynowa");
      lastCalibration = millis();
      calibrateTouch();
    }
    if (touchReading(2) < touchThreshold) {
      Serial.println("Wykryty dotyk");
      lastTouch = millis();

      if (ustawienieJasnosci == 0) {
        ustawienieJasnosci = 10;
      } else if (ustawienieJasnosci == 10) {
        ustawienieJasnosci = 28;
      } else if (ustawienieJasnosci == 28) {
        ustawienieJasnosci = 0;
      }

      unsigned int i = 0;
      while (i < 100) {
        zapalLed(1, 0);
        Serial.println("Led...");
        delay(1);
        i++;
      }

      while (touchReading(2) < touchThreshold) {
        Serial.println("While...");
      }
      delay(50);
    }
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
    delayMicroseconds(600);
  }
}

bool firstRun = true;
void loop() {
  if (ustawienieJasnosci != 28) {
    if (millis() - lastWeather < 8000) {
      timeStamp = timeClient.getFormattedTime();
      String hours = timeStamp.substring(0, 2);
      String minutes = timeStamp.substring(3, 5);
      String seconds = timeStamp.substring(6, 9);

      if (seconds.toInt() % 2 == 0) {
        display("@", 0);
        delayMicroseconds(200);
      } else {
        delayMicroseconds(200);
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
    } else {
      if (lastN == 0) {
        li += 1;
        if (temperatura == -100.0 || li == 800) {
          lastWeather = millis();
          li = 0;
          lastN = 1;
        } else {
          displayText(String(temperatura) + "*C");

          //zapalLed(0, 1);
          //delayMicroseconds(800);
        }

      } else if (lastN == 1) {
        li += 1;
        if (wilgotnosc == -100 || li == 800) {
          lastWeather = millis();
          li = 0;
          lastN = 2;
        } else {
          displayText(String(wilgotnosc) + " rH");

          //zapalLed(1, 0);
          //delayMicroseconds(400);
        }
      } else if (lastN == 2) {
        li += 1;
        if (cisnienie == -100 || li == 800) {
          lastWeather = millis();
          li = 0;
          lastN = 3;
        } else {
          displayText(String(cisnienie) + " hPa");

          //zapalLed(0, 1);
          //delayMicroseconds(800);
        }
      } else if (lastN == 3) {
        li += 1;
        if (li == 800) {
          lastWeather = millis();
          li = 0;
          lastN = 0;
        }
        displayText(getFormattedDate());
      }
    }
    display("", 0);
  } else {
    display("@", 0);
  }

  delay(ustawienieJasnosci);
}
