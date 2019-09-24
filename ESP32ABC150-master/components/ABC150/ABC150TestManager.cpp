/*
 * ABC150TestManager.cpp
 */
#include "ABC150TestManager.hpp"
#include "esp_log.h"
#include "esp_task_wdt.h"

TimerHandle_t debugLogTimer = NULL;


ABC150TestManager::ABC150TestManager(ABC150Controller &_abc150Controller) :
                  abc150Controller(_abc150Controller),
                  plateHandler(abc150Controller.getPlateCANHandler()),
                  abc150Handler(abc150Controller.getABC150CANHandler()),
                  collection(BatteryModuleCollection::collection()),
                  bmAmpleID{},
                  xLastWakeTime(0){

  /* Create a timer for logging */
  debugLogTimer = xTimerCreate("debugLogTimer",
                            pdMS_TO_TICKS(1000),
                            pdTRUE,
                            (void*)0,
                            debugLog);
  assert(debugLogTimer != NULL);
  debugLogEnable = false;
  /* Create loop task */
  if (xTaskCreate(&ABC150TestManager::loopTaskWrapper, "ABC150 loop", 4096, this, configMAX_PRIORITIES-3, &loopTaskHandle) != pdPASS){
    ESP_LOGE(TAG, "Failed to create ABC150 loop Task");
  }
}

void ABC150TestManager::debugLog(TimerHandle_t xTimer) {
  BatteryInfo *batteryInfo = BatteryModuleCollection::collection().getBatteryInfo();

  printf("%.02fV | %.02fA | %.02f%% | %d online | %d HV_ON | %.02fC_Max | %.02fC_Avg | %.02fkw_AP | %.02fkwh_AE | %.02fkw_CP\r\n",
    batteryInfo->voltage,
    batteryInfo->current,
    batteryInfo->soc,
    batteryInfo->onlineCount,
    batteryInfo->HVOnCount,
    batteryInfo->maxTemp,
    batteryInfo->avgTemp,
    batteryInfo->availablePower,
    batteryInfo->availableEnergy,
    batteryInfo->chargingPower
    );
}

void ABC150TestManager::addSingleChannelTest(SingleChannelTest *singleTest) {
  singleTestVec.push_back(singleTest);
}

void ABC150TestManager::addDualChannelTest(DualChannelTest *dualTest) {
  dualTestVec.push_back(dualTest);
}

const char* ABC150TestManager::getTestStateName(ABC150Test::TestState testState) {
  switch(testState) {
    case ABC150Test::TestState::Running:
      return "Running";

    case ABC150Test::TestState::Idle:
      return "Idle";

    case ABC150Test::TestState::Success:
      return "Success";

    case ABC150Test::TestState::Failed:
      return "Failed";

    case ABC150Test::TestState::Restart:
      return "Restart";

    default:
      return "Unknown";
  }
}

bool ABC150TestManager::runSingleTest(int singleTest, int cycleNum) {
  if (!testCheck(TestType::Single, singleTest)) {
    return false;
  }
  unsigned int bmID = bmAmpleID[singleTestVec[singleTest]->getChannel()];
  for (int j = 0; j < dualTestVec.size(); j++) {
    if (dualTestVec[j]->getTestState() == ABC150Test::TestState::Running) {
      ESP_LOGE(TAG, "%s already running.", dualTestVec[j]->getTestName());
      return false;
    }
  }
  for (int i = 0; i < singleTestVec.size(); i++) {
    if (singleTestVec[i]->getTestState() == ABC150Test::TestState::Running) {
      if (singleTestVec[i]->getChannel() == singleTestVec[singleTest]->getChannel()) {
        ESP_LOGE(TAG, "%s already running on channel %s.", singleTestVec[i]->getTestName(), SingleChannelTest::getChannelName(singleTestVec[i]->getChannel()));
        return false;
      }
    }
  }
  BatteryModuleInfo *bmInfo = collection.getBatteryModuleByID(bmID);
  if (bmInfo != NULL) {
    singleTestVec[singleTest]->setCycles(cycleNum);
    singleTestVec[singleTest]->startTest(bmInfo);
  } else {
    if (bmAmpleID[singleTestVec[singleTest]->getChannel()] == 0) {
      ESP_LOGE(TAG, "BM ID not set");
      return false;
    }
    ESP_LOGE(TAG, "Cannot find BM with Ample ID 0x%02x", bmID);
    return false;
  }
  return true;
}

