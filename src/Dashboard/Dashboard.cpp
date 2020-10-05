#include "Dashboard.h"

//create battery object
Battery battery(BATT_MIN_VOLTAGE, BATT_MAX_VOLTAGE, BATT_VOLTAGE_SENSE_PIN);
//voltage reference for battery sense
VoltageReference vRef = VoltageReference();

bool prevIsLeftOn = false; //left light's state in the previous update
bool prevIsRightOn = false; //right light's state in the previous update
bool prevIsLoOn = false; //lo light's state in the previous update
bool prevIsHiOn = false; //hi light's state in the previous update
uint8_t prevBatteryPercentage = 0; //battery percentage in the previous update
uint16_t prevBatteryVoltage = 0; //battery voltage in previous update
int8_t prevBatteryCurrent = 0; //battery current in previous update
int8_t prevBatteryTemperature = 0; //battery temperature in previous update
uint8_t prevSpeed = 0; //speed in the previous update in mph
volatile long prevSignalTime = 0; //time at the end of the previous call to update speed
volatile long currentSignalTime = 0; //time of the final pulse before the call to update speed
volatile uint8_t pulses = 0; //number of pulses between the calls to update speed

/*
   Constructor
*/
Dashboard::Dashboard(Adafruit_RA8875 tft)
  : m_display(tft), m_isCharging(false), m_warnings{false, false, false, false}
  , m_batteryVoltage(0), m_batteryPercentage(0), m_batteryTemperature(0)
  , m_isLeftOn(false), m_isRightOn(false), m_isLoOn(false), m_isHiOn(false)
  , m_speed(0), m_refVoltage(0), m_batteryCurrent(0)
{
}

void Dashboard::begin() {
  //initialize display with 800x480 resolution
  if (!m_display.begin(RA8875_800x480)) {
    Serial.println("Display not found");
    while (1);
  }
  Serial.println("Starting display");

  //read the reference voltage
  vRef.begin(EEPROM.read(VREF_EEPROM_ADDR), EEPROM.read(VREF_EEPROM_ADDR + 1), EEPROM.read(VREF_EEPROM_ADDR + 2));
  m_refVoltage = vRef.readVcc();

  //initialize battery library with board's reference voltage and voltage divider's ratio
  battery.begin(m_refVoltage, DIVIDER_RATIO);

  //set the sense pins as inputs
  pinMode(CHARGE_SENSE_PIN, INPUT_PULLUP);
  pinMode(LEFT_LIGHT_SENSE_PIN, INPUT);
  pinMode(RIGHT_LIGHT_SENSE_PIN, INPUT);
  pinMode(LO_LIGHT_SENSE_PIN, INPUT);
  pinMode(HI_LIGHT_SENSE_PIN, INPUT);
  pinMode(BATT_TEMP_SENSE_PIN, INPUT);
  pinMode(BATT_CURRENT_SENSE_PIN, INPUT);

  //set the speed sense pin as an interrupt pin on rising signal
  pinMode(SPEED_SENSE_PIN, INPUT);
  attachPCINT(digitalPinToPCINT(SPEED_SENSE_PIN), countPulse, RISING);

  //turn display on
  m_display.displayOn(true);
  m_display.GPIOX(true);
  m_display.PWM1config(true, RA8875_PWM_CLK_DIV1024); // PWM output for backlight
  m_display.PWM1out(255);

  initDashboard();
}

void Dashboard::initDashboard() {
  Serial.println("Initializing dashboard");

  //reset screen
  m_display.fillScreen(RA8875_WHITE);

  //draw basic elements
  drawBatteryOutline();
  updateBatteryDisplay();
  drawLightIndicators();
  drawWarningBox();

  //dashboard if battery's not charging
  if (!isCharging()) {
    drawSpeedIndicator();
    updateSpeedDisplay();
  }
  //dashboard if battery's charging
  else {
    drawBatteryVoltageDisplay();
    drawBatteryTemperatureDisplay();
    drawBatteryCurrentDisplay();
  }
}

