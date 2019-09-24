/*
 * ABC150Test.hpp
 */

#ifndef _ABC150TEST_HPP_
#define _ABC150TEST_HPP_

#include "BatteryModuleCollection.hpp"
#include "esp_log.h"
#include <queue>

class ABC150Test {

public:

  enum class TestState {Idle, Running, Success, Failed, Restart};

  virtual bool stopTest(TestState testState) = 0;
  ABC150Test::TestState getTestState();
  const char* getTestName();
  virtual void loop() = 0;
  virtual void printResult() = 0;
  uint64_t getRunningTime();
  void setDestinationVoltage(float voltage);
  void setCycles(int cyclesNum);
  bool getCycleFlag();
  bool getCDFlag();
  void printAndSaveResult(std::stringstream &result);
  void printAllResults();

protected:
  uint64_t startTime = 0;
  uint64_t stopTime = 0;
  uint64_t startWait = 0;
  uint64_t stopWait = 0;
  float destinationVoltage = 0;
  ABC150Test::TestState state = TestState::Idle;
  const char* TAG;
  int cycles = 1;
  bool cycleFlag = true;
  bool cDFlag = false;
  std::queue<std::string> resultQueue;
  SemaphoreHandle_t startMutex = xSemaphoreCreateMutex();
  SemaphoreHandle_t stopMutex = xSemaphoreCreateMutex();

};



#endif /* _ABC150TEST_HPP_ */
