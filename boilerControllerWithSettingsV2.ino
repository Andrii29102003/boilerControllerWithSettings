//BY DANDRII

#include <OneWire.h>
#include <DallasTemperature.h>
#include "RTClib.h"
#include <LiquidCrystal.h>
#include <EEPROM.h>
#include <OneButton.h>
#include <avr/wdt.h>


// Піни
#define ONE_WIRE_BUS_PIN 4 
#define BOILER_SWITCH_PIN 11
#define WATER_USAGE_SENSOR_PIN 2
#define ELECTRIC_USAGE_SENSOR_PIN 3
#define SETTING_BUTTON_PIN A0
#define CHANGE_BUTTON_PIN A1
#define DAY_STAT_BUTTON_PIN A2

//LCD
#define RS_PIN 5
#define EN_PIN 6
#define D4_PIN 7
#define D5_PIN 8
#define D6_PIN 9
#define D7_PIN 10

// Затримки
#define SETUP_DELAY 2000 // мс
#define BOILER_ITERATION_DELAY 2000 // мс
#define SAVE_INTERVAL_MS 3600000UL // 1 година в мілісекундах
#define MAIN_LOOP_DELAY 1 // мс

// Температурні межі
#define LOGIC_MIN_WORK_TEMP -50
#define LOGIC_MAX_WORK_TEMP 90

#define USED_WATER_EEPROM_ADDRESS 0
#define USED_ELECTRIC_EEPROM_ADDRESS 4
#define LOSTED_MONEY_FOR_WATER_EEPROM_ADDRESS 8
#define LOSTED_MONEY_FOR_ELECTRIC_EEPROM_ADDRESS 12
#define WATER_LITER_PRICE_EEPROM_ADDRESS 16
#define ELECTRIC_WATT_PRICE_EEPROM_ADDRESS 20

#define NO_ERROR_MSG " "
#define TEMPERATURE_ERROR_MSG "!TEMP ERROR!"
#define TIME_ERROR_MSG "!TIME ERROR!"
#define TEMPERATURE_AND_TIME_ERROR_MSG "!TEMP + TIME ERROR!"

// Константи
// const float WATER_LITER_PRICE = 80.0/1000;
// const float ELECTRIC_WATT_PRICE = 4.8/1000;
const float LITERS_PER_IMPULSE = 1.0/450;
const float WATT_PER_IMPULSE = 1000.0/3200;
const uint8_t SETTING_PAGES_COUNT = 26;
const uint8_t SETTING_PAGE_NAME_LEN = 6;
const uint8_t SETTING_PAGE_AVAILABLE_VALUES_COUNT = 8;
const uint8_t SETTING_PAGES_BOILING_HOURS_START_INDEX = 0; //Початок сторінок налаштувань годин роботи, години мають іти по порядку від 00 до 23
const uint8_t DAY_STAT_PAGES_COUNT = 31;
const uint16_t DAY_STAT_PAGES_START_EEPROM_ADDRESS = 80;
const uint8_t SETTING_FLOAT_VALUE_PAGES_COUNT = 2;

//Структури
struct settingPage{
  char name[SETTING_PAGE_NAME_LEN];
  uint8_t defaultValue;
  uint8_t value;
  uint8_t availableValues[SETTING_PAGE_AVAILABLE_VALUES_COUNT];
  uint8_t availableValuesCount;
  uint16_t eepromAddress;
};
struct settingFloatValuePage{
  char name[SETTING_PAGE_NAME_LEN];
  float value;
  float step;
  uint16_t eepromAddress;
};
struct dayStatPage{
  uint8_t day;
  uint8_t month;
  uint16_t year;
  uint16_t usedWaterInLiters;
  uint16_t lostedMoneyForWater;
  uint16_t usedElectricInWatts;
  uint16_t lostedMoneyForElectric;
  uint16_t hotWaterLiterPriceInCoins;
};
const uint8_t DAY_STAT_PAGE_LEN = sizeof(dayStatPage); //Константа довжини однієї dayStatPage

