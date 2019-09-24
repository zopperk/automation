/*
 * PlateChargeDischargeTest.cpp
 */

#include "esp_log.h"
#include "TimeUtils.hpp"
#include "PlateChargeDischargeTest.hpp"
#include "PCAL6416a.hpp"
#include "AmpleConfig.hpp"
#include "esp_task_wdt.h"


PlateChargeDischargeTest::PlateChargeDischargeTest(ABC150CANHandler *_abc150Handler, PlateCANHandler *_plateHandler):
                     DualChannelTest(_abc150Handler, _plateHandler),
                     abc150Handler(_abc150Handler),
                     plateHandler(_plateHandler),
                     collection(BatteryModuleCollection::collection()),
                     pcal6416a(PCAL6416a::getInstance()),
                     charging(false),
                     bmCount(0){
  TAG = "PlateChargeDischargeTest";
  cDFlag = true;
  cycleFlag = false;
  assert(startMutex != NULL);
  assert(stopMutex != NULL);
}

void PlateChargeDischargeTest::printResult() {
}

bool PlateChargeDischargeTest::startTest() {
  xSemaphoreTake(startMutex, portMAX_DELAY);
  if (state == TestState::Running) {
    ESP_LOGE(TAG, "Already running");
    xSemaphoreGive(startMutex);
    return false;
  }
  if (onlineCount < 1){
	  ESP_LOGE(TAG, "There are 0 BMs online.");
	  return false;
  }

  if (destinationVoltage == 0) {
    ESP_LOGE(TAG, "Plate Destination voltage not set");
    xSemaphoreGive(startMutex);
    return false;
  }
  ESP_LOGI(TAG, "Destination Voltage: %f\n", destinationVoltage);
  
  /*Initial settings*/
  plateHandler->HVOn();
  vTaskDelay(pdMS_TO_TICKS(500));

  if (collection.getHVCount() != onlineCount) {
    ESP_LOGE(TAG, "HVCount() != onlineCount\n");
    xSemaphoreGive(startMutex);
    return false;
  } else {
    ESP_LOGI(TAG, "There are %d BMs online.\n", onlineCount);
    bmCount = onlineCount;
  }

  /*set limits*/
  abc150Handler->setLowerVoltageLimit(ABC150CANHandler::A, 240);
  abc150Handler->setLowerCurrentLimit(ABC150CANHandler::A, -6 * onlineCount);
  abc150Handler->setLowerPowerLimit(ABC150CANHandler::A, -2460 * onlineCount);
  abc150Handler->setUpperVoltageLimit(ABC150CANHandler::A, 406);
  abc150Handler->setUpperCurrentLimit(ABC150CANHandler::A, 6 * onlineCount);
  abc150Handler->setUpperPowerLimit(ABC150CANHandler::A, 2460 * onlineCount);

  abc150Handler->setLowerVoltageLimit(ABC150CANHandler::B, 240);
  abc150Handler->setLowerCurrentLimit(ABC150CANHandler::B, -6 * onlineCount);
  abc150Handler->setLowerPowerLimit(ABC150CANHandler::B, -2460 * onlineCount);
  abc150Handler->setUpperVoltageLimit(ABC150CANHandler::B, 406);
  abc150Handler->setUpperCurrentLimit(ABC150CANHandler::B, 6 * onlineCount);
  abc150Handler->setUpperPowerLimit(ABC150CANHandler::B, 2460 * onlineCount);

  if (destinationVoltage > abc150Handler->getVoltage(ABC150CANHandler::A)) {
    charging = true;
  } else {
    charging = false;
  }

  /*Close plate contactors*/
  pcal6416a->gpioSetValue(CONFIG::CONTACTORS::preChargeCtrlPin,1);
  pcal6416a->gpioSetValue(CONFIG::CONTACTORS::relayNCtrlPin,1);
  vTaskDelay(pdMS_TO_TICKS(100));
  pcal6416a->gpioSetValue(CONFIG::CONTACTORS::relayPCtrlPin,1);
  vTaskDelay(pdMS_TO_TICKS(100));
  pcal6416a->gpioSetValue(CONFIG::CONTACTORS::preChargeCtrlPin,0);

  /* ABC150 Commands */
  abc150Handler->takeControl(ABC150CANHandler::A);
  vTaskDelay(pdMS_TO_TICKS(500));
  abc150Handler->takeControl(ABC150CANHandler::B);
  abc150Handler->setLoadMode(ABC150CANHandler::A,ABC150CANHandler::Parallel);
  vTaskDelay(pdMS_TO_TICKS(500));
  abc150Handler->releaseControl(ABC150CANHandler::B);
  startTime = TimeUtils::esp_timer_get_time_ms();
  AmpleLogger::getTestLogger()->logStartTime("PlateChargeDischarge");
  abc150Handler->setVoltage(ABC150CANHandler::A, destinationVoltage);
  abc150Handler->enable(ABC150CANHandler::A);
  state = TestState::Running;
  xLastWakeTimePlate = xTaskGetTickCount();
  xSemaphoreGive(startMutex);
  return true;
}

bool PlateChargeDischargeTest::stopTest(TestState testState) {
  xSemaphoreTake(stopMutex, portMAX_DELAY);
  abc150Handler->disable(ABC150CANHandler::A);
  vTaskDelay(pdMS_TO_TICKS(500));
  /*Open plate contactors*/
  pcal6416a->gpioSetValue(CONFIG::CONTACTORS::relayNCtrlPin,1);
  vTaskDelay(pdMS_TO_TICKS(100));
  pcal6416a->gpioSetValue(CONFIG::CONTACTORS::relayPCtrlPin,1);
  vTaskDelay(pdMS_TO_TICKS(100));
  plateHandler->HVOff();
  abc150Handler->setLoadMode(ABC150CANHandler::A,ABC150CANHandler::Independent);
  vTaskDelay(pdMS_TO_TICKS(500));
  abc150Handler->releaseControl(ABC150CANHandler::A);
  abc150Handler->releaseControl(ABC150CANHandler::B);
  stopTime = TimeUtils::esp_timer_get_time_ms();
  AmpleLogger::getTestLogger()->logEndTime("PlateChargeDischarge");
  if (testState == TestState::Success) {
    if (state == TestState::Running) state = TestState::Success;
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

void PlateChargeDischargeTest::loop() {
  int64_t currentTime;
  if (state == TestState::Running && loopCheck()) {
    currentTime = TimeUtils::esp_timer_get_time_ms();
    /* Stop test after current becomes negligible */
    if (((charging && abc150Handler->getCurrent(ABC150CANHandler::A) <= (0.2 * bmCount)) ||
        (!charging && abc150Handler->getCurrent(ABC150CANHandler::A) >= (-0.2 * bmCount)))
        && (currentTime - startTime > 5000)) {
      stopTest(TestState::Success);
    }
  }
}

