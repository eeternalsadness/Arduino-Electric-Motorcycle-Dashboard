/*
  This is the library for the dashboard. The library includes functions that take inputs
  from the sensors to update the internal values and output them to the touch screen.
*/

#ifndef DASHBOARD_H
#define DASHBOARD_H

#include <Arduino.h>
#include <EEPROM.h>
#include <VoltageReference.h>
#include <Battery.h>
#include <Adafruit_RA8875.h>
#include <PinChangeInterrupt.h>

//sets the storage area of the calibrated microcontroller voltage to the very end of the EEPROM
#define VREF_EEPROM_ADDR (E2END - 2)
//pin setup for the touch functionality
#define RA8875_INT 3 //interrupt
//voltage divider ratio for the sensing circuit
#define DIVIDER_RATIO 4.0
//value of the voltage divider used for the battery feeding the arduino
//multiply the voltage reading by this value to get the battery voltage
#define BATT_MULTIPLIER 1

//list of analog sense pins
#define BATT_VOLTAGE_SENSE_PIN A1 // pin for sensing battery voltage (analog A0)
#define BATT_TEMP_SENSE_PIN A2 //pin for sensing battery temperature (analog A1)
#define BATT_CURRENT_SENSE_PIN A3 //pin for sensing battery current (analog A2)

//list of warnings
enum Warnings {
  LOW_BATTERY,
  BATTERY_OVERHEAT,
  BATTERY_LOW_TEMPERATURE,
  BATTERY_IMBALANCE,
};

//list of digital sense pins
enum DigitalSensePins {
  LEFT_LIGHT_SENSE_PIN = 24,
  RIGHT_LIGHT_SENSE_PIN = 26,
  LO_LIGHT_SENSE_PIN = 28,
  HI_LIGHT_SENSE_PIN = 30,
  SPEED_SENSE_PIN = 11,
  CHARGE_SENSE_PIN = 22,
};

enum Constants {
  LOW_BATT_THRESHOLD = 20,
  BATT_OVERHEAT_THRESHOLD = 60, //battery overheat threshold in degrees celsius
  BATT_LOW_TEMP_THRESHOLD = -20, //battery low temperature threshold in degrees celsius
  //battery percentage error due to fluctuation in voltage
  //this is to reduce flickering caused by constantly updating the battery percentage display
  BATT_PERCENT_ERROR = 2,
  BATT_MIN_TEMP = -100, //minimum battery temperature in degrees celsius that can be displayed
  BATT_MAX_TEMP = 100, //maximum battery temperature in degrees celsius that can be displayed
  BATT_MIN_VOLTAGE = 9000, //battery minimum voltage after voltage divider in millivolts
  BATT_MAX_VOLTAGE = 12000, //battery maximum voltage after voltage divider in millivolts
  BATT_MIN_CURRENT = -50, //minimum battery current in amperes that can be displayed
  BATT_MAX_CURRENT = 50, //maximum battery current in amperes that can be displayed
  WHEEL_DIAMETER_INCHES = 1, //diameter of the motorcycle's wheel in inches
  MAX_SPEED = 120, //maximum speed in mph
};

class Dashboard {

  private:
    Adafruit_RA8875 m_display;
    bool m_warnings[4];
    bool m_isCharging;
    bool m_isDisplayingBattInfo;
    uint16_t m_refVoltage; //board's reference voltage ~5V
    uint16_t m_batteryVoltage; //battery voltage in millivolts
    int8_t m_batteryCurrent; //battery current in amperes
    uint8_t m_batteryPercentage;
    int8_t m_batteryTemperature; //battery temperature in degrees Celsius
    uint8_t m_speed; //speed of the motorcycle in mph
    float m_xScale; //x scale for touch events
    float m_yScale; //y scale for touch events

    //bool m_isBalanced;
    //TODO: Have an array to store the voltages and temperatures of different cells to measure imbalance while charging

    //lights' states
    bool m_isLeftOn;
    bool m_isRightOn;
    bool m_isHiOn;
    bool m_isLoOn;

    //General purpose functions
    void initDashboard();
    /*
       Resets all member variables except m_isCharging. Use when changing charging states
    */
    void resetVariables();

    //TODO: Refactor the helper functions to files (i.e Lights, Speed, etc.)

    //Helper functions that draw elements onto the display
    void drawSpeedIndicator();
    void drawBatteryOutline();
    void drawLightIndicators();
    void drawLeftLight();
    void drawRightLight();
    void drawLoLight();
    void drawHiLight();
    void drawWarningBox();
    void drawBatteryVoltageDisplay();
    void drawBatteryTemperatureDisplay();
    void drawBatteryCurrentDisplay();
    void drawButton();

    //Helper functions to check for warnings
    void checkLowBatteryWarning();
    void checkBatteryOverheatWarning();
    void checkBatteryLowTemperatureWarning();
    void checkBatteryImbalanceWarning();

    //Helper functions to update internal values
    /*
      Returns true when there's a change in charging state, otherwise returns false
    */
    bool updateChargingState();
    void updateBatteryVoltage();
    /*
      @param sensePin is the sense pin for the light that will get its state updated
    */
    void updateLightState(uint8_t sensePin);

    //Helper functions to update display elements
    void updateBatteryTemperatureDisplay();
    void updateBatteryVoltageDisplay();
    void updateBatteryPercentageDisplay();
    void updateBatteryCurrentDisplay();
    void updateBatteryDisplay();
    void updateSpeedDisplay();
    void updateLightsDisplay();
    void updateWarningsDisplay();
    void updateLowBatteryDisplay();
    void updateBatteryOverheatDisplay();
    void updateBatteryLowTemperatureDisplay();
    void updateBatteryImbalanceDisplay();

    //Function to call when the interrupt happens to count the number of revolutions the wheel has made
    static void countPulse();

    bool isCharging();
    bool isDisplayingBattInfo();

  public:
    /*
      Creates an instance of the dashboard that displays critical values and warnings for the motorcycle
      @param tft is the display object that is used for the dashboard
    */
    Dashboard(Adafruit_RA8875 tft);
    void begin();
    void checkTouch();
    void updateWarnings();
    void updateDashboardDisplay();
    void updateBatteryTemperature();
    void updateBatteryPercentage();
    void updateBatteryCurrent();
    void updateSpeed();
    void updateLightStates();
};

#endif