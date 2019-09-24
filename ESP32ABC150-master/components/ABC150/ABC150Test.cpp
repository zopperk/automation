/*
 * ABC150Test.cpp
 */
#include "ABC150Test.hpp"
#include <sstream>

ABC150Test::TestState ABC150Test::getTestState(){
  return state;
}

const char* ABC150Test::getTestName(){
  return TAG;
}

uint64_t ABC150Test::getRunningTime(){
  return (stopTime - startTime);
}

void ABC150Test::setDestinationVoltage(float voltage){
  destinationVoltage = voltage;
}

void ABC150Test::setCycles(int cyclesNum) {
  cycles = cyclesNum;
}

bool ABC150Test::getCycleFlag() {
  return cycleFlag;
}

bool ABC150Test::getCDFlag() {
  return cDFlag;
}

void ABC150Test::printAndSaveResult(std::stringstream &result) {
  std::string res = result.str();
  result.str("");
  std::cout << res;
  resultQueue.push(res);
}

void ABC150Test::printAllResults() {
  while (resultQueue.size()) {
    cout << resultQueue.front();
    resultQueue.pop();
  }
}


