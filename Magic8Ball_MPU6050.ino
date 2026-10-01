#define NUM_LEDS 12
#define LED_PIN 6
#define BUTTON_PIN 3
#define MPU_ADDR_LOW 0x68
#define MPU_ADDR_HIGH 0x69
#define SHAKE_THRESHOLD 12000L  // зміна суми прискорень (у відліках датчика)
#define TURN_THRESHOLD 18000L   // сума швидкостей обертання (відліки гіроскопа)
#define CALM_THRESHOLD 2500L   // нижче цього рівня чекаємо заспокоєння
#define CALM_TURN_THRESHOLD 3000L
#define CALM_TIME_MS 500UL

#include <Wire.h>
#include <stdlib.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <FastLED.h>
#include "utils.h"           // для Utf8win1251
#include "fonts/tahomaUkr6.h"  // ПІДКЛЮЧАЄМО 6-Й ШРИФТ

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32

CRGB leds[NUM_LEDS];
byte counter;
uint8_t mpuAddress = MPU_ADDR_LOW;
bool mpuReady = false;
bool accelValid = false;
bool shakeArmed = true;
unsigned long calmSince = 0;
int16_t previousX, previousY, previousZ;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// GY-521 має адресу 0x68 (AD0=LOW) або 0x69 (AD0=HIGH).
bool mpuPresent(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

bool writeMpuRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(mpuAddress);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

bool initMpu() {
  if (mpuPresent(MPU_ADDR_LOW)) mpuAddress = MPU_ADDR_LOW;
  else if (mpuPresent(MPU_ADDR_HIGH)) mpuAddress = MPU_ADDR_HIGH;
  else return false;

  // Вихід зі сну; діапазони: акселерометр ±2 g, гіроскоп ±250 °/с.
  return writeMpuRegister(0x6B, 0x00) &&
         writeMpuRegister(0x1C, 0x00) &&
         writeMpuRegister(0x1B, 0x00);
}

bool readMotion(int16_t &x, int16_t &y, int16_t &z,
                int16_t &gx, int16_t &gy, int16_t &gz) {
  Wire.beginTransmission(mpuAddress);
  Wire.write(0x3B);  // ACCEL_XOUT_H
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(mpuAddress, (uint8_t)14) != 14) return false;

  x = (int16_t)((uint16_t)Wire.read() << 8 | Wire.read());
  y = (int16_t)((uint16_t)Wire.read() << 8 | Wire.read());
  z = (int16_t)((uint16_t)Wire.read() << 8 | Wire.read());
  Wire.read();  // температура: два байти, тут не потрібна
  Wire.read();
  gx = (int16_t)((uint16_t)Wire.read() << 8 | Wire.read());
  gy = (int16_t)((uint16_t)Wire.read() << 8 | Wire.read());
  gz = (int16_t)((uint16_t)Wire.read() << 8 | Wire.read());
  return true;
}

bool shakeDetected() {
  if (!mpuReady) return false;

  int16_t x, y, z, gx, gy, gz;
  if (!readMotion(x, y, z, gx, gy, gz)) {
    accelValid = false;
    calmSince = 0;
    return false;
  }

  if (!accelValid) {
    previousX = x;
    previousY = y;
    previousZ = z;
    accelValid = true;
    return false;
  }

  long motion = labs((long)x - previousX) +
                labs((long)y - previousY) +
                labs((long)z - previousZ);
  long turn = labs((long)gx) + labs((long)gy) + labs((long)gz);
  previousX = x;
  previousY = y;
  previousZ = z;

  if (!shakeArmed) {
    if (motion < CALM_THRESHOLD && turn < CALM_TURN_THRESHOLD) {
      if (calmSince == 0) calmSince = millis();
      if (millis() - calmSince >= CALM_TIME_MS) shakeArmed = true;
    } else {
      calmSince = 0;
    }
    return false;
  }

  if (motion > SHAKE_THRESHOLD || turn > TURN_THRESHOLD) {
    shakeArmed = false;
    calmSince = 0;
    return true;
  }
  return false;
}

// Збільшуємо розмір рядка до 64 байтів, щоб всі UTF-8 літери точно влізли
const char positiveMessages[][64] PROGMEM = {
  "YCПIX ВЖЕ HA ШЛЯХУ ДО TEБE!",
  "ЧУДОВІ НОВИНИ ПPИЙДYTb!",
  "РАДІСТЬ Y BAШOMY ЖИТТІ!",
  "ТВОЯ МРІЯ ЗДІЙСНИТЬСЯ!",
  "ЩАСТЯ ТЕБЕ HE ОБІЙДЕ!",
  "ЧЕКАЙ ВЕЛИКЕ ДОСЯГНЕННЯ!",
  "ЗУСИЛЛЯ ПРИНЕСУТЬ ПЛОДИ!",
  "ЧЕКАЙ ХОРОШИХ СЮРПРИЗІВ!",
  "YДAЧA HA BAШOMY БОЦІ!",
  "ГAPMOHIЯ ВЖЕ ПОРЯД!",
  "TAK!"
};

const char negativeMessages[][64] PROGMEM = {
  "HEБE3ПEKA ВЖЕ ПОРЯД",
  "ОБЕРЕЖНІШЕ 3 ВИБОРОМ",
  "ДРУЗІ МОЖУТЬ ЗРАДИТИ",
  "ОБЕРЕЖНО 3 ОБІЦЯНКАМИ",
  "БУДУТЬ HOBI ТРУДНОЩІ",
  "HE ДОВІРЯЙ НЕЗНАЙОМЦЯМ",
  "УНИКАЙ РИЗИКОВИХ СПРАВ",
  "ОЧІКУЙ РОЗЧАРУВАННЯ",
  "HI ЦE НЕМОЖЛИВО",
  "ПРОБЛЕМИ 3 ФІНАНСАМИ",
  "HI"
};

