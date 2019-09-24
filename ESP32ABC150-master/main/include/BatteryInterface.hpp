/*
 * BatteryInterface.hpp
 */

#ifndef _BATTERYINTERFACE_HPP_
#define _BATTERYINTERFACE_HPP_

#include "AmpleSerial.hpp"
#include "ABC150Controller.hpp"

void manageBattery(ABC150Controller &controller, AmpleSerial &pc);


#endif /* _BATTERYINTERFACE_HPP_ */