void Dashboard::updateDashboard() {
  Serial.println("Updating dashboard display");
  bool chargingStateChanged = updateChargingState();

  //if charging state's changed, reset the dashboard display
  if (chargingStateChanged) {
    reset();
    initDashboard();
  }

  //only update battery percentage display if the percentage difference is >= battery percentage error
  int8_t batteryPercentageDifference = abs(m_batteryPercentage - prevBatteryPercentage);
  if (batteryPercentageDifference >= BATT_PERCENT_ERROR) {
    updateBatteryDisplay();
  }

  updateLightsDisplay();

  //updates to display when running on battery
  if (!isCharging()) {
    if (prevSpeed != m_speed) {
      updateSpeedDisplay();
    }
  }

  //updates to display when battery's charging
  else {
    if (prevBatteryVoltage != m_batteryVoltage) {
      updateBatteryVoltageDisplay();
    }
    if (prevBatteryTemperature != m_batteryTemperature) {
      updateBatteryTemperatureDisplay();
    }
    if (prevBatteryCurrent != m_batteryCurrent) {
      updateBatteryCurrentDisplay();
    }
  }
}

void Dashboard::updateWarnings() {
  Serial.println("Updating warnings");

  //only check for low battery when battery's not charging
  if (!isCharging()) {
    checkLowBattery();
  }
  checkBatteryOverheat();
  checkBatteryLowTemperature();
  //checkBatteryImbalance();
}

void Dashboard::updateBatteryPercentage() {
  Serial.println("Updating battery percentage");

  //update battery percentage
  prevBatteryPercentage = m_batteryPercentage;
  updateBatteryVoltage();
  m_batteryPercentage = battery.level(m_batteryVoltage);
}

void Dashboard::updateBatteryTemperature() {
  Serial.println("Updating battery temperature");
  prevBatteryTemperature = m_batteryTemperature;
  //TODO: rewrite the algorithm so that it scales with the minimum and maximum voltage inputs from the sensor
  //because the actual values won't be exactly between 0 & 5V
  //scale reading (0-1023, mapped between 0-5V) to temperature
  m_batteryTemperature = (long)analogRead(BATT_TEMP_SENSE_PIN) * (BATT_MAX_TEMP - BATT_MIN_TEMP) / 1024 + BATT_MIN_TEMP;
}

void Dashboard::updateBatteryCurrent() {
  Serial.println("Updating battery current");
  prevBatteryCurrent = m_batteryCurrent;
  //TODO: rewrite the algorithm so that it scales with the minimum and maximum voltage inputs from the sensor
  //because the actual values won't be exactly between 0 & 5V
  //scale reading to current
  m_batteryCurrent = (long)analogRead(BATT_CURRENT_SENSE_PIN) * (BATT_MAX_CURRENT - BATT_MIN_CURRENT) / 1024 + BATT_MIN_CURRENT;
}

void Dashboard::updateLightStates() {
  Serial.println("Updating light states");
  updateLightState(LEFT_LIGHT_SENSE_PIN);
  updateLightState(RIGHT_LIGHT_SENSE_PIN);
  updateLightState(LO_LIGHT_SENSE_PIN);
  updateLightState(HI_LIGHT_SENSE_PIN);
}

void Dashboard::updateSpeed() {
  Serial.println("Updating speed");
  prevSpeed = m_speed;
  long distanceTraveledInches = WHEEL_DIAMETER_INCHES * PI * pulses;
  long timeElapsedMicroseconds = currentSignalTime - prevSignalTime;
  m_speed = distanceTraveledInches * 56818 / timeElapsedMicroseconds; //convert speed from in/ms to mph
  pulses = 0;
  prevSignalTime = micros();
}

/*



    Private helper functions



*/

