/*
 * CapacityTest.cpp
 */

#include "CapacityTest.hpp"
#include "TimeUtils.hpp"
#include "esp_log.h"
#include <sstream>

#define DISCHARGE_CURRENT   6.0


CapacityTest::CapacityTest(int _capacityWaitTime, ABC150CANHandler::Channel _channel, ABC150CANHandler *_abc150Handler, PlateCANHandler *_plateHandler):
                     SingleChannelTest(_channel, _abc150Handler, _plateHandler),
                     abc150Handler(_abc150Handler),
                     plateHandler(_plateHandler),
                     collection(BatteryModuleCollection::collection()),
                     bmInfo(NULL),
                     capacityWaitTime(_capacityWaitTime),
                     espDischargeStartTime(0),
                     espDischargeEndTime(0),
                     abcDischargeStartTime(0),
                     abcDischargeEndTime(0),
                     capacity(0),
                     energy(0),
                     lastLoopTime(0),
                     localState(LocalState::CC){
  TAG = "CapacityTest";
  assert(startMutex != NULL);
  assert(stopMutex != NULL);
  }

bool CapacityTest::startTest(BatteryModuleInfo *_bmInfo) {
  xSemaphoreTake(startMutex, portMAX_DELAY);
  if (!preTestChecks(_bmInfo)) {
    xSemaphoreGive(startMutex);
    return false;
  }

  if (state == TestState::Running) {
    ESP_LOGE(TAG, "Already running");
    xSemaphoreGive(startMutex);
    return false;
  }

  if (collection.isBMCriticalErrors(_bmInfo) || collection.isBMPlateErrors(_bmInfo) ||
      _bmInfo->shortCircuitError ||
      _bmInfo->voltageDiffError ||
      _bmInfo->busVoltageDiffError ||
      _bmInfo->tempError ||
      _bmInfo->currentError ||
      _bmInfo->tempSensingFailure ||
      _bmInfo->voltageSensingFailure ||
      _bmInfo->FETFailure ||
      _bmInfo->otherHardwareFailure) {
    ESP_LOGE(TAG, "BM error");
    xSemaphoreGive(startMutex);
    return false;
  }

  bmInfo = _bmInfo;

  /* Set limits */
  abc150Handler->setLowerVoltageLimit(channel, 240);
  abc150Handler->setLowerCurrentLimit(channel, -6);
  abc150Handler->setLowerPowerLimit(channel, -2460);
  abc150Handler->setUpperVoltageLimit(channel, 406);
  abc150Handler->setUpperCurrentLimit(channel, 6);
  abc150Handler->setUpperPowerLimit(channel, 2460);

  abc150Handler->setCurrent(channel, 6);
  plateHandler->setBMState(bmInfo->batteryID, true);
  plateHandler->HVOn(bmInfo->batteryID);
  abc150Handler->takeControl(channel);
  vTaskDelay(pdMS_TO_TICKS(2000));
  if (abc150Handler->getConverterStatus(channel) != ABC150CANHandler::ConverterStatus::Remote) {
    ESP_LOGE(TAG, "Not in remote mode, taking control again.");
    abc150Handler->takeControl(channel);
    vTaskDelay(pdMS_TO_TICKS(2000));
  }
  state = TestState::Running;
  startTime = TimeUtils::esp_timer_get_time_ms();
  AmpleLogger::getTestLogger()->logStartTime("CapacityTest");
  localState = LocalState::CC;
  abc150Handler->enable(channel);
  xSemaphoreGive(startMutex);
  return true;

}

bool CapacityTest::stopTest(TestState testState) {
  xSemaphoreTake(stopMutex, portMAX_DELAY);
  /* If the user stops the test in between cycles */
  if (testState == TestState::Idle && state == TestState::Restart) {
    state = TestState::Idle;
    ESP_LOGI(TAG, "Idle");
    xSemaphoreGive(stopMutex);
    return true;
  }
  abc150Handler->disable(channel);
  vTaskDelay(pdMS_TO_TICKS(500));
  abc150Handler->releaseControl(channel);
  plateHandler->HVOff(bmInfo->batteryID);
  cycles--;
  stopTime = TimeUtils::esp_timer_get_time_ms();
  AmpleLogger::getTestLogger()->logEndTime("CapacityTest");
  /* A cycle finishes */
  if (testState == TestState::Success) {
    /* Multiple cycles */
    if (cycles > 0) {
      ESP_LOGI(TAG, "Cycle %d done", cycles);
      state = ABC150Test::TestState::Restart;
      startWait = TimeUtils::esp_timer_get_time_ms();
      ESP_LOGI(TAG, "Restart");
    } else {
      if (state == TestState::Running) state = TestState::Success;
      ESP_LOGI(TAG, "Success");
      printAllResults();
    }
  /* A cycle finishes */
  } else if (testState == TestState::Failed) {
    state = TestState::Failed;
    ESP_LOGI(TAG, "Failed");
    printAllResults();
  /* User stops test */
  } else if (testState == TestState::Idle) {
    state = TestState::Idle;
    ESP_LOGI(TAG, "Idle");
    printAllResults();
  }
  xSemaphoreGive(stopMutex);
  return true;
}

