/*
 * PulseTest.cpp
 */

#include "PulseTest.hpp"
#include "TimeUtils.hpp"
#include "esp_log.h"

#define PULSE_TIME_MS   10000
#define PULSE_CURRENT   10.0


PulseTest::PulseTest(int _pulseWaitTime, ABC150CANHandler::Channel _channel, ABC150CANHandler *_abc150Handler, PlateCANHandler *_plateHandler):
                     SingleChannelTest(_channel, _abc150Handler, _plateHandler),
                     abc150Handler(_abc150Handler),
                     plateHandler(_plateHandler),
                     collection(BatteryModuleCollection::collection()),
                     bmInfo(NULL),
                     pulseWaitTime(_pulseWaitTime),
                     cellVoltagesInitial{},
                     cellVoltagesFinal{},
                     bmVoltageInitial(0),
                     bmVoltageFinal(0){
  TAG = "PulseTest";
  assert(startMutex != NULL);
  assert(stopMutex != NULL);
  }

bool PulseTest::startTest(BatteryModuleInfo *_bmInfo) {
  xSemaphoreTake(startMutex, portMAX_DELAY);
  if (!preTestChecks(_bmInfo)) {
    xSemaphoreGive(startMutex);
    return false;
  }
  if (state == ABC150Test::TestState::Running) {
    ESP_LOGE(TAG, "Already running");
    xSemaphoreGive(startMutex);
    return false;
  }
  if (collection.isBMErrors(_bmInfo)) {
    ESP_LOGE(TAG, "BM error present");
    xSemaphoreGive(startMutex);
    return false;
  }

  bmInfo = _bmInfo;

  /* Set limits */
  abc150Handler->setLowerVoltageLimit(channel, 240);
  abc150Handler->setLowerCurrentLimit(channel, PULSE_CURRENT * -1);
  abc150Handler->setLowerPowerLimit(channel, -4000);
  abc150Handler->setUpperVoltageLimit(channel, 401.5);
  abc150Handler->setUpperCurrentLimit(channel, 0);
  abc150Handler->setUpperPowerLimit(channel, 0);

  /* Copy initial values */
  memcpy(cellVoltagesInitial, bmInfo->cellVoltages, sizeof(cellVoltagesInitial));
  bmVoltageInitial = bmInfo->voltage;

  abc150Handler->setCurrent(channel, PULSE_CURRENT * -1);
  plateHandler->setBMState(bmInfo->batteryID, true);
  plateHandler->HVOn(bmInfo->batteryID);
  abc150Handler->takeControl(channel);
  vTaskDelay(pdMS_TO_TICKS(2000));
  if (abc150Handler->getConverterStatus(channel) != ABC150CANHandler::ConverterStatus::Remote) {
    ESP_LOGE(TAG, "Not in remote mode, taking control again.");
    abc150Handler->takeControl(channel);
    vTaskDelay(pdMS_TO_TICKS(2000));
  }
  startTime = TimeUtils::esp_timer_get_time_ms();
  AmpleLogger::getTestLogger()->logStartTime("PulseTest");
  state = ABC150Test::TestState::Running;
  abc150Handler->enable(channel);
  xSemaphoreGive(startMutex);
  return true;
}

bool PulseTest::stopTest(TestState testState) {
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
  AmpleLogger::getTestLogger()->logEndTime("PulseTest");
  if (testState == TestState::Success) {
    if (cycles > 0) {
      ESP_LOGI(TAG, "Cycle %d done", cycles);
      state = TestState::Restart;
      startWait = TimeUtils::esp_timer_get_time_ms();
      ESP_LOGI(TAG, "Restart");
    } else {
      if (state == TestState::Running) state = TestState::Success;
      ESP_LOGI(TAG, "Success");
    }
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

void PulseTest::printCellVoltages(float *cellVoltages) {
  float currentVoltage;
  float max = 0;
  float min = 100;
  int maxID = 0;
  int minID = 0;
  printf("== Cell Voltage Measurement ==\r\n");
  for (int i = 0; i < (NUM_CELLS/12); i++) {
    printf("[Bank #%d] ", i+1);
    for (int j = 0; j < 12; j++) {
      currentVoltage = cellVoltages[i*12 + j];
      printf((currentVoltage > 4.2 || currentVoltage < 3.0) ? "\033[1m\033[31m" : "\033[1m\033[32m");

      printf("%2d:%6.4f; ", 12*i+j+1, currentVoltage);
      printf("\033[0m"); // reset
      if (currentVoltage > max) {
        max = currentVoltage;
        maxID = 12*i+j+1;
      }
      if (currentVoltage < min) {
        min = currentVoltage;
        minID = 12*i+j+1;
      }
      if (j == 5) {
        printf("\r\n          ");
      }
    }
    printf("\r\n");
  
  }
  printf("Max voltage: %f \t ID: %d\r\n", max, maxID);
  printf("Min voltage: %f \t ID: %d\r\n", min, minID);
  
  if (max - min >= 0.1) {
    printf("===BROKEN WELD===");
  }
}

void PulseTest::printDCR() {
  float max = 0;
  float min = 100;
  int maxID = 0;
  int minID = 0;

  printf("BM DCR = %f\r\n", (bmVoltageInitial - bmVoltageFinal)/PULSE_CURRENT);
  float currentDCR;
  printf("== Cell DCR Measurement ==\r\n");
  for (int i = 0; i < (NUM_CELLS/12); i++) {
    printf("[Bank #%d] ", i+1);
    for (int j = 0; j < 12; j++) {
      currentDCR = (cellVoltagesInitial[i*12 + j] - cellVoltagesFinal[i*12 + j])/PULSE_CURRENT;

      printf("%2d:%6.4f; ", 12*i+j+1, currentDCR);

      if (j == 5) {
        printf("\r\n          ");
      }
      if (currentDCR > max) {
        max = currentDCR;
        maxID = 12*i+j+1;
      }
      if (currentDCR < min) {
        min = currentDCR;
        minID = 12*i+j+1;
      }
    }
    printf("\r\n");
  }
  printf("Max DCR: %f \t ID: %d\r\n", max, maxID);
  printf("Min DCR: %f \t ID: %d\r\n", min, minID);
}

void PulseTest::printResult() {

  printf("Initial cell voltages:\r\n");
  printCellVoltages(cellVoltagesInitial);

  printf("Initial BM Voltage: %fV\r\n", bmVoltageInitial);

  printf("Final cell voltages:\r\n");
  printCellVoltages(cellVoltagesFinal);

  printf("Final BM Voltage: %fV\r\n", bmVoltageFinal);

  printDCR();

}

void PulseTest::loop() {
  int64_t currentTime;
  if (state == TestState::Running) {

    if(loopCheck(bmInfo) == false) {
      stopTest(TestState::Failed);
      return;
    }

    if (collection.isBMErrors(bmInfo)) {
      ESP_LOGE(TAG, "BM error");
      stopTest(TestState::Failed);
      return;
    }
    currentTime = TimeUtils::esp_timer_get_time_ms();
    if (currentTime - startTime >= 10000) {
      /* Copy final values */
      memcpy(cellVoltagesFinal, bmInfo->cellVoltages, sizeof(cellVoltagesInitial));
      bmVoltageFinal = bmInfo->voltage;
      stopTest(TestState::Success);
      printResult();
    }
  } else if (state == TestState::Restart) {
    stopWait = TimeUtils::esp_timer_get_time_ms();
    if (stopWait - startWait >= pulseWaitTime) {
      startTest(bmInfo);
    }
  }
}