bool Dashboard::updateChargingState() {
  Serial.println("Updating charging state");
  int chargeState = digitalRead(CHARGE_SENSE_PIN);
  bool wasCharging = isCharging(); //previous charging state
  if (chargeState == HIGH) {
    Serial.println("Charging");
    m_isCharging = true;
  }
  else {
    Serial.println("Not charging");
    m_isCharging = false;
  }

  //return true if there's a change in charging state
  if (wasCharging != isCharging()) {
    Serial.println("\n\n\n\n\n\n\n\n\n\nCharging state changed!\n\n\n\n\n\n\n\n\n\n");
    return true;
  }

  //return false if there's no change in charging state
  return false;
}

void Dashboard::drawSpeedIndicator() {
  Serial.println("Drawing speed indicator");
  m_display.textMode();
  m_display.textTransparent(RA8875_BLACK);
  char mphString[] = "mph";
  m_display.textEnlarge(3);
  m_display.textSetCursor(420, 200);
  m_display.textWrite(mphString);
}

void Dashboard::drawBatteryOutline() {
  Serial.println("Drawing battery outline");
  m_display.graphicsMode();
  m_display.drawRect(578, 10, 102, 50, RA8875_BLACK);
  m_display.fillRect(680, 20, 10, 30, RA8875_BLACK);
}

void Dashboard::drawLightIndicators() {
  Serial.println("Drawing light indicators");
  drawLeftLight();
  drawRightLight();
  drawLoLight();
  drawHiLight();
}

void Dashboard::drawLeftLight() {
  Serial.println("Drawing left light");
  m_display.graphicsMode();
  m_display.drawRect(270, 370, 70, 70, RA8875_BLACK);
  m_display.fillTriangle(280, 405, 320, 385, 320, 425, RA8875_GREEN);
}

void Dashboard::drawRightLight() {
  Serial.println("Drawing right light");
  m_display.graphicsMode();
  m_display.drawRect(380, 370, 70, 70, RA8875_BLACK);
  m_display.fillTriangle(400, 385, 400, 425, 440, 405, RA8875_GREEN);
}

void Dashboard::drawLoLight() {
  Serial.println("Drawing lo light");
  m_display.graphicsMode();
  m_display.drawRect(160, 370, 70, 70, RA8875_BLACK);
  m_display.fillCurve(195, 405, 25, 20, 2, RA8875_BLUE);
  m_display.fillCurve(195, 405, 25, 20, 3, RA8875_BLUE);
  m_display.drawLine(190, 385, 170, 395, RA8875_BLUE);
  m_display.drawLine(190, 386, 170, 396, RA8875_BLUE);
  m_display.drawLine(190, 387, 170, 397, RA8875_BLUE);
  m_display.drawLine(190, 388, 170, 398, RA8875_BLUE);
  m_display.drawLine(190, 389, 170, 399, RA8875_BLUE);
  m_display.drawLine(190, 402, 170, 412, RA8875_BLUE);
  m_display.drawLine(190, 403, 170, 413, RA8875_BLUE);
  m_display.drawLine(190, 404, 170, 414, RA8875_BLUE);
  m_display.drawLine(190, 405, 170, 415, RA8875_BLUE);
  m_display.drawLine(190, 406, 170, 416, RA8875_BLUE);
  m_display.drawLine(190, 420, 170, 430, RA8875_BLUE);
  m_display.drawLine(190, 421, 170, 431, RA8875_BLUE);
  m_display.drawLine(190, 422, 170, 432, RA8875_BLUE);
  m_display.drawLine(190, 423, 170, 433, RA8875_BLUE);
  m_display.drawLine(190, 424, 170, 434, RA8875_BLUE);
}

void Dashboard::drawHiLight() {
  Serial.println("Drawing hi light");
  m_display.graphicsMode();
  m_display.drawRect(50, 370, 70, 70, RA8875_BLACK);
  m_display.fillCurve(85, 405, 25, 20, 2, RA8875_BLUE);
  m_display.fillCurve(85, 405, 25, 20, 3, RA8875_BLUE);
  m_display.fillRect(60, 385, 20, 5, RA8875_BLUE);
  m_display.fillRect(60, 402, 20, 5, RA8875_BLUE);
  m_display.fillRect(60, 420, 20, 5, RA8875_BLUE);
}