void CapacityTest::printResult() {
  const char* channelName = getChannelName(channel);
  std::stringstream s;
  s << "Channel: " << channelName << std::endl;
  printAndSaveResult(s);
  s << "ESP: StartTime: " << espDischargeStartTime << "\tEndTime: " << espDischargeEndTime << std::endl;
  printAndSaveResult(s);
  s << "ABC: StartTime: " << abcDischargeStartTime << "\tEndTime: " << abcDischargeEndTime << std::endl;
  printAndSaveResult(s);
  s << "Capacity (esp32 Timer): " << (espDischargeEndTime - espDischargeStartTime) * DISCHARGE_CURRENT / 1000.0 / 3600.0 << "Ah" << std::endl;
  printAndSaveResult(s);
  s << "Capacity (abc Timer): " << (abcDischargeEndTime - abcDischargeStartTime) * DISCHARGE_CURRENT / 1000.0 / 3600.0 << "Ah" << std::endl;
  printAndSaveResult(s);
  s << "Energy: " << energy / 1000.0 / 3600.0 << std::endl;
  printAndSaveResult(s);
}

void CapacityTest::loop() {
  int64_t currentTime;
   if (state == TestState::Running) {

     if(loopCheck(bmInfo) == false) {
       stopTest(TestState::Failed);
       return;
     }

     if (collection.isBMCriticalErrors(bmInfo) || collection.isBMPlateErrors(bmInfo) ||
         bmInfo->shortCircuitError ||
         bmInfo->voltageDiffError ||
         bmInfo->busVoltageDiffError ||
         bmInfo->tempError ||
         bmInfo->currentError ||
         bmInfo->tempSensingFailure ||
         bmInfo->voltageSensingFailure ||
         bmInfo->FETFailure ||
         bmInfo->otherHardwareFailure) {
       ESP_LOGE(TAG, "BM error");
       stopTest(TestState::Failed);
       return;
     }

     /* Check if we need to stop CC mode */
     if ((localState == LocalState::CC) && (bmInfo->maxCellVoltage >= 4.2)) {
       ESP_LOGI(TAG, "CC done, Setting voltage to %f", abc150Handler->getVoltage(channel));
       abc150Handler->setVoltage(channel, abc150Handler->getVoltage(channel));
       localState = LocalState::CV;
     } else if ((localState == LocalState::CV) && (abc150Handler->getCurrent(channel) <= 0.2)) {
       ESP_LOGI(TAG, "CV done");
       espDischargeStartTime = TimeUtils::esp_timer_get_time_ms();
       abcDischargeStartTime = abc150Handler->getTimeStamp(channel);
       lastLoopTime = espDischargeStartTime;
       energy = 0;
       abc150Handler->setCurrent(channel, -1 * DISCHARGE_CURRENT);
       localState = LocalState::Discharge;
     } else if (localState == LocalState::Discharge) {
       currentTime = TimeUtils::esp_timer_get_time_ms();
       energy += (abc150Handler->getVoltage(channel) * abc150Handler->getCurrent(channel) * (currentTime - lastLoopTime));
       if (bmInfo->minCellVoltage <= 2.5) {
           ESP_LOGI(TAG, "Discharge done");
           localState = LocalState::Recharge;
           espDischargeEndTime = currentTime;
           abcDischargeEndTime = abc150Handler->getTimeStamp(channel);
           printResult();
        }
     } else if (localState == LocalState::Recharge) {
        abc150Handler->setCurrent(channel, 6);
        if (abc150Handler->getVoltage(channel) >= 320) {
          ESP_LOGI(TAG, "Recharge done");
          stopTest(TestState::Success);
          }
     }
   } else if (state == TestState::Restart) {
    stopWait = TimeUtils::esp_timer_get_time_ms();
    if (stopWait - startWait >= capacityWaitTime) {
      startTest(bmInfo);
    }
  }
}



