/*
 * PlateDriveCycleTest.cpp
 */

#include "esp_log.h"
#include "TimeUtils.hpp"
#include "PlateDriveCycleTest.hpp"
#include "PCAL6416a.hpp"
#include <fstream>
#include <iostream>
#include "esp_vfs.h"
#include "esp_spiffs.h"
#include "esp_task_wdt.h"
#include "AmpleConfig.hpp"

ifstream file;

PlateDriveCycleTest::PlateDriveCycleTest(int _driveCycleWaitTime, ABC150CANHandler *_abc150Handler, PlateCANHandler *_plateHandler):
                     DualChannelTest(_abc150Handler, _plateHandler),
                     abc150Handler(_abc150Handler),
                     plateHandler(_plateHandler),
                     collection(BatteryModuleCollection::collection()),
                     driveCycleWaitTime(_driveCycleWaitTime),
                     xLastWakeTimePlate(0),
                     pcal6416a(PCAL6416a::getInstance()){
  TAG = "PlateDriveCycleTest";
  assert(startMutex != NULL);
  assert(stopMutex != NULL);

  logger = AmpleLogger::getTestLogger();

  if (xTaskCreate(&PlateDriveCycleTest::loopPlateTaskWrapper, "ABC150 Plate loop", 4096, this, configMAX_PRIORITIES-4, &loopPlateTaskHandle) != pdPASS){
    ESP_LOGI(TAG, "Failed to create ABC150 loop Task");
  }
}

void PlateDriveCycleTest::printResult() {
}

int PlateDriveCycleTest::getTimeDelta(){
  int timeDelta;
  file >> timeDelta;
  return timeDelta*1000;
}

bool PlateDriveCycleTest::startTest() {
  xSemaphoreTake(startMutex, portMAX_DELAY);
  if(!preTestChecks()) {
    xSemaphoreGive(startMutex);
    return false;
  }
  if (state == TestState::Running) {
    ESP_LOGE(TAG, "Already running");
    xSemaphoreGive(startMutex);
    return false;
  }
  /* Mount and check SPIFFS status */
  esp_vfs_spiffs_conf_t conf = {
      .base_path = "/spiffs",
      .partition_label = "storage",
      .max_files = 5,
      .format_if_mount_failed = false
  };
  static const char *TAG = "SPIFFS";
  esp_err_t ret = esp_vfs_spiffs_register(&conf);
  if (ret != ESP_OK) {
    if (ret == ESP_FAIL) {
      ESP_LOGE(TAG, "Failed to mount or format filesystem");
    } else if (ret == ESP_ERR_NOT_FOUND) {
      ESP_LOGE(TAG, "Failed to find SPIFFS partition");
    } else {
      ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
    }
    xSemaphoreGive(startMutex);
    return false;
  }
  /*Open file*/
  file.open("/spiffs/DriveCycleSample.csv");
  if(!file.good()) {
    ESP_LOGE(TAG, "File is unreadable");
    xSemaphoreGive(startMutex);
    return false;
  }

  plateHandler->HVOn();
  vTaskDelay(pdMS_TO_TICKS(500));

  if (collection.getHVCount() != onlineCount) {
    ESP_LOGE(TAG, "HVCount() != onlineCount\n");
    xSemaphoreGive(startMutex);
    return false;
  } else {
    ESP_LOGI(TAG, "There are %d BMs online.\n", onlineCount);
  }

  /*set limits*/
  abc150Handler->setLowerVoltageLimit(ABC150CANHandler::A, 240);
  abc150Handler->setLowerCurrentLimit(ABC150CANHandler::A, -15 * onlineCount);
  abc150Handler->setLowerPowerLimit(ABC150CANHandler::A, -3600 * onlineCount);
  abc150Handler->setUpperVoltageLimit(ABC150CANHandler::A, 406);
  abc150Handler->setUpperCurrentLimit(ABC150CANHandler::A, 6 * onlineCount);
  abc150Handler->setUpperPowerLimit(ABC150CANHandler::A, 2436 * onlineCount);

  abc150Handler->setLowerVoltageLimit(ABC150CANHandler::B, 240);
  abc150Handler->setLowerCurrentLimit(ABC150CANHandler::B, -15 * onlineCount);
  abc150Handler->setLowerPowerLimit(ABC150CANHandler::B, -3600 * onlineCount);
  abc150Handler->setUpperVoltageLimit(ABC150CANHandler::B, 406);
  abc150Handler->setUpperCurrentLimit(ABC150CANHandler::B, 6 * onlineCount);
  abc150Handler->setUpperPowerLimit(ABC150CANHandler::B, 2436 * onlineCount);

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
  abc150Handler -> setLoadMode(ABC150CANHandler::A,ABC150CANHandler::Parallel);
  vTaskDelay(pdMS_TO_TICKS(500));
  abc150Handler->releaseControl(ABC150CANHandler::B);
  startTime = TimeUtils::esp_timer_get_time_ms();
  logger->logStartTime("PlateDriveCycleTest");
  abc150Handler->setPower(ABC150CANHandler::A, 0);
  abc150Handler->enable(ABC150CANHandler::A);
  state = TestState::Running;
  printf("Test Power\t|\tAvailable Power\t|\tCharging Power\t|\tC/D\t|\tValue\t|\tCurrent\t|\tCommand\n");
  xLastWakeTimePlate = xTaskGetTickCount();
  vTaskResume(loopPlateTaskHandle);
  xSemaphoreGive(startMutex);
  return true;
}

