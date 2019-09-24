/*
 * ABC150TestManager.hpp
 */

#ifndef _ABC150TESTMANAGER_HPP_
#define _ABC150TESTMANAGER_HPP_

#include "ABC150Test.hpp"
#include "ABC150Controller.hpp"
#include "BatteryModuleCollection.hpp"
#include "SingleChannelTest.hpp"
#include "DualChannelTest.hpp"
#include "freertos/timers.h"
#include "assert.h"

class ABC150TestManager {

public:

  enum class TestType {Single, Dual};

  ABC150TestManager(ABC150Controller &_abc150Controller);
  static void debugLog(TimerHandle_t xTimer);
  void debugToggle();
  virtual void addSingleChannelTest(SingleChannelTest *singleTest);
  virtual void addDualChannelTest(DualChannelTest *dualTest);
  const char* getTestStateName(ABC150Test::TestState testState);
  bool testCheck(TestType type, int test);
  bool checkCycleFlag(TestType type, int test);
  bool checkCDFlag(TestType type, int test);
  void listAllTests();
  uint64_t getRunningTime(TestType type, int test);
  void listTestsByType(TestType type);
  bool runSingleTest(int test, int cycles = 1);
  bool runDualTest(int test, int cycles = 1);
  bool stopSingleTest(int test);
  bool stopDualTest(int test);
  void stopAll();
  void stopAllOverride();
  bool setDestinationVoltage(TestType type, int test, float voltage);
  void printInfo();
  void setBMAmpleID(int ch, unsigned int ID);
  /* Loop task */
  static void loopTaskWrapper(void *arg);
  void loopTask();
  vector<SingleChannelTest*> singleTestVec;
  vector<DualChannelTest*> dualTestVec;

private:
  ABC150Controller &abc150Controller;
  PlateCANHandler *plateHandler;
  ABC150CANHandler *abc150Handler;
  BatteryModuleCollection &collection;
  unsigned int bmAmpleID[2];
  bool debugLogEnable;
  TaskHandle_t loopTaskHandle;
  TickType_t xLastWakeTime;
  const char* TAG = "ABC150TestManager";
};

class ABC150TestUserInterface {

private:
  static void help();

public:
  static void ABC150TestInterface(AmpleSerial &pc, ABC150TestManager *testManager);

};



#endif /* COMPONENTS_ABC150_INCLUDE_ABC150TESTMANAGER_HPP_ */