void Dashboard::drawWarningBox() {
  Serial.println("Drawing warning box");
  m_display.graphicsMode();
  m_display.drawRect(578, 150, 200, 300, RA8875_BLACK);
  m_display.textMode();
  char warningString[] = "Warnings";
  m_display.textTransparent(RA8875_RED);
  m_display.textEnlarge(1);
  m_display.textSetCursor(608, 100);
  m_display.textWrite(warningString);
}

void Dashboard::updateBatteryDisplay() {
  Serial.println("Updating battery display");
  m_display.graphicsMode();

  //battery display when not charging
  if (!isCharging()) {
    //Fill in battery. > 20% = green, <= 20% = red
    if (m_batteryPercentage > LOW_BATT_THRESHOLD) {
      m_display.fillRect(579, 11, m_batteryPercentage, 48, RA8875_GREEN);
    }
    else {
      //if <= 20% & > 0%, fill in red
      if (m_batteryPercentage > 0) {
        m_display.fillRect(579, 11, m_batteryPercentage, 48, RA8875_RED);
      }
    }

    //if battery percentage < 100%, fill in the white area
    if (m_batteryPercentage < 100) {
      m_display.fillRect(579 + m_batteryPercentage, 11, 100 - m_batteryPercentage, 48, RA8875_WHITE);
    }

    updateBatteryPercentageDisplay();
  }
  else {
    //only fill in green if battery isn't empty
    if (m_batteryPercentage > 0) {
      m_display.fillRect(579, 11, m_batteryPercentage, 48, RA8875_GREEN);
    }

    //only fill in white if battery isn't full
    if (m_batteryPercentage < 100) {
      m_display.fillRect(579 + m_batteryPercentage, 11, 100 - m_batteryPercentage, 48, RA8875_WHITE);
    }
    updateBatteryPercentageDisplay();

    //draw charging symbol
    m_display.drawLine(629, 15, 624, 35, RA8875_BLACK);
    m_display.drawLine(624, 35, 634, 35, RA8875_BLACK);
    m_display.drawLine(634, 35, 629, 55, RA8875_BLACK);
  }
}

void Dashboard::checkLowBattery() {
  Serial.println("Checking for low battery");
  if (m_batteryPercentage <= LOW_BATT_THRESHOLD) {
    Serial.println("Low Battery!");
    m_warnings[LOW_BATTERY] = true;
    m_display.textMode();
    m_display.textTransparent(RA8875_RED);
    m_display.textEnlarge(0);
    char lowBatteryString[] = "Low Battery";
    m_display.textSetCursor(590, 160);
    m_display.textWrite(lowBatteryString);
  }
  else {
    //remove warning if no longer low on battery
    if (m_warnings[LOW_BATTERY]) {
      m_warnings[LOW_BATTERY] = false;
      m_display.graphicsMode();
      m_display.fillRect(590, 160, 150, 20, RA8875_WHITE);
    }
  }
}

void Dashboard::checkBatteryOverheat() {
  Serial.println("Checking for battery overheat");
  if (m_batteryTemperature > BATT_OVERHEAT_THRESHOLD) {
    Serial.println("Baterry Overheat!");
    m_warnings[BATTERY_OVERHEAT] = true;
    m_display.textMode();
    m_display.textTransparent(RA8875_RED);
    m_display.textEnlarge(0);
    char batteryOverheatString[] = "Battery Overheat";
    m_display.textSetCursor(590, 185);
    m_display.textWrite(batteryOverheatString);
  }
  else {
    //remove warning if battery's no longer overheating
    if (m_warnings[BATTERY_OVERHEAT]) {
      m_warnings[BATTERY_OVERHEAT] = false;
      m_display.graphicsMode();
      m_display.fillRect(590, 185, 160, 20, RA8875_WHITE);
    }
  }
}

