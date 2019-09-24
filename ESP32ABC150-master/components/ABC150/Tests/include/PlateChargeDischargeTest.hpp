/*
 * PlateChargeDischargeTest.hpp
 */

#ifndef _PLATECHARGEDDISCHARGE_HPP_
#define _PLATECHARGEDDISCHARGE_HPP_

//#include "ABC150Test.hpp"
#include "DualChannelTest.hpp"
#include "ABC150CANHandler.hpp"
#include "BatteryModuleCollection.hpp"
#include "PlateCANHandler.hpp"
#include "PCAL6416a.hpp"
#include "AmpleLogger.hpp"


class PlateChargeDischargeTest : public DualChannelTest {

public:
PlateChargeDischargeTest(ABC150CANHandler *_abc150Handler, PlateCANHandler *_plateCANHandler);
PlateChargeDischargeTest();
  /* ABC150Test virtual functions */
  bool startTest();
  bool stopTest(TestState testState);
  void loop();
  void printResult();
  void loopPlateTask();
  static void loopPlateTaskWrapper(void *arg);

private:
  ABC150CANHandler *abc150Handler;
  PlateCANHandler *plateHandler;
  BatteryModuleCollection &collection;
  TaskHandle_t loopPlateTaskHandle;
  TickType_t xLastWakeTimePlate;
  PCAL6416a *pcal6416a;
  bool charging;
  int bmCount;
};




#endif /* _PLATECHARGEDISCHARGETEST_HPP_ */
