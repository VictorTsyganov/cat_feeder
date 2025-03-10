#include <Arduino.h>
#include <EncButton.h>
#include <EEPROM.h>
#include <microDS3231.h>
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>

#define BTN 2
#define STEPS_FRW 12
#define STEPS_BKW 19
#define FEED_SPEED 2000
#define EE_RESET 12

LiquidCrystal_I2C lcd(0x27, 16, 2);
Button btn(BTN);
MicroDS3231 rtc;

int feedAmount = 30;
const byte drvPins[] = {3, 4, 5, 6};
const byte feedTime[][2] = {
  {6, 30},
  {14, 0},
  {19, 0},
  {23, 55},
};
bool feedByTimer = true;

void showFeederMessage(int caseNum) {
  lcd.clear();
  lcd.setCursor(0, 1);
  switch (caseNum)
  {
  case 1: lcd.print("Set Am: "+ String(feedAmount));
    break;
  
  case 2: lcd.print("NTM Feed Am: "+ String(feedAmount));
    break;
  
  case 3: lcd.print("TM Feed Am: "+ String(feedAmount));
    break;

  case 4: lcd.print("TMF active: "+ String(feedAmount));
    break;

  case 5: lcd.print("TMF stopped!!!");
    break;

  default: lcd.print("Started. Am "+ String(feedAmount));
    break;
  }
}

void setup() {
  Serial.begin(9600);
  lcd.init();
	lcd.backlight();
  rtc.begin();
  if (EEPROM.read(0) != EE_RESET) {
    EEPROM.write(0, EE_RESET);
    EEPROM.put(1, feedAmount);
    rtc.setTime(BUILD_SEC, BUILD_MIN, BUILD_HOUR, BUILD_DAY, BUILD_MONTH, BUILD_YEAR);
  }
  EEPROM.get(1, feedAmount);
  showFeederMessage(100);  
  for (byte i = 0; i < 4; i++) pinMode(drvPins[i], OUTPUT);
}

void setLcdDateTime() {
  DateTime now = rtc.getTime();
  lcd.setCursor(0, 0);
  lcd.print(
    (String(now.hour).length() == 1? '0' + String(now.hour) : String(now.hour))+":"+
    (String(now.minute).length() == 1? '0' + String(now.minute) : String(now.minute))+":"+
    (String(now.second).length() == 1? '0' + String(now.second) : String(now.second))+" "+
    (String(now.date).length() == 1? '0' + String(now.date) : String(now.date))+"/"+
    (String(now.month).length() == 1? '0' + String(now.month) : String(now.month))+" "+
    (String(now.day))
  );
}

const byte steps[] = {0b1010, 0b0110, 0b0101, 0b1001};
void runMotor(int8_t dir) {
  static byte step = 0;
  for (byte i = 0; i < 4; i++) digitalWrite(drvPins[i], bitRead(steps[step & 0b11], i));
  delayMicroseconds(FEED_SPEED);
  step += dir;
}

void oneRev() {
  for (int i = 0; i < STEPS_BKW; i++) runMotor(-1);
  for (int i = 0; i < STEPS_FRW; i++) runMotor(1);
}

void disableMotor() {
  for (byte i = 0; i < 4; i++) digitalWrite(drvPins[i], 0);
}

void feed() {
  for (int i = 0; i < feedAmount; i++) oneRev();
  disableMotor();
}

void loop() {
  static uint32_t tmr = 0;
  if(feedByTimer) {
    if (millis() - tmr > 500) {
      static byte prevMin = 0;
      tmr = millis();
      DateTime now = rtc.getTime();
      if (prevMin != now.minute) {
        prevMin = now.minute;
        for (byte i = 0; i < sizeof(feedTime) / 2; i++)
          if (feedTime[i][0] == now.hour && feedTime[i][1] == now.minute) {
            feed();
            showFeederMessage(3);
          }
      }
    }
  }
  
  static uint32_t tmr2 = 0;
  if (millis() - tmr2 > 250) {
    tmr2 = millis();
    setLcdDateTime();
  }

  btn.tick();
  if (btn.hasClicks(2)) {
    feed();
    showFeederMessage(2);
  }

  if (btn.holding()) {
    int newAmount = 0;
    while (btn.holding()) {
      btn.tick();
      oneRev();
      newAmount++;
    }
    disableMotor();
    feedAmount = newAmount;
    EEPROM.put(1, feedAmount);
    showFeederMessage(1);
  }
  if(btn.click()) {
    feedByTimer = !feedByTimer;
    if(feedByTimer) {
      showFeederMessage(4);
    } else {
      showFeederMessage(5);
    }
  }
}