void Dashboard::checkBatteryLowTemperature() {
  Serial.println("Checking for low battery temperature");
  if (m_batteryTemperature < BATT_LOW_TEMP_THRESHOLD) {
    Serial.println("Low Battery Temperature!");
    m_warnings[BATTERY_LOW_TEMPERATURE] = true;
    m_display.textMode();
    m_display.textTransparent(RA8875_RED);
    m_display.textEnlarge(0);
    char batteryLowTempString[] = "Low Battery Temperature";
    m_display.textSetCursor(590, 210);
    m_display.textWrite(batteryLowTempString);
  }
  else {
    //remove warning if battery's no longer at low temperature
    if (m_warnings[BATTERY_LOW_TEMPERATURE]) {
      m_warnings[BATTERY_LOW_TEMPERATURE] = false;
      m_display.graphicsMode();
      m_display.fillRect(590, 210, 185, 20, RA8875_WHITE);
    }
  }
}

void Dashboard::checkBatteryImbalance() {
  //TODO: implement the function to check for imbalanced charging
}

void Dashboard::updateBatteryVoltage() {
  Serial.println("Updating battery voltage");
  prevBatteryVoltage = m_batteryVoltage;
  m_batteryVoltage = battery.voltage() * BATT_MULTIPLIER;
}

void Dashboard::drawBatteryVoltageDisplay() {
  Serial.println("Drawing battery voltage display");

  //draw text
  m_display.textMode();
  m_display.textTransparent(RA8875_BLACK);
  m_display.textEnlarge(1);
  m_display.textSetCursor(50, 75);
  char batteryVoltageString[] = "Battery Voltage: ";
  m_display.textWrite(batteryVoltageString);
  m_display.textSetCursor(355, 116);
  char voltageString[] = "mV";
  m_display.textWrite(voltageString);

  //draw bar graph
  m_display.graphicsMode();
  m_display.drawRect(50, 120, 202, 25, RA8875_BLACK);
}

void Dashboard::updateBatteryVoltageDisplay() {
  Serial.println("Updating battery voltage text");

  //clear currently displayed battery voltage
  m_display.graphicsMode();
  m_display.fillRect(270, 116, 85, 30, RA8875_WHITE);
  //draw new battery voltage
  m_display.textMode();
  m_display.textTransparent(RA8875_BLACK);
  m_display.textEnlarge(1);
  m_display.textSetCursor(270, 116);
  char currentBatteryVoltageString[5];
  itoa(m_batteryVoltage, currentBatteryVoltageString, 10);
  m_display.textWrite(currentBatteryVoltageString);

  //clear bar graph
  m_display.graphicsMode();
  m_display.fillRect(51, 121, 200, 23, RA8875_WHITE);
  //convert voltage into a percentage between minimum and maximum voltage
  uint8_t voltagePercentage;
  if (m_batteryVoltage <= BATT_MIN_VOLTAGE) {
    voltagePercentage = 0;
    return;
  }
  else if (m_batteryVoltage >= BATT_MAX_VOLTAGE) {
    voltagePercentage = 100;
  }
  else {
    voltagePercentage = ((long)m_batteryVoltage - BATT_MIN_VOLTAGE) * 100 / (BATT_MAX_VOLTAGE - BATT_MIN_VOLTAGE);
  }
  //fill in bar graph (multiply percentage value by 2 to scale)
  m_display.fillRect(51, 121, voltagePercentage * 2, 23, RA8875_GREEN);
}

void Dashboard::drawBatteryTemperatureDisplay() {
  Serial.println("Drawing battery temperature display");

  //draw text
  m_display.textMode();
  m_display.textTransparent(RA8875_BLACK);
  m_display.textEnlarge(1);
  m_display.textSetCursor(50, 150);
  char batteryTemperatureString[] = "Battery Temperature: ";
  m_display.textWrite(batteryTemperatureString);
  m_display.textSetCursor(335, 191);
  char celsiusString[] = "C";
  m_display.textWrite(celsiusString);

  //draw bar graph
  m_display.graphicsMode();
  m_display.drawRect(50, 195, 202, 25, RA8875_BLACK);
}