bool PlateDriveCycleTest::stopTest(TestState testState) {
  xSemaphoreTake(stopMutex, portMAX_DELAY);
  /* If the user stops the test in between cycles */
  if (testState == TestState::Idle && state == TestState::Restart) {
    state = TestState::Idle;
    ESP_LOGI(TAG, "Idle");
    xSemaphoreGive(stopMutex);
    return true;
  }
  logger->logEndTime("PlateDriveCycleTest");
  abc150Handler->disable(ABC150CANHandler::A);
  vTaskDelay(pdMS_TO_TICKS(500));

  /*Open plate contactors*/
  pcal6416a->gpioSetValue(CONFIG::CONTACTORS::relayNCtrlPin,1);
  vTaskDelay(pdMS_TO_TICKS(100));
  pcal6416a->gpioSetValue(CONFIG::CONTACTORS::relayPCtrlPin,1);
  vTaskDelay(pdMS_TO_TICKS(100));

  plateHandler->HVOff();
  abc150Handler -> setLoadMode(ABC150CANHandler::A,ABC150CANHandler::Independent);
  vTaskDelay(pdMS_TO_TICKS(1000));
  abc150Handler->releaseControl(ABC150CANHandler::A);
  abc150Handler->releaseControl(ABC150CANHandler::B);
  if(file.is_open()) {
    file.close();
  }
  esp_vfs_spiffs_unregister("storage");
  ESP_LOGI(TAG, "Test stopped");
  vTaskSuspend(loopPlateTaskHandle);
  abc150Handler->setDefaultFrequency();
  cycles--;
  stopTime = TimeUtils::esp_timer_get_time_ms();
  if (testState == TestState::Success) {
    if (cycles > 0) {
      ESP_LOGI(TAG, "Cycle %d done", cycles);
      state = TestState::Restart;
      startWait = TimeUtils::esp_timer_get_time_ms(); 
      ESP_LOGI(TAG, "Restart");
    } else {
      state = TestState::Success;
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

void PlateDriveCycleTest::loopPlateTaskWrapper(void *arg) {
  PlateDriveCycleTest* obj =  (PlateDriveCycleTest *)arg;
  obj->loopPlateTask();
}

void PlateDriveCycleTest::loopPlateTask() {
  CAN_frame_t canMsg;
  vTaskSuspend(NULL);
  const TickType_t xFrequency = pdMS_TO_TICKS(timeDelta);
  while (1) {
    vTaskDelayUntil(&xLastWakeTimePlate, xFrequency);
    loopPlate();
  }
}
void PlateDriveCycleTest::loop(){
  if (state == TestState::Running) {
    loopCheck();
  }
}

void PlateDriveCycleTest::loopPlate() {
  if (state == TestState::Running) {
    if (file.good()) {
      /*Get next value in drive cycle file*/
      float r[3];
      for(int i = 0; i < 3; ++i) {
        file >> r[i];
        file.get();
      }
      float power;
      float testPower = r[2]/16;
      /*Limit discharging power to 70 kW*/
      if (testPower >= 70){
        testPower = 70;
      }
      /*Power battery can provide*/
      float availablePower = BatteryModuleCollection::collection().getBatteryInfo()->availablePower;
      /*Power battery can intake*/
      float chargingPower = BatteryModuleCollection::collection().getBatteryInfo()->chargingPower;
      /*Charging or Discharging*/
      char CD;
      //Discharging plate
      if (testPower >= 0) {
          CD = 'D';
        if (testPower<=availablePower){
          power = testPower *-1;
        } else {
          power = availablePower*-1;
        }
      //Charging plate
      } else {
        CD = 'C';
        testPower = abs(testPower);
        if (testPower <= chargingPower) {
          power = testPower;
        } else {
          power = chargingPower;
        }
      }
      logger->logDriveCyclePower(testPower,power);
      abc150Handler->setPower(ABC150CANHandler::A, power*1000);
      printf("%f\t|\t%f\t|\t%f\t|\t%c\t|\t%f\t|\t%f\t|\t%f\t|\t\n", testPower, availablePower, chargingPower, CD, power, abc150Handler->getCurrent(ABC150CANHandler::A), abc150Handler->getCommand(ABC150CANHandler::A)/1000);
    } else {
      /*EOF*/
      if (file.eof()) {
        ESP_LOGI(TAG, "Finished Drive Cycle");
        stopTest(TestState::Success);
        return;
      } else {
        /* unreadable file*/
        ESP_LOGE(TAG, "File not readable.");
        stopTest(TestState::Failed);
        return;
      }
    }
  } else if (state == TestState::Restart) {
      stopWait = TimeUtils::esp_timer_get_time_ms();
      if (stopWait - startWait >= driveCycleWaitTime) {
        startTest();
      }
  }
}