// Глобальні змінні
bool isBoilingTime = false;
bool isBoilingNeeded = false;
bool isTemperatureError = false;
bool isTimeError = false;
uint8_t nowHour = 0;
float temperature = DEVICE_DISCONNECTED_C;
volatile uint32_t waterImpulses = 0;
volatile uint32_t electricImpulses = 0;
float usedWaterInLiters = 0.0;
float usedElectricInWatts = 0.0;
float lostedMoneyForWater = 0.0;
float lostedMoneyForElectric = 0.0;
float nowDayUsedWaterInLiters = 0.0;
float nowDayUsedElectricInWatts = 0.0;
float nowDayLostedMoneyForWater = 0.0;
float nowDayLostedMoneyForElectric = 0.0;
uint32_t lastSaveMillis = 0;
uint32_t lastBoilerIterationMillis = 0;
bool isSettingPagesActive = false;
uint8_t nowDisplayedSettingPage = 0;
uint8_t nowDisplayedAvailableValueOnSettingPage = 0;
bool isCurrentDayStatPageActive = false;
uint8_t nowDisplayedDayStatPage = 0;
bool isAlreadyNowDayStatPageSavedToEeprom = false;
bool isSettingFloatValuePagesActive = false;
uint8_t nowDisplayedSettingFloatValuePage = 0;
char errorMsg[32];
settingPage settingPages[SETTING_PAGES_COUNT] = {
  {"00h", 0, 0, {0, 1}, 2, 45},
  {"01h", 0, 0, {0, 1}, 2, 46},
  {"02h", 0, 0, {0, 1}, 2, 47},
  {"03h", 0, 0, {0, 1}, 2, 48},
  {"04h", 0, 0, {0, 1}, 2, 49},
  {"05h", 1, 1, {0, 1}, 2, 50},
  {"06h", 1, 1, {0, 1}, 2, 51},
  {"07h", 1, 1, {0, 1}, 2, 52},
  {"08h", 0, 0, {0, 1}, 2, 53},
  {"09h", 0, 0, {0, 1}, 2, 54},
  {"10h", 0, 0, {0, 1}, 2, 55},
  {"11h", 0, 0, {0, 1}, 2, 56},
  {"12h", 0, 0, {0, 1}, 2, 57},
  {"13h", 0, 0, {0, 1}, 2, 58},
  {"14h", 0, 0, {0, 1}, 2, 59},
  {"15h", 0, 0, {0, 1}, 2, 60},
  {"16h", 0, 0, {0, 1}, 2, 61},
  {"17h", 0, 0, {0, 1}, 2, 62},
  {"18h", 0, 0, {0, 1}, 2, 63},
  {"19h", 0, 0, {0, 1}, 2, 64},
  {"20h", 0, 0, {0, 1}, 2, 65},
  {"21h", 0, 0, {0, 1}, 2, 66},
  {"22h", 0, 0, {0, 1}, 2, 67},
  {"23h", 0, 0, {0, 1}, 2, 68},
  {"MIN T", 40, 40, {25, 30, 33, 35, 40, 45, 50, 60}, 8, 69},
  {"GIS T", 10, 10, {5, 10, 15, 20, 30}, 5, 70}
};
settingFloatValuePage settingFloatValuePages[SETTING_FLOAT_VALUE_PAGES_COUNT] = {
  {"GpL", 0.08, 0.001, WATER_LITER_PRICE_EEPROM_ADDRESS},
  {"GpW", 0.0048,  0.0001, ELECTRIC_WATT_PRICE_EEPROM_ADDRESS}
};
dayStatPage currentDayStatPage;
dayStatPage nowDayStatPage;

RTC_DS3231 rtc;
DateTime now;
OneWire oneWire(ONE_WIRE_BUS_PIN);
DallasTemperature tSensor(&oneWire);
LiquidCrystal lcd(RS_PIN, EN_PIN, D4_PIN, D5_PIN, D6_PIN, D7_PIN);
OneButton settingButton(SETTING_BUTTON_PIN, true); // активна кнопка при LOW
OneButton changeButton(CHANGE_BUTTON_PIN, true); // активна кнопка при LOW
OneButton dayStatButton(DAY_STAT_BUTTON_PIN, true); // активна кнопка при LOW

// Функція обробки імпульсів лічильника води
void countUsedWaterImpulses() {
  waterImpulses++;
}

