/*
 * CapacityTest.hpp
 */

#ifndef _CAPACITYTEST_HPP_
#define _CAPACITYTEST_HPP_

#include "ABC150Test.hpp"
#include "ABC150CANHandler.hpp"
#include "BatteryModuleCollection.hpp"
#include "PlateCANHandler.hpp"
#include "SingleChannelTest.hpp"
#include "AmpleLogger.hpp"

class CapacityTest : public SingleChannelTest {
  
public:
  CapacityTest(int _capacityWaitTime, ABC150CANHandler::Channel _channel,  ABC150CANHandler *_abc150Handler, PlateCANHandler *_plateCANHandler);
  /* ABC150Test virtual functions */
  bool startTest(BatteryModuleInfo *_bmInfo);
  bool stopTest(TestState testState);
  void loop();
  void printResult();
  enum class LocalState {CC, CV, Discharge, Recharge};

private:
  ABC150CANHandler *abc150Handler;
  PlateCANHandler *plateHandler;
  BatteryModuleCollection &collection;
  BatteryModuleInfo *bmInfo;
  int capacityWaitTime;
  int64_t espDischargeStartTime;
  int64_t espDischargeEndTime;
  uint64_t abcDischargeStartTime;
  uint64_t abcDischargeEndTime;
  float capacity;
  double energy;
  int64_t lastLoopTime;
  LocalState localState;

};

#endif /* _CAPACITYTEST_HPP_ */
