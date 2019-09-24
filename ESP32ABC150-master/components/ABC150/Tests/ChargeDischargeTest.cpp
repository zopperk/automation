/*
 * ChargeDischarge.cpp
 */

#include "ChargeDischargeTest.hpp"
#include "esp_log.h"
#include "TimeUtils.hpp"


ChargeDischargeTest::ChargeDischargeTest(ABC150CANHandler::Channel _channel, ABC150CANHandler *_abc150Handler, PlateCANHandler *_plateHandler):
                     SingleChannelTest(_channel, _abc150Handler, _plateHandler),
                     abc150Handler(_abc150Handler),
                     plateHandler(_plateHandler),
                     collection(BatteryModuleCollection::collection()),
                     bmInfo(NULL),
                     charging(false){
  TAG = "ChargeDischargeTest";
  cDFlag = true;
  cycleFlag = false;
  assert(startMutex != NULL);
  assert(stopMutex != NULL);
  }

bool ChargeDischargeTest::startTest(BatteryModuleInfo *_bmInfo) {
  xSemaphoreTake(startMutex, portMAX_DELAY);
  if (!preTestChecks(_bmInfo)) {
    xSemaphoreGive(startMutex);
    return false;
  }

  if (destinationVoltage == 0) {
    ESP_LOGE(TAG, "Destination voltage not set");
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

  if (destinationVoltage > bmInfo->voltage) {
    charging = true;
  } else {
    charging = false;
  }

  abc150Handler->setVoltage(channel, destinationVoltage);
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
  AmpleLogger::getTestLogger()->logStartTime("Charge/DischargeTest");
  abc150Handler->enable(channel);
  xSemaphoreGive(startMutex);
  return true;
}

bool ChargeDischargeTest::stopTest(TestState testState) {
  xSemaphoreTake(stopMutex, portMAX_DELAY);
  AmpleLogger::getTestLogger()->logEndTime("Charge/DischargeTest");
  abc150Handler->disable(channel);
  vTaskDelay(pdMS_TO_TICKS(500));
  abc150Handler->releaseControl(channel);
  plateHandler->HVOff(bmInfo->batteryID);
  destinationVoltage = 0;
  stopTime = TimeUtils::esp_timer_get_time_ms();
  if (testState == TestState::Success) {
    state = TestState::Success;
    ESP_LOGI(TAG, "Success");
  } else if (testState == TestState::Failed) {
    state = TestState::Failed;
    ESP_LOGI(TAG, "Failed");
  } else if (testState == TestState::Idle) {
    state = TestState::Idle;
    ESP_LOGI(TAG, "Idle");
  }
  xSemaphoreGive(stopMutex);
  return true;
}

void ChargeDischargeTest::printResult() {
}

void ChargeDischargeTest::loop() {
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
         bmInfo->currentError ||
         bmInfo->tempSensingFailure ||
         bmInfo->voltageSensingFailure ||
         bmInfo->FETFailure ||
         bmInfo->otherHardwareFailure) {
       ESP_LOGE(TAG, "BM error");
       stopTest(TestState::Failed);
       return;
     }

     currentTime = TimeUtils::esp_timer_get_time_ms();
     if (((charging && abc150Handler->getCurrent(channel) <= 0.2) ||
         (!charging && abc150Handler->getCurrent(channel) >= -0.2))
         && (currentTime - startTime > 5000)) {
       stopTest(TestState::Success);
     }
   }
}