// Функція обробки імпульсів лічильника електроенергії
void countUsedElectricImpulses() {
  electricImpulses++;
}

// Перевірка, чи зараз час кип'ятіння
void checkIsBoilingTime() {
  isBoilingTime = false;
  //Години мають іти по порядку від 00 до 23
  if(settingPages[nowHour+SETTING_PAGES_BOILING_HOURS_START_INDEX].value == 1){
    isBoilingTime = true;
  }
}

// Перевірка, чи потрібно вмикати котел
void checkIsBoilingNeeded() {
  checkIsBoilingTime();
  if (!isBoilingTime) {
    isBoilingNeeded = false;
    return;
  }

  if (temperature < settingPages[24].value) {
    isBoilingNeeded = true;
  } else if (temperature > settingPages[24].value + settingPages[25].value) {
    isBoilingNeeded = false;
  }
}

void errorsCheck(){
  sprintf(errorMsg, NO_ERROR_MSG);
  
  if (temperature < LOGIC_MIN_WORK_TEMP || temperature > LOGIC_MAX_WORK_TEMP) {
    isTemperatureError = true;
    sprintf(errorMsg, TEMPERATURE_ERROR_MSG);
  } else {
    isTemperatureError = false;
  }

  if (now.year() < 2000 || now.year() > 2100) {
    isTimeError = true;
    sprintf(errorMsg, TIME_ERROR_MSG);
  } else {
    isTimeError = false;
  }

  if (isTemperatureError && isTimeError){
    sprintf(errorMsg, TEMPERATURE_AND_TIME_ERROR_MSG);
  }

  if (isTemperatureError || isTimeError){
    isBoilingNeeded = false;
  }
}

// Керування котлом
void boilIfNeeded() {
  checkIsBoilingNeeded();
  errorsCheck();
  digitalWrite(BOILER_SWITCH_PIN, isBoilingNeeded ? HIGH : LOW);
}

// Вивід логів в Serial
void printLog() {
  Serial.print(nowHour);
  Serial.print("h");

  Serial.print("  temperature = ");
  Serial.print(temperature != DEVICE_DISCONNECTED_C ? temperature : -999);

  Serial.print("  boil = ");
  Serial.print(isBoilingNeeded ? "YES" : "NO");

  Serial.print("  usedWaterInLiters = ");
  Serial.print(usedWaterInLiters);

  Serial.print("  usedElectricInWatts = ");
  Serial.print(usedElectricInWatts);

  Serial.print("  lostedMoneyForWater = ");
  Serial.print(lostedMoneyForWater);

  Serial.print("  lostedMoneyForElectric = ");
  Serial.print(lostedMoneyForElectric);

  Serial.print("  errorMsg = ");
  Serial.print(errorMsg);

  Serial.println();
}

// Вивід даних на LCD
void lcdPrintData() {
  if(isSettingPagesActive || isCurrentDayStatPageActive || isSettingFloatValuePagesActive){
    return;
  }
  char buf[32];

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("T:");
  lcd.setCursor(2, 0);
  if (isTemperatureError) {
    lcd.print("ERR");
  } else {
    lcd.print(round(temperature));
  }

  lcd.setCursor(6, 0);
  if (isTimeError) {
    lcd.print("ERR");
  } else {
    sprintf(buf, "%2d", nowHour);
    lcd.print(buf);
  }
  lcd.print("H");

  lcd.setCursor(11, 0);
  lcd.print("BOIL:");
  lcd.setCursor(16, 0);
  lcd.print(isBoilingNeeded ? "YES" : "NO");

  //water
  lcd.setCursor(0, 1);
  lcd.print("WTR");

  sprintf(buf, "%8dL", int(round(usedWaterInLiters)));
  lcd.setCursor(3, 1);
  lcd.print(buf);

  sprintf(buf, "%7dG", int(round(lostedMoneyForWater)));
  lcd.setCursor(12, 1);
  lcd.print(buf);

  //electric
  lcd.setCursor(0, 2);
  lcd.print("ELE");

  sprintf(buf, "%8dW", int(round(usedElectricInWatts)));
  lcd.setCursor(3, 2);
  lcd.print(buf);

  sprintf(buf, "%7dG", int(round(lostedMoneyForElectric)));
  lcd.setCursor(12, 2);
  lcd.print(buf);

  //errorMsg
  lcd.setCursor(0, 3);
  lcd.print(errorMsg);
}

