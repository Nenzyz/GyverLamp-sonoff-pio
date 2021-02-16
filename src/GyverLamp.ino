/*
  Скетч к проекту "Многофункциональный RGB светильник"
  Страница проекта (схемы, описания): https://alexgyver.ru/GyverLamp/
  Исходники на GitHub: https://github.com/AlexGyver/GyverLamp/
  Нравится, как написан код? Поддержи автора! https://alexgyver.ru/support_alex/
  Автор: AlexGyver, AlexGyver Technologies, 2019
  https://AlexGyver.ru/
*/

/*
  Версия 1.4:
  - Исправлен баг при смене режимов
  - Исправлены тормоза в режиме точки доступа
*/

// Ссылка для менеджера плат:
// http://arduino.esp8266.com/stable/package_esp8266com_index.json

// ============= НАСТРОЙКИ =============

// -------- ВРЕМЯ -------
#define GMT 2              // смещение (москва 3)
#define NTP_ADDRESS  "europe.pool.ntp.org"    // сервер времени

// -------- РАССВЕТ -------
#define DAWN_BRIGHT 200       // макс. яркость рассвета
#define DAWN_TIMEOUT 1        // сколько рассвет светит после времени будильника, минут

// ---------- МАТРИЦА ---------
#define BRIGHTNESS 40         // стандартная маскимальная яркость (0-255)
#define CURRENT_LIMIT 1000    // лимит по току в миллиамперах, автоматически управляет яркостью (пожалей свой блок питания!) 0 - выключить лимит

#define WIDTH 16              // ширина матрицы
#define HEIGHT 16             // высота матрицы

#define COLOR_ORDER GRB       // порядок цветов на ленте. Если цвет отображается некорректно - меняйте. Начать можно с RGB

#define MATRIX_TYPE 0         // тип матрицы: 0 - зигзаг, 1 - параллельная
#define CONNECTION_ANGLE 0    // угол подключения: 0 - левый нижний, 1 - левый верхний, 2 - правый верхний, 3 - правый нижний
#define STRIP_DIRECTION 0     // направление ленты из угла: 0 - вправо, 1 - вверх, 2 - влево, 3 - вниз
// при неправильной настройке матрицы вы получите предупреждение "Wrong matrix parameters! Set to default"
// шпаргалка по настройке матрицы здесь! https://alexgyver.ru/matrix_guide/

// ============= ДЛЯ РАЗРАБОТЧИКОВ =============

#if defined(SONOFF)
#define LED_PIN 14             // пин ленты
#else
#define LED_PIN 13             // пин ленты
#endif
#define BTN_PIN T3 // 15
#define LED_ENABLE_PIN GPIO_NUM_2
#define MODE_AMOUNT 18

#define NUM_LEDS WIDTH * HEIGHT
#define SEGMENTS 1            // диодов в одном "пикселе" (для создания матрицы из кусков ленты)
// ---------------- БИБЛИОТЕКИ -----------------
// #define FASTLED_INTERRUPT_RETRY_COUNT 0
// #define FASTLED_ALLOW_INTERRUPTS 0
#if defined(ESP8266)
#define FASTLED_ESP8266_RAW_PIN_ORDER
#endif
#define NTP_INTERVAL 60 * 1000    // обновление (1 минута)

#include "timerMinim.h"
#include <FastLED.h>
#if defined(ESP8266)
#include <ESP8266WiFi.h>
#else
#include <WiFi.h>
#endif
#include <DNSServer.h>
#include <WiFiManager.h>
#include <WiFiUdp.h>
#include <EEPROM.h>
#include <NTPClient.h>
#include <GyverButton.h>

#include <ArduinoOTA.h>

#include "esp_deep_sleep.h"

#ifndef UDP_TX_PACKET_MAX_SIZE
#define UDP_TX_PACKET_MAX_SIZE 15
#endif

#define GENERAL_DEBUG 1
#if defined(GENERAL_DEBUG) && GENERAL_DEBUG_TELNET
WiFiServer telnetServer(TELNET_PORT);                       // telnet сервер
WiFiClient telnet;                                          // обработчик событий telnet клиента
bool telnetGreetingShown = false;                           // признак "показано приветствие в telnet"
#define LOG                   telnet
#else
#define LOG                   Serial
#endif

// ------------------- ТИПЫ --------------------
CRGB leds[NUM_LEDS];
WiFiManager wifiManager;
WiFiUDP Udp;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTP_ADDRESS, GMT * 3600, NTP_INTERVAL);
timerMinim timeTimer(3000);
GButton touch(BTN_PIN, HIGH_PULL, NORM_OPEN);

// ----------------- ПЕРЕМЕННЫЕ ------------------
char packetBuffer[UDP_TX_PACKET_MAX_SIZE + 1]; //buffer to hold incoming packet
String inputBuffer;
static const byte maxDim = max(WIDTH, HEIGHT);
struct {
  byte brightness = 50;
  byte speed = 30;
  byte scale = 40;
} modes[MODE_AMOUNT];

struct {
  boolean state = false;
  int time = 0;
} my_alarm[7];

