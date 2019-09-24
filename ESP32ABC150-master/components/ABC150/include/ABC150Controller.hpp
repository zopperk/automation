/*
 * ABC150Controller.hpp
 */

#ifndef _ABC150CONTROLLER_HPP_
#define _ABC150CONTROLLER_HPP_

#include "ESP32_CAN.hpp"
#include "PlateCANHandler.hpp"
#include "MCP2515_CAN.hpp"
#include "ABC150CANHandler.hpp"
#include "driver/gpio.h"


class ABC150Controller {
public:
  ABC150Controller(unsigned int _ampleID,
                   gpio_num_t _batteryControlPin = GPIO_NUM_26);
  ABC150CANHandler* getABC150CANHandler();
  PlateCANHandler* getPlateCANHandler();

  void turn12vOn();
  void turn12vOff();

private:

  unsigned int ampleID;

  MCP2515_CAN mcp2515CAN;
  AmpleCAN ABC150CAN;
  ABC150CANHandler abc150CANHandler;

  ESP32_CAN esp32CAN;
  AmpleCAN plateCAN;
  PlateCANHandler plateCANHandler;
  gpio_num_t batteryControlPin;
  bool bat12vEnabled;

};






#endif /* _ABC150CONTROLLER_HPP_ */