void lcdDisplaySettingPage(){
  if(!isSettingPagesActive){
    return;
  }
  lcd.clear();
  lcd.setCursor(4, 0);
  lcd.print("--SETTINGS--");
  lcd.setCursor(0, 1);
  lcd.print(settingPages[nowDisplayedSettingPage].name);
  lcd.setCursor(10, 1);
  lcd.print(settingPages[nowDisplayedSettingPage].value);
}

void lcdDisplayFloatValuePage(){
  if(!isSettingFloatValuePagesActive){
    return;
  }
  lcd.clear();
  lcd.setCursor(4, 0);
  lcd.print("--SETTINGS--");
  lcd.setCursor(0, 1);
  lcd.print(settingFloatValuePages[nowDisplayedSettingFloatValuePage].name);
  lcd.setCursor(10, 1);
  lcd.print(settingFloatValuePages[nowDisplayedSettingFloatValuePage].value, 5);
  lcd.print("G");
}

void lcdDisplayDayStatPage(){
  if(!isCurrentDayStatPageActive){
    return;
  }
  char buf[32];
  lcd.clear();
  lcd.setCursor(4, 0);
  lcd.print("--DAY STAT--");

  //water
  lcd.setCursor(0, 1);
  lcd.print("WTR");

  sprintf(buf, "%8dL", currentDayStatPage.usedWaterInLiters);
  lcd.setCursor(3, 1);
  lcd.print(buf);

  sprintf(buf, "%7dG", currentDayStatPage.lostedMoneyForWater);
  lcd.setCursor(12, 1);
  lcd.print(buf);

  //electric
  lcd.setCursor(0, 2);
  lcd.print("ELE");

  sprintf(buf, "%8dW", currentDayStatPage.usedElectricInWatts);
  lcd.setCursor(3, 2);
  lcd.print(buf);

  sprintf(buf, "%7dG", currentDayStatPage.lostedMoneyForElectric);
  lcd.setCursor(12, 2);
  lcd.print(buf);

  //date
  lcd.setCursor(0, 3);
  lcd.print(currentDayStatPage.day);
  lcd.print(".");
  lcd.print(currentDayStatPage.month);
  lcd.print(".");
  lcd.print(currentDayStatPage.year);

  //hotWaterLiterPriceInCoins
  sprintf(buf, "%5dCPL", currentDayStatPage.hotWaterLiterPriceInCoins);
  lcd.setCursor(12, 3);
  lcd.print(buf);
}

bool boilerIteration(){
  if(millis() - lastBoilerIterationMillis >= BOILER_ITERATION_DELAY) {
    lastBoilerIterationMillis = millis();
    
    nowHour = now.hour();
    tSensor.requestTemperatures();
    temperature = tSensor.getTempCByIndex(0);

    usedWaterInLiters += waterImpulses * LITERS_PER_IMPULSE;
    usedElectricInWatts += electricImpulses * WATT_PER_IMPULSE;
    nowDayUsedWaterInLiters += waterImpulses * LITERS_PER_IMPULSE;
    nowDayUsedElectricInWatts += electricImpulses * WATT_PER_IMPULSE;
    lostedMoneyForWater += waterImpulses * LITERS_PER_IMPULSE * settingFloatValuePages[0].value;
    lostedMoneyForElectric += electricImpulses * WATT_PER_IMPULSE * settingFloatValuePages[1].value;
    nowDayLostedMoneyForWater += waterImpulses * LITERS_PER_IMPULSE * settingFloatValuePages[0].value;
    nowDayLostedMoneyForElectric += electricImpulses * WATT_PER_IMPULSE * settingFloatValuePages[1].value;
    waterImpulses = 0;
    electricImpulses = 0;

    boilIfNeeded();
    lcdPrintData();
    printLog();
    updateNowDayStatPage();

    if (millis() - lastSaveMillis >= SAVE_INTERVAL_MS) {
      lastSaveMillis = millis();
      EEPROM.put(USED_WATER_EEPROM_ADDRESS, usedWaterInLiters);
      EEPROM.put(USED_ELECTRIC_EEPROM_ADDRESS, usedElectricInWatts);
      EEPROM.put(LOSTED_MONEY_FOR_WATER_EEPROM_ADDRESS, lostedMoneyForWater);
      EEPROM.put(LOSTED_MONEY_FOR_ELECTRIC_EEPROM_ADDRESS, lostedMoneyForElectric);
      EEPROM.put(WATER_LITER_PRICE_EEPROM_ADDRESS, settingFloatValuePages[0].value);
      EEPROM.put(ELECTRIC_WATT_PRICE_EEPROM_ADDRESS, settingFloatValuePages[1].value);
      Serial.println("Saved current data to EEPROM");
    }
    return true;

  }else{
    return false;
  }
}