bool ABC150TestManager::runDualTest(int dualTest, int cycleNum) {
  if (!testCheck(TestType::Dual, dualTest)) {
    return false;
  }
  for (int j = 0; j < dualTestVec.size(); j++) {
    if (dualTestVec[j]->getTestState() == ABC150Test::TestState::Running) {
      ESP_LOGE(TAG, "%s already running.", dualTestVec[j]->getTestName());
      return false;
    }
  }
  for (int i = 0; i < singleTestVec.size(); i++) {
    if (singleTestVec[i]->getTestState() == ABC150Test::TestState::Running) {
      ESP_LOGE(TAG, "%s already running on %s", singleTestVec[i]->getTestName(), SingleChannelTest::getChannelName(singleTestVec[i]->getChannel()));
      return false;
    }
  }
  dualTestVec[dualTest]->setCycles(cycleNum);
  dualTestVec[dualTest]->startTest();
  return true;
}

bool ABC150TestManager::stopSingleTest(int singleTest) {
  if (!testCheck(TestType::Single, singleTest)) {
    return false;
  }
  if (singleTestVec[singleTest]->getTestState() == ABC150Test::TestState::Running ||
      singleTestVec[singleTest]->getTestState() == ABC150Test::TestState::Restart) {
    singleTestVec[singleTest]->stopTest(ABC150Test::TestState::Idle);
  } else {
    ESP_LOGE(TAG, "%s not running on channel %s.", singleTestVec[singleTest]->getTestName(), SingleChannelTest::getChannelName(singleTestVec[singleTest]->getChannel()));
    return false;
  }
  return true;
}

bool ABC150TestManager::stopDualTest(int dualTest) {
  if (testCheck(TestType::Dual, dualTest)) {
    return false;
  }
  if (dualTestVec[dualTest]->getTestState() == ABC150Test::TestState::Running ||
      dualTestVec[dualTest]->getTestState() == ABC150Test::TestState::Restart) {
    dualTestVec[dualTest]->stopTest(ABC150Test::TestState::Idle);
  } else {
    ESP_LOGE(TAG, "%s not running.", dualTestVec[dualTest]->getTestName());
    return false;
  }
  return true;
}

void ABC150TestManager::stopAll() {
  for(int i = 0; i < singleTestVec.size(); i++) {
    if (singleTestVec[i]->getTestState() == ABC150Test::TestState::Running ||
        singleTestVec[i]->getTestState() == ABC150Test::TestState::Restart) {
      singleTestVec[i]->stopTest(ABC150Test::TestState::Idle);
    }
  }
  for(int j = 0; j < dualTestVec.size(); j++) {
    if (singleTestVec[j]->getTestState() == ABC150Test::TestState::Running ||
        singleTestVec[j]->getTestState() == ABC150Test::TestState::Restart) {
      singleTestVec[j]->stopTest(ABC150Test::TestState::Idle);
    }
  }
}

void ABC150TestManager::stopAllOverride() {
  plateHandler->HVOff();
  vTaskDelay(pdMS_TO_TICKS(500));
  abc150Handler->releaseControl(ABC150CANHandler::A);
  abc150Handler->releaseControl(ABC150CANHandler::B);
}

bool ABC150TestManager::testCheck(TestType type, int test) {
  if (type == TestType::Single) {
    if (test < 0 || test >= singleTestVec.size()) {
      ESP_LOGE(TAG, "Invalid test");
      return false;
    }
  } else if (type == TestType::Dual) {
    if (test < 0 || test >= dualTestVec.size()) {
      ESP_LOGE(TAG, "Invalid test");
      return false;
    }
  }
  return true;
}

bool ABC150TestManager::checkCycleFlag(TestType type, int test) {
  if (type == TestType::Single) {
    return singleTestVec[test]->getCycleFlag();
  } else if (type == TestType::Dual) {
    return dualTestVec[test]->getCycleFlag();
  }
  return false;
}

bool ABC150TestManager::checkCDFlag(TestType type, int test) {
  if (type == TestType::Single) {
    return singleTestVec[test]->getCDFlag();
  } else if (type == TestType::Dual) {
    return dualTestVec[test]->getCDFlag();
  }
  return false;
}

void ABC150TestManager::listAllTests() {
  printf("\r\n");
  printf(GREEN "%-25s|%-10s|%-10s\r\n", "TestName", "Channel", "State" RESET);
  for (int i = 0; i < singleTestVec.size(); i++) {
    printf("%-25s|%-10s|%-10s\r\n", singleTestVec[i]->getTestName(),
      SingleChannelTest::getChannelName(singleTestVec[i]->getChannel()), getTestStateName(singleTestVec[i]->getTestState()));
  }
  for (int j = 0; j < dualTestVec.size(); j++) {
    printf("%-25s|%-10s|%-10s\r\n", dualTestVec[j]->getTestName(), "dual",
      getTestStateName(dualTestVec[j]->getTestState()));
  }
}

