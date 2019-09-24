/*
 * SingleChannelTest.hpp
 */

#ifndef _SINGLECHANNELTEST_HPP_
#define _SINGLECHANNELTEST_HPP_

#include "ABC150Test.hpp"
#include "BatteryModuleCollection.hpp"
#include "ABC150CANHandler.hpp"
#include "PlateCANHandler.hpp"


class SingleChannelTest : public ABC150Test {
public:
  SingleChannelTest(ABC150CANHandler::Channel _channel, ABC150CANHandler *_abc150Handler, PlateCANHandler *_plateCANHandler);
  ABC150CANHandler::Channel getChannel();
  bool preTestChecks(BatteryModuleInfo* bmInfo);
  bool loopCheck(BatteryModuleInfo* bmInfo);
  virtual bool startTest(BatteryModuleInfo *_bmInfo) = 0;
  virtual bool stopTest(TestState testState) = 0;
  virtual void loop() = 0;
  virtual void printResult() = 0;
  static const char* getChannelName(ABC150CANHandler::Channel channel);

private:
  PlateCANHandler *plateHandler;
  ABC150CANHandler *abc150Handler;
  BatteryModuleCollection &collection;

protected:
  ABC150CANHandler::Channel channel;

};

#endif /* _SINGLECHANNELTEST_HPP_ */