void displayOrHideSettingsPage(){
  if(!isCurrentDayStatPageActive && !isSettingFloatValuePagesActive){
    isSettingPagesActive = !isSettingPagesActive;
    nowDisplayedSettingPage = 0;
    lcdDisplaySettingPage();
    if(!isSettingPagesActive){
      writeSettingsToEeprom();
    }
  }
}

void displayNextSettingPage(){
  if(isSettingPagesActive){
    nowDisplayedSettingPage = (nowDisplayedSettingPage + 1) % SETTING_PAGES_COUNT;
    nowDisplayedAvailableValueOnSettingPage = 0;
    lcdDisplaySettingPage();
  }
  if(isSettingFloatValuePagesActive){
    nowDisplayedSettingFloatValuePage = (nowDisplayedSettingFloatValuePage + 1) % SETTING_FLOAT_VALUE_PAGES_COUNT;
    lcdDisplayFloatValuePage();
  }
}

void setNextValueOnSettingPage(){
  if(isSettingPagesActive){
    uint8_t settingPageAvailableValuesCount = settingPages[nowDisplayedSettingPage].availableValuesCount;
    if(settingPageAvailableValuesCount >= SETTING_PAGE_AVAILABLE_VALUES_COUNT){
      settingPageAvailableValuesCount = SETTING_PAGE_AVAILABLE_VALUES_COUNT;
    }
    nowDisplayedAvailableValueOnSettingPage = (nowDisplayedAvailableValueOnSettingPage + 1) % settingPageAvailableValuesCount;
    settingPages[nowDisplayedSettingPage].value = settingPages[nowDisplayedSettingPage].availableValues[nowDisplayedAvailableValueOnSettingPage];
    lcdDisplaySettingPage();
  }
  if(isSettingFloatValuePagesActive){ //value - step
    settingFloatValuePages[nowDisplayedSettingFloatValuePage].value -= settingFloatValuePages[nowDisplayedSettingFloatValuePage].step;
    lcdDisplayFloatValuePage();
  }
}

void setDefaultValueOnSettingPage(){
  if(isSettingPagesActive){
    settingPages[nowDisplayedSettingPage].value = settingPages[nowDisplayedSettingPage].defaultValue;
    lcdDisplaySettingPage();
  }
}

void displayOrHideDayStatPage(){
  if(!isSettingPagesActive && !isSettingFloatValuePagesActive){
    isCurrentDayStatPageActive = !isCurrentDayStatPageActive;
    nowDisplayedDayStatPage = 0;
    readCurrentDayStatPageFromEeprom(nowDisplayedDayStatPage);
    lcdDisplayDayStatPage();
  }
}

void displayNextDayStatPage(){
  if(isCurrentDayStatPageActive){
    nowDisplayedDayStatPage = (nowDisplayedDayStatPage + 1) % DAY_STAT_PAGES_COUNT;
    readCurrentDayStatPageFromEeprom(nowDisplayedDayStatPage);
    lcdDisplayDayStatPage();
  }
  if(isSettingFloatValuePagesActive){ //value + step
    settingFloatValuePages[nowDisplayedSettingFloatValuePage].value += settingFloatValuePages[nowDisplayedSettingFloatValuePage].step;
    lcdDisplayFloatValuePage();
  }
}

