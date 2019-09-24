
#include "SingleChannelTest.hpp"
#include "esp_log.h"
#include "esp_task_wdt.h"


SingleChannelTest::SingleChannelTest(ABC150CANHandler::Channel _channel, ABC150CANHandler *_abc150Handler, PlateCANHandler *_plateHandler):
                  plateHandler(_plateHandler),
                  abc150Handler(_abc150Handler),
                  collection(BatteryModuleCollection::collection()),
                  channel(_channel){
                  TAG = "SingleChannelTest";
                  }

ABC150CANHandler::Channel SingleChannelTest::getChannel() {
  return channel;
}

bool SingleChannelTest::preTestChecks(BatteryModuleInfo* bmInfo){
  if (bmInfo == NULL) {
    ESP_LOGE(TAG, "NULL bmInfo");
    return false;
  }
  if (!abc150Handler->isDetected()) {
    ESP_LOGE(TAG, "ABC150 not detected");
    return false;
  }
  return true;
}

bool SingleChannelTest::loopCheck(BatteryModuleInfo* bmInfo) {
  if (!bmInfo->online || !bmInfo->HVOn || !bmInfo->used) {
    ESP_LOGE(TAG, "BM state error");
    return false;
  }
  if (abc150Handler->getConverterStatus(channel) != ABC150CANHandler::ConverterStatus::Remote) {
    ESP_LOGE(TAG, "Not in remote mode.");
    return false;
  }
  if (abc150Handler->getLoadModeOut(channel) != ABC150CANHandler::LoadMode::Independent) {
    ESP_LOGE(TAG,"No in independent load mode.");
    return false;
  }
  return true;
}

const char* SingleChannelTest::getChannelName(ABC150CANHandler::Channel channel) {
  switch(channel) {

    case ABC150CANHandler::Channel::A:
      return "A";

    case ABC150CANHandler::Channel::B:
      return "B";

    default:
      return "Unknown";
  }
}