void ABC150TestManager::listTestsByType(TestType type) {
  printf("\r\n");
  if (type == TestType::Single) {
      printf(GREEN "%-3s|%-25s|%-10s|%-10s\r\n", "Num","TestName", "Channel", "State" RESET);
      for (int i = 0; i < singleTestVec.size(); i++) {
        printf("%-3d|%-25s|%-10s|%-10s\r\n", i, singleTestVec[i]->getTestName(),
        SingleChannelTest::getChannelName(singleTestVec[i]->getChannel()), getTestStateName(singleTestVec[i]->getTestState()));
      }
      return;
  } else if (type == TestType::Dual) {
      printf(GREEN "%-3s|%-25s|%-10s\r\n", "Num","TestName", "State" RESET);
      for (int j = 0; j < dualTestVec.size(); j++) {
        printf("%-3d|%-25s|%-10s\r\n", j, dualTestVec[j]->getTestName(),
        getTestStateName(dualTestVec[j]->getTestState()));
      }
      return;
  }
}

bool ABC150TestManager::setDestinationVoltage(TestType type, int test, float voltage) {
  if(!testCheck(type, test)) {
    return false;
  }
  if (type == TestType::Single) {
    singleTestVec[test]->setDestinationVoltage(voltage);
  } else if (type == TestType::Dual) {
    dualTestVec[test]->setDestinationVoltage(voltage);
  }
  return true;
}

void ABC150TestManager::setBMAmpleID(int ch, unsigned int ID) {
  if (ch == 0 || ch == 1) {
    bmAmpleID[ch] = ID;
  } else {
    ESP_LOGE(TAG, "Invalid channel");
  }
}

void ABC150TestManager::debugToggle() {
  if (debugLogEnable == true) {
    debugLogEnable = false;
    assert(xTimerStop(debugLogTimer, pdMS_TO_TICKS(1000)) == pdPASS);
  } else {
    debugLogEnable = true;
    assert(xTimerStart(debugLogTimer, pdMS_TO_TICKS(1000)) == pdPASS);
  }
  printf("debug output %s\r\n", debugLogEnable ? "enabled" : "disabled");
}

void ABC150TestManager::loopTaskWrapper(void *arg) {
  ABC150TestManager * obj =  (ABC150TestManager *)arg;
  obj->loopTask();
}

uint64_t ABC150TestManager::getRunningTime(TestType type, int test) {
  if (type == TestType::Single) {
    if (testCheck(TestType::Single, test)) {
      return singleTestVec[test]->getRunningTime();
    }
  } else if (type == TestType::Dual) {
    if (testCheck(TestType::Dual, test)) {
      return dualTestVec[test]->getRunningTime();
    }
  }
  return 0;
}

void ABC150TestManager::loopTask() {
  CAN_frame_t canMsg;
  const TickType_t xFrequency = pdMS_TO_TICKS(100);//Run 100ms
  xLastWakeTime = xTaskGetTickCount();

  while (1) {
    // Wait for the next cycle.

    vTaskDelayUntil(&xLastWakeTime, xFrequency);
    for(int i = 0; i < singleTestVec.size(); i++) {
      singleTestVec[i]->loop();
    }
    for (int j = 0; j < dualTestVec.size(); j++) {
      dualTestVec[j]->loop();
    }
  }
}

void ABC150TestUserInterface::help() {
  printf("\r\n");
  printf(GREEN "ABC150 Test Interface.\r\n" RESET);

  printf("  1: Start Single Channel Test\r\n");
  printf("  2: Start Dual Channel Test\r\n");
  printf("  3: Stop Single Channel Test\r\n");
  printf("  4: Stop Dual Channel Test\r\n");
  printf("  5: Stop All Tests\n");
  printf("  6: Stop All Override\r\n");
  printf("  7: Set BMID\r\n\n");

  printf("  d: Enable/Disable debug output\r\n");
  printf("  r: Get running time of a test\r\n");
  printf("  l: List all tests\r\n");

  printf("  h: Print this help again\r\n");
  printf("  q: Quit\r\n\n\n");
}