void Dashboard::updateBatteryTemperatureDisplay() {
  Serial.println("Updating battery temperature display");

  //clear currently displayed battery temperature
  m_display.graphicsMode();
  m_display.fillRect(270, 191, 65, 30, RA8875_WHITE);
  //draw new battery temperature
  m_display.textMode();
  m_display.textTransparent(RA8875_BLACK);
  m_display.textEnlarge(1);
  m_display.textSetCursor(270, 191);
  char currentBatteryTemperatureString[3];
  itoa(m_batteryTemperature, currentBatteryTemperatureString, 10);
  m_display.textWrite(currentBatteryTemperatureString);

  //clear bar graph
  m_display.graphicsMode();
  m_display.fillRect(51, 196, 200, 23, RA8875_WHITE);

  int8_t temperature;
  uint16_t color = RA8875_GREEN;
  if (m_batteryTemperature <= BATT_MIN_TEMP) {
    temperature = BATT_MIN_TEMP;
  }
  else if (m_batteryTemperature >= BATT_MAX_TEMP) {
    temperature = BATT_MAX_TEMP;
  }
  else {
    temperature = m_batteryTemperature;
  }
  //change color to red if battery temperature is outside of operating range
  if (temperature > BATT_OVERHEAT_THRESHOLD || temperature < BATT_LOW_TEMP_THRESHOLD) {
    color = RA8875_RED;
  }

  //fill in bar graph
  if (temperature < 0) {
    if (temperature >= BATT_LOW_TEMP_THRESHOLD) {
      color = RA8875_CYAN;
    }
    m_display.fillRect(150 + temperature, 196, abs(temperature), 23, color);
  }
  else {
    m_display.fillRect(151, 196, temperature, 23, color);
  }
  //display line at 0 degree
  m_display.drawLine(150, 196, 150, 219, RA8875_BLACK);
}

void Dashboard::drawBatteryCurrentDisplay() {
  Serial.println("Drawing battery current display");

  //draw text
  m_display.textMode();
  m_display.textTransparent(RA8875_BLACK);
  m_display.textEnlarge(1);
  m_display.textSetCursor(50, 225);
  char batteryCurrentString[] = "Battery Current: ";
  m_display.textWrite(batteryCurrentString);
  m_display.textSetCursor(335, 266);
  char ampereString[] = "A";
  m_display.textWrite(ampereString);

  //draw bar graph
  m_display.graphicsMode();
  m_display.drawRect(50, 270, 202, 25, RA8875_BLACK);
}

void Dashboard::updateBatteryCurrentDisplay() {
  Serial.println("Updating battery current display");

  //clear currently displayed battery current
  m_display.graphicsMode();
  m_display.fillRect(270, 266, 65, 30, RA8875_WHITE);
  //draw new battery current
  m_display.textMode();
  m_display.textTransparent(RA8875_BLACK);
  m_display.textEnlarge(1);
  m_display.textSetCursor(270, 266);
  char currentBatteryCurrentString[2];
  itoa(m_batteryCurrent, currentBatteryCurrentString, 10);
  m_display.textWrite(currentBatteryCurrentString);

  //clear bar graph
  m_display.graphicsMode();
  m_display.fillRect(51, 271, 200, 23, RA8875_WHITE);

  int8_t current;
  uint16_t color = RA8875_RED;
  if (m_batteryCurrent <= BATT_MIN_CURRENT) {
    current = BATT_MIN_CURRENT * 2; //multiply current by 2 to scale
  }
  else if (m_batteryCurrent >= BATT_MAX_CURRENT) {
    current = BATT_MAX_CURRENT * 2;
  }
  else {
    current = m_batteryCurrent * 2;
    color = RA8875_GREEN;
  }

  //fill in bar graph
  if (current < 0) {
    m_display.fillRect(150 + current, 271, abs(current), 23, color);
  }
  else {
    m_display.fillRect(151, 271, current, 23, color);
  }
  //display line at 0 ampere
  m_display.drawLine(150, 271, 150, 294, RA8875_BLACK);
}