void displayNowDayStatPage(){
  if(isCurrentDayStatPageActive){
    currentDayStatPage = nowDayStatPage;
    lcdDisplayDayStatPage();
  }
}

void displayOrHideSettingsFloatValuePage(){
  if(!isCurrentDayStatPageActive && !isSettingPagesActive){
    isSettingFloatValuePagesActive = !isSettingFloatValuePagesActive;
    nowDisplayedSettingFloatValuePage = 0;
    readCurrentDayStatPageFromEeprom(nowDisplayedDayStatPage);
    lcdDisplayFloatValuePage();
    if(!isSettingFloatValuePagesActive){
      writeSettingFloatValuePagesToEeprom();
    }
  }
}

void readSettingsFromEeprom(){
  for(int i = 0; i < SETTING_PAGES_COUNT; i++){
    EEPROM.get(settingPages[i].eepromAddress, settingPages[i].value);
  }
}

void writeSettingsToEeprom(){
  for(int i = 0; i < SETTING_PAGES_COUNT; i++){
    EEPROM.put(settingPages[i].eepromAddress, settingPages[i].value);
  }
}

void readSettingFloatValuePagesFromEeprom(){
  for(int i = 0; i < SETTING_FLOAT_VALUE_PAGES_COUNT; i++){
    EEPROM.get(settingFloatValuePages[i].eepromAddress, settingFloatValuePages[i].value);
  }
}

void writeSettingFloatValuePagesToEeprom(){
  for(int i = 0; i < SETTING_FLOAT_VALUE_PAGES_COUNT; i++){
    EEPROM.put(settingFloatValuePages[i].eepromAddress, settingFloatValuePages[i].value);
  }
}

void readCurrentDayStatPageFromEeprom(uint8_t indexOfDayStatPage){
  EEPROM.get(DAY_STAT_PAGES_START_EEPROM_ADDRESS + indexOfDayStatPage * DAY_STAT_PAGE_LEN, currentDayStatPage);
}

void writeDayStatPageToEeprom(uint8_t indexOfDayStatPage){
  EEPROM.put(DAY_STAT_PAGES_START_EEPROM_ADDRESS + indexOfDayStatPage * DAY_STAT_PAGE_LEN, nowDayStatPage);
}

void dayStatPagesEepromInit(){
  for(int i = 0; i < DAY_STAT_PAGES_COUNT; i++){
    nowDayStatPage = {99, 99, 99, 99, 99, 99, 99, 99};
    writeDayStatPageToEeprom(i);
  }
}

void writeDayStatPageToEepromIfCurrentDayEnds(){
  if (now.hour() == 0 && now.minute() == 0 && now.second() == 0) {
    if (!isAlreadyNowDayStatPageSavedToEeprom) {
      writeDayStatPageToEeprom(now.day() - 1);
      nowDayUsedWaterInLiters = 0.0;
      nowDayUsedElectricInWatts = 0.0;
      nowDayLostedMoneyForWater = 0.0;
      nowDayLostedMoneyForElectric = 0.0;
      nowDayStatPage = {99, 99, 99, 99, 99, 99, 99, 99};
      isAlreadyNowDayStatPageSavedToEeprom = true;
    }
  } else {
    // Після 00:00:01 — дозволити наступне спрацювання наступної ночі
    isAlreadyNowDayStatPageSavedToEeprom = false;
  }
}

void writePricesToEeprom(){
  EEPROM.put(WATER_LITER_PRICE_EEPROM_ADDRESS, 80.0/1000);
  EEPROM.put(ELECTRIC_WATT_PRICE_EEPROM_ADDRESS, 4.8/1000);
}

void updateNowDayStatPage(){
  nowDayStatPage.day = now.day();
  nowDayStatPage.month = now.month();
  nowDayStatPage.year = now.year();
  nowDayStatPage.usedWaterInLiters = nowDayUsedWaterInLiters;
  nowDayStatPage.lostedMoneyForWater = nowDayLostedMoneyForWater;
  nowDayStatPage.usedElectricInWatts = nowDayUsedElectricInWatts;
  nowDayStatPage.lostedMoneyForElectric = nowDayLostedMoneyForElectric;
  if (nowDayUsedWaterInLiters > 0.1) {
    float totalCost = nowDayLostedMoneyForWater + nowDayLostedMoneyForElectric;
    float costPerLiter = totalCost / nowDayUsedWaterInLiters * 100;
    nowDayStatPage.hotWaterLiterPriceInCoins = uint16_t(round(costPerLiter));
  } else {
    nowDayStatPage.hotWaterLiterPriceInCoins = 0;
  }
}

