/*
 * PulseTest.hpp
 */

#ifndef _PULSETEST_HPP_
#define _PULSETEST_HPP_

//#include "ABC150Test.hpp"
#include "SingleChannelTest.hpp"
#include "ABC150CANHandler.hpp"
#include "BatteryModuleCollection.hpp"
#include "PlateCANHandler.hpp"
#include "AmpleLogger.hpp"

class PulseTest: public SingleChannelTest {

public:
  PulseTest(int _pulseWaitTime, ABC150CANHandler::Channel _channel, ABC150CANHandler *_abc150Handler, PlateCANHandler *_plateCANHandler);
  /* ABC150Test virtual functions */
  bool startTest(BatteryModuleInfo *_bmInfo);
  bool stopTest(TestState testState);
  static void restartTest(TimerHandle_t xTimer);
  void loop();
  void printResult();

  void printCellVoltages(float *cellVoltages);
  void printDCR();

private:
  ABC150CANHandler *abc150Handler;
  PlateCANHandler *plateHandler;
  BatteryModuleCollection &collection;
  BatteryModuleInfo *bmInfo;
  int pulseWaitTime;
  float cellVoltagesInitial[NUM_CELLS];
  float cellVoltagesFinal[NUM_CELLS];
  float bmVoltageInitial;
  float bmVoltageFinal;
  TimerHandle_t prechargeOffTimer;

};

#endif /* _PULSETEST_HPP_ */
