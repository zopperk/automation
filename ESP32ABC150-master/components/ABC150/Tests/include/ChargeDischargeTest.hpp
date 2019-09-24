/*
 * ChargeDischarge.hpp
 */

#ifndef _CHARGEDISCHARGE_HPP_
#define _CHARGEDISCHARGE_HPP_

#include "ABC150Test.hpp"
#include "ABC150CANHandler.hpp"
#include "BatteryModuleCollection.hpp"
#include "PlateCANHandler.hpp"
#include "SingleChannelTest.hpp"
#include "AmpleLogger.hpp"

class ChargeDischargeTest : public SingleChannelTest {
public:
  ChargeDischargeTest(ABC150CANHandler::Channel _channel, ABC150CANHandler *_abc150Handler, PlateCANHandler *_plateCANHandler);
  /* ABC150Test virtual functions */
  bool startTest(BatteryModuleInfo *_bmInfo);
  bool stopTest(TestState testState);
  void loop();
  void printResult();

private:
  ABC150CANHandler *abc150Handler;
  PlateCANHandler *plateHandler;
  BatteryModuleCollection &collection;
  BatteryModuleInfo *bmInfo;
  bool charging;
};

#endif /* _CHARGEDISCHARGE_HPP_ */