const char neutralMessages[][64] PROGMEM = {
  "ЧАС ПОКАЖЕ",
  "3AЛИШAЙCЯ СПОКІЙНИМ",
  "HA ПРАВИЛЬНОМУ ШЛЯХУ",
  "НАКОПИ ДОСВІД",
  "БУДЬ ВІДКРИТИЙ",
  "HE ПОСПІШАЙ 3 РІШЕННЯМ",
  "СИТУАЦІЯ CTAБIЛbHA",
  "BCE ЙДЕ 3A ПЛАНОМ",
  "МОЖЛИВІ HOBI ШЛЯХИ",
  "ВАЖКО ПЕРЕДБАЧИТИ",
  "MOЖЛИBO"
};

// Допоміжна функція для читання з PROGMEM
void getProgmemString(const char* source, char* buffer, size_t bufferSize) {
  strncpy_P(buffer, source, bufferSize);
  buffer[bufferSize - 1] = 0; // гарантія нуль-термінатора
}

// ПРИМУСОВЕ ЦЕНТРУВАННЯ ЗА КІЛЬКІСТЮ СИМВОЛІВ
void printTextCentered(const char* text) {
  display.clearDisplay();
  display.setFont(&tahomaUkr6); 
  display.setTextColor(SSD1306_WHITE);

  String convertedText = String(Utf8win1251(text));

  int16_t x1, y1;
  uint16_t w, h;

  if (convertedText.length() <= 14) {
    display.getTextBounds(convertedText, 0, 0, &x1, &y1, &w, &h);
    int16_t x = (SCREEN_WIDTH - w) / 2;
    if (x < 0) x = 0;
    
    display.setCursor(x, 20); 
    display.print(convertedText);
  } 
  else {
    int splitIndex = -1;
    int middle = convertedText.length() / 2;
    int minDiff = 1000;

    for (int i = 0; i < convertedText.length(); i++) {
      if (convertedText[i] == ' ') {
        int diff = abs(i - middle);
        if (diff < minDiff) {
          minDiff = diff;
          splitIndex = i;
        }
      }
    }

    String line1 = convertedText;
    String line2 = "";
    
    if (splitIndex != -1) {
      line1 = convertedText.substring(0, splitIndex);
      line2 = convertedText.substring(splitIndex + 1);
    } 
    else {
      line1 = convertedText.substring(0, middle);
      line2 = convertedText.substring(middle);
    }

    display.getTextBounds(line1, 0, 0, &x1, &y1, &w, &h);
    int16_t x_line1 = (SCREEN_WIDTH - w) / 2;
    if (x_line1 < 0) x_line1 = 0;
    display.setCursor(x_line1, 12); 
    display.print(line1);

    display.getTextBounds(line2, 0, 0, &x1, &y1, &w, &h);
    int16_t x_line2 = (SCREEN_WIDTH - w) / 2;
    if (x_line2 < 0) x_line2 = 0;
    display.setCursor(x_line2, 28); 
    display.print(line2);
  }

  display.display();
}

// Переливання WS2812
void rainbowLeds() {
  for(int i=0; i<NUM_LEDS; i++){
    leds[i] = CHSV(counter + i*20, 255, 255);
  }
  counter++;
  FastLED.show();
  delay(20);
}

// Показує передбачення з кольором WS2812
void showPrediction(const char* text, CRGB color) {
  for(int i=0; i<NUM_LEDS; i++) leds[i]=color;
  FastLED.show();

  printTextCentered(text);
  delay(5000); // показ передбачення
}

// Заставка
void showSplash() {
  printTextCentered("MAGIK 8 BALL");
}

void setup() {
  Wire.begin();
  mpuReady = initMpu();
  randomSeed(analogRead(A0) ^ micros());

  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(180);

  pinMode(BUTTON_PIN, INPUT);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) while(true);

  display.clearDisplay();
  if (mpuReady) showSplash();
  else printTextCentered("MPU-6050 НЕ ЗНАЙДЕНО");
  delay(2000);
  showSplash();
}

void loop() {
  bool buttonPressed = digitalRead(BUTTON_PIN) == HIGH;
  bool shaken = shakeDetected();

  if (buttonPressed || shaken) {

    // Збільшуємо буфер для читання, щоб великі фрази поміщалися
    char buffer[64]; 
    const char* text;
    CRGB color;
    int choice = random(3); 
    
    // Тепер у нас 11 повідомлень у кожній категорії (від 0 до 10)
    int index = random(11); 

    if(choice==0){
      getProgmemString(positiveMessages[index], buffer, sizeof(buffer));
      text = buffer;
      color = CRGB::Green;
    } else if(choice==1){
      getProgmemString(negativeMessages[index], buffer, sizeof(buffer));
      text = buffer;
      color = CRGB::Red;
    } else {
      getProgmemString(neutralMessages[index], buffer, sizeof(buffer));
      text = buffer;
      color = CRGB::Blue;
    }

    showPrediction(text, color);
    showSplash();

    // Після паузи не порівнюємо новий вимір зі старим до показу фрази.
    accelValid = false;
    shakeArmed = false;
    calmSince = 0;

    while (digitalRead(BUTTON_PIN) == HIGH) delay(10);
  } else {
    rainbowLeds();
  }
}
