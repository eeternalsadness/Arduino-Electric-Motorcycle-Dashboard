#include <SPI.h>
#include "Dashboard.h"

//pin setup for display board
#define RA8875_INT 3 //interrupt
#define RA8875_CS 10 //CS
#define RA8875_RESET 9 //reset

//create display object
Adafruit_RA8875 tft(RA8875_CS, RA8875_RESET);

Dashboard dashboard = Dashboard(tft);

void setup() {
  Serial.begin(9600);
  Serial.println("Starting");

  dashboard.begin();
}

void loop() {
  //update battery percentage
  dashboard.updateBatteryPercentage();

  //update battery temperature
  dashboard.updateBatteryTemperature();

  //update battery current
  dashboard.updateBatteryCurrent();
  
  //update light states
  dashboard.updateLightStates();

  //update speed
  dashboard.updateSpeed();

  //update warnings
  dashboard.updateWarnings();

  //update dashboard visuals
  dashboard.updateDashboard();
}
