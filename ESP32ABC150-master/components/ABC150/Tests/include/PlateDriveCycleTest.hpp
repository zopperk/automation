/*
 * PlateDriveCycleTest.hpp
 */

#ifndef _PLATEDRIVECYCLETEST_HPP_
#define _PLATEDRIVECYCLETEST_HPP_

#include "ABC150Test.hpp"
#include "ABC150CANHandler.hpp"
#include "BatteryModuleCollection.hpp"
#include "PlateCANHandler.hpp"
#include "PCAL6416a.hpp"
#include "DualChannelTest.hpp"
#include "AmpleLogger.hpp"
#include "ABC150TestManager.hpp"

class PlateDriveCycleTest : public DualChannelTest {

public:
  PlateDriveCycleTest(int _driveCycleWaitTime, ABC150CANHandler *_abc150Handler, PlateCANHandler *_plateCANHandler);
  /* ABC150Test virtual functions */
  bool startTest();
  bool stopTest(TestState testState);
  void loop();
  void loopPlate();
  void printResult();
  void loopPlateTask();
  static void loopPlateTaskWrapper(void *arg);

private:
  ABC150CANHandler *abc150Handler;
  PlateCANHandler *plateHandler;
  TestLogger *logger;
  BatteryModuleCollection &collection;
  int driveCycleWaitTime;
  TaskHandle_t loopPlateTaskHandle;
  TickType_t xLastWakeTimePlate;
  PCAL6416a *pcal6416a;
  int getTimeDelta();
  float timeDelta;

};

#endif /* _PLATEDRIVECYCLETEST_HPP_ */