void clearEEPROM(){
  for (int i = 0; i < EEPROM.length(); i++){
    EEPROM.write(i, 0);
  }
}

void setup() {
  wdt_enable(WDTO_8S);

  Serial.begin(9600);
  Serial.println("  ");
  Serial.println("--START--");

  if (!rtc.begin()) {
    Serial.println("--RTC ERR--");
    while (1) delay(10);
  }

  // Один раз виставте час, потім закоментуйте цей рядок!
  // rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  //Стерти EEPROM, перезеписати її нулями
  //clearEEPROM();

  //Записати значення за замовчуванням
  //writeSettingsToEeprom();
  //dayStatPagesEepromInit();
  //writePricesToEeprom();

  tSensor.begin();
  tSensor.requestTemperatures();

  lcd.begin(20, 4);
  lcd.setCursor(5, 1);
  lcd.print("--START--");
  lcd.setCursor(4, 2);
  lcd.print("READ EEPROM");

  pinMode(BOILER_SWITCH_PIN, OUTPUT);
  pinMode(WATER_USAGE_SENSOR_PIN, INPUT_PULLUP);
  pinMode(ELECTRIC_USAGE_SENSOR_PIN, INPUT_PULLUP);

  settingButton.attachLongPressStart(displayOrHideSettingsPage);
  settingButton.attachClick(displayNextSettingPage);
  settingButton.attachDoubleClick(displayOrHideSettingsFloatValuePage);
  changeButton.attachLongPressStart(setDefaultValueOnSettingPage);
  changeButton.attachClick(setNextValueOnSettingPage);
  dayStatButton.attachLongPressStart(displayOrHideDayStatPage);
  dayStatButton.attachClick(displayNextDayStatPage);
  dayStatButton.attachDoubleClick(displayNowDayStatPage);

  digitalWrite(BOILER_SWITCH_PIN, LOW);
  attachInterrupt(digitalPinToInterrupt(WATER_USAGE_SENSOR_PIN), countUsedWaterImpulses, FALLING);
  attachInterrupt(digitalPinToInterrupt(ELECTRIC_USAGE_SENSOR_PIN), countUsedElectricImpulses, FALLING);

  EEPROM.get(USED_WATER_EEPROM_ADDRESS, usedWaterInLiters);
  EEPROM.get(USED_ELECTRIC_EEPROM_ADDRESS, usedElectricInWatts);
  EEPROM.get(LOSTED_MONEY_FOR_WATER_EEPROM_ADDRESS, lostedMoneyForWater);
  EEPROM.get(LOSTED_MONEY_FOR_ELECTRIC_EEPROM_ADDRESS, lostedMoneyForElectric);

  readSettingsFromEeprom();
  readSettingFloatValuePagesFromEeprom();

  if(isnan(usedWaterInLiters)) usedWaterInLiters = 0.0;
  if(isnan(usedElectricInWatts)) usedElectricInWatts = 0.0;
  if(isnan(lostedMoneyForWater)) lostedMoneyForWater = 0.0;
  if(isnan(lostedMoneyForElectric)) lostedMoneyForElectric = 0.0;
  if(isnan(settingFloatValuePages[0].value)) settingFloatValuePages[0].value = 80.0/1000;
  if(isnan(settingFloatValuePages[1].value)) settingFloatValuePages[1].value = 4.8/1000;

  lastSaveMillis = millis();

  sprintf(errorMsg, NO_ERROR_MSG);

  delay(SETUP_DELAY);
}

void loop() {

  now = rtc.now();

  boilerIteration();
  writeDayStatPageToEepromIfCurrentDayEnds();

  settingButton.tick();
  changeButton.tick();
  dayStatButton.tick();

  wdt_reset();

  delay(MAIN_LOOP_DELAY);
}
