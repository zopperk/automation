/*
 * ABC150Controller.cpp
 */


#include "ABC150Controller.hpp"
#include "NVSConfig.hpp"
#include "AmpleLogger.hpp"


#define BATT_CAN_FREQUENCY CAN_SPEED_500KBPS
#define ABC150_CAN_FREQUENCY CAN_SPEED_250KBPS


ABC150Controller::ABC150Controller(unsigned int _ampleID,
                                   gpio_num_t _batteryControlPin) :
                                    ampleID(_ampleID),
                                    mcp2515CAN(ABC150_CAN_FREQUENCY, NVSConfig::getInt(MCP2515_CAN_ENABLE)),
                                    ABC150CAN(mcp2515CAN),
                                    abc150CANHandler(ABC150CAN),
                                    esp32CAN(BATT_CAN_FREQUENCY, GPIO_NUM_25, GPIO_NUM_35, NVSConfig::getInt(ESP32_CAN_ENABLE)),
                                    plateCAN(esp32CAN),
                                    plateCANHandler(plateCAN, ampleID, NVSConfig::getInt(MIN_CAR_OPERATING_VOLTAGE), NVSConfig::getInt(MAX_CAR_OPERATING_VOLTAGE), NULL, true),
                                    batteryControlPin(_batteryControlPin),
                                    bat12vEnabled(false){

  gpio_pad_select_gpio(batteryControlPin);
  gpio_set_direction(batteryControlPin, GPIO_MODE_OUTPUT);
  gpio_set_level(batteryControlPin, 0);

}


ABC150CANHandler* ABC150Controller::getABC150CANHandler() {
  return &abc150CANHandler;
}

PlateCANHandler* ABC150Controller::getPlateCANHandler() {
  return &plateCANHandler;
}


void ABC150Controller::turn12vOn() {
  printf("Turning 12v ON\r\n");
  gpio_set_level(batteryControlPin, 1);
  bat12vEnabled = true;
  AmpleLogger::getBatteryModuleLogger()->log12v(true);
}

void ABC150Controller::turn12vOff() {
  printf("Turning 12v OFF\r\n");
  gpio_set_level(batteryControlPin, 0);
  bat12vEnabled = false;
  AmpleLogger::getBatteryModuleLogger()->log12v(false);
}