void Dashboard::updateBatteryPercentageDisplay() {
  Serial.println("Updating battery percentage text");
  //clear battery percentage
  m_display.fillRect(700, 10, 100, 50, RA8875_WHITE);

  //show battery percentage in number
  char currentBatteryPercentage[3];
  itoa(m_batteryPercentage, currentBatteryPercentage, 10);
  m_display.textMode();
  m_display.textSetCursor(700, 10);
  m_display.textTransparent(RA8875_BLACK);
  m_display.textEnlarge(2);
  m_display.textWrite(currentBatteryPercentage);
  char percentageSign[2] = "%";
  m_display.textWrite(percentageSign);
}

void Dashboard::updateLightState(uint8_t sensePin) {
  Serial.println("Updating light state");
  int lightState = digitalRead(sensePin);
  bool isLightOn = (lightState == HIGH) ? true : false;
  switch (sensePin) {
    case LEFT_LIGHT_SENSE_PIN: prevIsLeftOn = m_isLeftOn; m_isLeftOn = isLightOn; return;
    case RIGHT_LIGHT_SENSE_PIN: prevIsRightOn = m_isRightOn; m_isRightOn = isLightOn; return;
    case LO_LIGHT_SENSE_PIN: prevIsLoOn = m_isLoOn; m_isLoOn = isLightOn; return;
    case HI_LIGHT_SENSE_PIN: prevIsHiOn = m_isHiOn; m_isHiOn = isLightOn; return;
    default: Serial.println("Wrong sense pin input for light state!"); return;
  }
}

void Dashboard::updateLightsDisplay() {
  Serial.println("Updating lights display");
  m_display.graphicsMode();

  //left blinker
  if (m_isLeftOn != prevIsLeftOn) {
    if (m_isLeftOn) {
      m_display.fillRect(271, 371, 68, 68, RA8875_YELLOW);
    }
    else {
      m_display.fillRect(271, 371, 68, 68, RA8875_WHITE);
    }
    m_display.drawRect(270, 370, 70, 70, RA8875_BLACK);
    drawLeftLight();
  }

  //right blinker
  if (m_isRightOn != prevIsRightOn) {
    if (m_isRightOn) {
      m_display.fillRect(381, 371, 68, 68, RA8875_YELLOW);
    }
    else {
      m_display.fillRect(381, 371, 68, 68, RA8875_WHITE);
    }
    drawRightLight();
  }


  //lo
  if (m_isLoOn != prevIsLoOn) {
    if (m_isLoOn) {
      m_display.fillRect(161, 371, 68, 68, RA8875_YELLOW);
    }
    else {
      m_display.fillRect(161, 371, 68, 68, RA8875_WHITE);
    }
    drawLoLight();
  }

  //hi
  if (m_isHiOn != prevIsHiOn) {
    if (m_isHiOn) {
      m_display.fillRect(51, 371, 68, 68, RA8875_YELLOW);
    }
    else {
      m_display.fillRect(51, 371, 68, 68, RA8875_WHITE);
    }
    drawHiLight();
  }
}

void Dashboard::updateSpeedDisplay() {
  Serial.println("Updating speed display");
  m_display.graphicsMode();
  //clear previous speed
  m_display.fillRect(300, 200, 120, 60, RA8875_WHITE);
  m_display.textMode();
  char currentSpeed[3];
  itoa(m_speed, currentSpeed, 10);
  m_display.textSetCursor(300, 200);
  m_display.textTransparent(RA8875_BLACK);
  m_display.textEnlarge(3);
  m_display.textWrite(currentSpeed);
}

void Dashboard::countPulse() {
  currentSignalTime = micros();
  ++pulses;
}

void Dashboard::reset() {
  Serial.println("Resetting variables");
  for (auto warning : m_warnings) {
    warning = false;
  }

  m_isLeftOn = false;
  m_isRightOn = false;
  m_isLoOn = false;
  m_isHiOn = false;
  m_refVoltage = 0;
  m_batteryVoltage = 0;
  m_batteryCurrent = 0;
  m_batteryPercentage = 0;
  m_batteryTemperature = 0;
  m_speed = 0;
}

bool Dashboard::isCharging() {
  return m_isCharging;
}
