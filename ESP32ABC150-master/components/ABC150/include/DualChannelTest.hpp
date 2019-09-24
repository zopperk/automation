
#ifndef _DUALCHANNELTEST_HPP_
#define _DUALCHANNELTEST_HPP_

#include "ABC150Test.hpp"
#include "ABC150CANHandler.hpp"
#include "BatteryModuleCollection.hpp"
#include "PlateCANHandler.hpp"
#include "ABC150Controller.hpp"


class PlateChargeDischargeTest;

class DualChannelTest : public ABC150Test {
public:
  DualChannelTest(ABC150CANHandler *_abc150Handler, PlateCANHandler *_plateCANHandler);
  bool preTestChecks();
  bool loopCheck();
  virtual bool startTest() = 0;
  virtual bool stopTest(TestState testState) = 0;
  virtual void loop() = 0;
  virtual void printResult() = 0;

private:
  PlateCANHandler *plateHandler;
  ABC150CANHandler *abc150Handler;
  BatteryModuleCollection &collection;

protected:
  int onlineCount;




};

#endif /* _DUALCHANNELTEST_HPP_ */