void ABC150TestUserInterface::ABC150TestInterface(AmpleSerial &pc, ABC150TestManager *testManager) {
  int bmID;
  float voltage;
  pc.clear();
  ABC150TestUserInterface::help();
  while(1) {
      char input;
      int test;
      int ch;
      int cycles;
      char confirm;
      int type;
      printf("ABC150> ");
      fflush(stdout);
      input = pc.rx_char();
      printf("%c\r\n", input);
      switch(input) {

      case '1':
        testManager->listTestsByType(ABC150TestManager::TestType::Single);
        printf("\r\n");
        if (pc.readNumber(test, "Which test? ")) {
          printf("\r\n");
          if (!testManager->testCheck(ABC150TestManager::TestType::Single, test)) break;
          if (testManager->checkCycleFlag(ABC150TestManager::TestType::Single, test)) {
            if (!pc.readNumber(cycles, "Number of cycles: ")) break;          }
          if (testManager->checkCDFlag(ABC150TestManager::TestType::Single, test)) {
            if (pc.readFloatNumber(voltage, "Charge/Discharge voltage: ")) {
              if (!testManager->setDestinationVoltage(ABC150TestManager::TestType::Single, test, voltage)) break;
            }
          }
          testManager->runSingleTest(test, cycles);
        }
        break;

      case '2':
        testManager->listTestsByType(ABC150TestManager::TestType::Dual);
        printf("\r\n");
        if (pc.readNumber(test, "Which test? ")) {
          printf("\r\n");
          if(!testManager->testCheck(ABC150TestManager::TestType::Dual, test)) break;
          if (testManager->checkCycleFlag(ABC150TestManager::TestType::Dual, test)) {
            if (!pc.readNumber(cycles, "Number of cycles: ")) break;
          }
          if (testManager->checkCDFlag(ABC150TestManager::TestType::Dual, test)) {
            if (pc.readFloatNumber(voltage, "Charge/Discharge voltage: ")) {
              if (!testManager->setDestinationVoltage(ABC150TestManager::TestType::Dual, test, voltage)) break;
            }
          }
          testManager->runDualTest(test, cycles);
        }
        break;

      case '3':
        testManager->listTestsByType(ABC150TestManager::TestType::Single);
        printf("\r\n");
        if(pc.readNumber(test, "Which test? ")) {
          testManager->stopSingleTest(test);
        }
        break;

      case '4':
        testManager->listTestsByType(ABC150TestManager::TestType::Dual);
        printf("\r\n");
        if(pc.readNumber(test, "Which test? ")) {
          testManager->stopDualTest(test);
        }
        break;

      case '5':
        printf("Stop all running tests? Press 'n' to cancel or 'y' to confirm.\r\n");
        confirm = pc.rx_char();
        printf("\r\n");
        if (confirm == 'y') {
          testManager->stopAll();
        } else if (confirm == 'n') {
          ESP_LOGI("ABC150TestManager", "Stop all cancelled");
        } else {
          ESP_LOGE("ABC150TestManager", "Invalid input");
        }
        break;

      case '6':
        printf("Stop all tests? Press 'n' to cancel or 'y' to confirm.\r\n");
        confirm = pc.rx_char();
        printf("\r\n");
        if (confirm == 'y') {
          testManager->stopAllOverride();
        } else if (confirm == 'n') {
          ESP_LOGI("ABC150TestManager", "Stop all cancelled");
        } else {
          ESP_LOGE("ABC150TestManager", "Invalid input");
        }
        break;

      case '7':
        printf("\n0| A\r\n1| B\r\n");
        printf("\r\n");
        if (pc.readNumber(ch, "Which channel? ")) {
          printf("\r\n");
          if(pc.readNumber(bmID, "Enter BM Ample ID: ")) {
            testManager->setBMAmpleID(ch, bmID);
          }
          printf("\r\n");
        }
        break;

      case 'd':
        testManager->debugToggle();
        break;

      case 'r':
        printf("\n1| Single\r\n2| Dual\r\n");
        printf("\r\n");
        if (pc.readNumber(type, "Single or Dual? ")) {
          if (type == 1) {
            testManager->listTestsByType(ABC150TestManager::TestType::Single);
            printf("\r\n");
            if(pc.readNumber(test, "Which test? ")) {
              testManager->testCheck(ABC150TestManager::TestType::Single, test);
              testManager->getRunningTime(ABC150TestManager::TestType::Single, test);
            }
          } else if (type == 2) {
            testManager->listTestsByType(ABC150TestManager::TestType::Dual);
            printf("\r\n");
            if(pc.readNumber(test, "Which test? ")) {
              testManager->testCheck(ABC150TestManager::TestType::Dual, test);
              testManager->getRunningTime(ABC150TestManager::TestType::Dual, test);
            }
          } else {
            ESP_LOGE("ABC150TestManager", "Invalid type");
          }
        }
        break;

      case 'l':
        testManager->listAllTests();
        break;

      case 'h':
        ABC150TestUserInterface::help();
        break;

      case 'q':
        return;
        break;

      default:
        break;

      }
  }
}