byte dawnOffsets[] = {5, 10, 15, 20, 25, 30, 40, 50, 60};
byte dawnMode;
boolean dawnFlag = false;
long thisTime;
boolean manualOff = false;

int8_t currentMode = 0;
boolean loadingFlag = true;
boolean ONflag = true;
uint32_t eepromTimer;
boolean settChanged = false;
// Конфетти, Огонь, Радуга верт., Радуга гориз., Смена цвета,
// Безумие 3D, Облака 3D, Лава 3D, Плазма 3D, Радуга 3D,
// Павлин 3D, Зебра 3D, Лес 3D, Океан 3D,

unsigned char matrixValue[8][16];

bool wifiConnected = false;

void print_wakeup_reason(){
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch(wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_EXT0 : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1 : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER : Serial.println("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP : Serial.println("Wakeup caused by ULP program"); break;
    default : Serial.printf("Wakeup was not caused by deep sleep: %d\n",wakeup_reason); break;
  }
}

void wake_callback(){
  //placeholder callback function
}

void setup() {
  // ESP.wdtDisable();
  // ESP.wdtEnable(0);
  // ЛЕНТА
  FastLED.addLeds<WS2812B, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS)/*.setCorrection( TypicalLEDStrip )*/;
  FastLED.setBrightness(BRIGHTNESS);
  if (CURRENT_LIMIT > 0) FastLED.setMaxPowerInVoltsAndMilliamps(5, CURRENT_LIMIT);
  FastLED.clear();
  FastLED.show();

  touch.setStepTimeout(100);
  touch.setClickTimeout(500);

  Serial.begin(115200);
  Serial.println();

  wifiManager.setConfigPortalBlocking(false);
  if (wifiManager.autoConnect("Fire Lamp", "")) {
    Serial.println("connected...yeey :)");
    wifiConnected = true;
  } else {
    Serial.println("non blocking config portal running");
    wifiManager.startConfigPortal("Fire Lamp AP", "ondemand");
  }

  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  Serial.print("Wake up reason: ");
  Serial.println(wakeup_reason);

  Serial.println("local ip");
  Serial.println(WiFi.localIP());

  Serial.println("UDP server on port 8888");
  Udp.begin(8888);

  // EEPROM
  EEPROM.begin(202);
  delay(50);
  if (EEPROM.read(198) != 20) {   // первый запуск
    EEPROM.write(198, 20);
    EEPROM.commit();

    for (byte i = 0; i < MODE_AMOUNT; i++) {
      EEPROM.put(3 * i + 40, modes[i]);
      EEPROM.commit();
    }
    for (byte i = 0; i < 7; i++) {
      EEPROM.write(5 * i, my_alarm[i].state);   // рассвет
      eeWriteInt(5 * i + 1, my_alarm[i].time);
      EEPROM.commit();
    }
    EEPROM.write(199, 0);   // рассвет
    EEPROM.write(200, 0);   // режим
    EEPROM.commit();
  }
  for (byte i = 0; i < MODE_AMOUNT; i++) {
    EEPROM.get(3 * i + 40, modes[i]);
  }
  for (byte i = 0; i < 7; i++) {
    my_alarm[i].state = EEPROM.read(5 * i);
    my_alarm[i].time = eeGetInt(5 * i + 1);
  }
  dawnMode = EEPROM.read(199);
  currentMode = (int8_t)EEPROM.read(200);

  // отправляем настройки
  sendCurrent();
  char reply[inputBuffer.length() + 1];
  inputBuffer.toCharArray(reply, inputBuffer.length() + 1);
  Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
#if defined(ESP8266)
  Udp.write(reply);
#else
  Udp.print(reply);
#endif
  Udp.endPacket();

  timeClient.begin();
  memset(matrixValue, 0, sizeof(matrixValue));

  pinMode(LED_ENABLE_PIN, OUTPUT);
  digitalWrite(LED_ENABLE_PIN, HIGH);
  // esp_sleep_enable_touchpad_wakeup();
  gpio_pullup_en(GPIO_NUM_15);
  gpio_pulldown_dis(GPIO_NUM_15);
  // esp_deep_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_AUTO); 
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_15, 0);

  randomSeed(micros());
}

void loop() {
  if (wifiConnected) {    
    // server.handleClient();
  } else {
    wifiManager.process();
  }
  parseUDP();
  effectsTick();
  eepromTick();
  timeTick();
  buttonTick();
  // ESP.wdtFeed();   // пнуть собаку
  yield();
}

void eeWriteInt(int pos, int val) {
  byte* p = (byte*) &val;
  EEPROM.write(pos, *p);
  EEPROM.write(pos + 1, *(p + 1));
  EEPROM.write(pos + 2, *(p + 2));
  EEPROM.write(pos + 3, *(p + 3));
  EEPROM.commit();
}

int eeGetInt(int pos) {
  int val;
  byte* p = (byte*) &val;
  *p        = EEPROM.read(pos);
  *(p + 1)  = EEPROM.read(pos + 1);
  *(p + 2)  = EEPROM.read(pos + 2);
  *(p + 3)  = EEPROM.read(pos + 3);
  return val;
}
