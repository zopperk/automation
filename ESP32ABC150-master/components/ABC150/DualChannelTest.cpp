#include "DualChannelTest.hpp"
#include "esp_log.h"
#include "esp_task_wdt.h"


DualChannelTest::DualChannelTest(ABC150CANHandler *_abc150Handler, PlateCANHandler *_plateHandler):
                  plateHandler(_plateHandler),
                  abc150Handler(_abc150Handler),
                  collection(BatteryModuleCollection::collection()),
                  onlineCount(0){
                  TAG = "DualChannelTest";
                  }

bool DualChannelTest::preTestChecks() {
  if (!abc150Handler->isDetected()) {
    ESP_LOGE(TAG, "ABC150 not detected");
    return false;
  }
  /* Loop through BMs, check for errors*/
  int bmCount = collection.count();
  BatteryModuleInfo *bms[bmCount];
  bmCount = collection.getAllBatteryModules(bms, bmCount);
  onlineCount = 0;
  for (int i = 0; i < bmCount; i++) {
    if (collection.isBMErrors(bms[i])) {
      if (bms[i]->online) {
        plateHandler->setBMState(bms[i]->batteryID, false);
      }
      ESP_LOGW(TAG, "BM %d has an error and will not go online.\n", bms[i]->batteryID);
    } else {
      plateHandler->setBMState(bms[i]->batteryID, true);
      onlineCount++;
    }
  }
  if (onlineCount<1) {
    ESP_LOGE(TAG, "There are 0 BMs online.");
    return false;
  }
  return true;
}

bool DualChannelTest::loopCheck(){
    /*Loop through online BMs, check for errors*/
    int bmCount = collection.count();
    BatteryModuleInfo *bms[bmCount];
    bmCount = collection.getAllBatteryModules(bms, bmCount);
    for (int i = 0; i < bmCount; i++) {
      if (bms[i] -> online) {
        if (collection.isBMErrors(bms[i]) || !bms[i]->HVOn)  {
          ESP_LOGE(TAG, "BM %d has an error.", bms[i]->batteryID);
          stopTest(TestState::Failed);
          return false;
        }
      }
    }
    if (abc150Handler->getConverterStatus(ABC150CANHandler::A) != ABC150CANHandler::ConverterStatus::Remote) {
      ESP_LOGE(TAG, "Not in remote mode.");
      stopTest(TestState::Failed);
      return false;
    }
    if (abc150Handler->getControlModeOut(ABC150CANHandler::A) != ABC150CANHandler::ControlMode::Power) {
      ESP_LOGE(TAG, "Not in power control mode.");
      stopTest(TestState::Failed);
      return false;
    }
    if (abc150Handler->getLoadModeOut(ABC150CANHandler::A) != ABC150CANHandler::LoadMode::Parallel) {
      ESP_LOGE(TAG,"No in parallel load mode.");
      stopTest(TestState::Failed);
      return false;
    }
    return true;
}
   