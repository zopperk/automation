#include <esp_log.h>
#include <string>

#include "ABC150Test.hpp"
#include "PlateLogger.hpp"
#include "sdkconfig.h"
#include "Serial.hpp"
#include "AmpleSerial.hpp"
#include "PlateCANHandler.hpp"
#include "ESP32NetworkManager.hpp"
#include "AmpleProtocolTask.hpp"
#include "NVSConfig.hpp"
#include "AmpleConfig.hpp"
#include "TextLogger.hpp"
#include "LoggerInterface.hpp"
#include "ABC150Controller.hpp"
#include "BatteryInterface.hpp"
#include "SingleChannelTest.hpp"
#include "DualChannelTest.hpp"
#include "ABC150TestManager.hpp"
#include "CapacityTest.hpp"
#include "PulseTest.hpp"
#include "ChargeDischargeTest.hpp"
#include "PlateChargeDischargeTest.hpp"
#include "PlateDriveCycleTest.hpp"
#include "ABC150CANHandler.hpp"
#include "PlateCANHandler.hpp"
#include "AmpleConfig.hpp"


using namespace CONFIG::WAITTIMES;
using namespace std;

extern "C" {
	void app_main(void);
}

void help() {

  printf("==              Ample!                ==\r\n");
  printf("==   	     ABC150 Automation          ==\r\n");
  printf("==               v 2.0                ==\r\n");
  printf("Press:\r\n");
  printf(GREEN " a" RESET ": ABC150 CAN Interface\r\n");
  printf(GREEN " t" RESET ": ABC150 Test Interface\r\n");
  printf(GREEN " b" RESET ": to control battery\r\n");
  printf(GREEN " n" RESET ": for NVS menu\r\n");
  printf(GREEN " l" RESET ": to manage logs\r\n");
  printf(GREEN " r" RESET ": Reboot\r\n");


}

void app_main(void)
{
  AmpleSerial serial(UART_NUM_0);
  NVSConfig::initialize(CONFIG::NVS::NVSConfig, sizeof(CONFIG::NVS::NVSConfig) / sizeof(NVSConfigRow), CONFIG::NVS::NVS_UPDATE, serial);
  uint32_t ampleID = 0x0F000000 | NVSConfig::getInt(AMPLE_ID);
  if (NVSConfig::getInt(WIFI_ENABLE)) {
    ESPNetworkManager *net = ESPNetworkManager::getInstance();
    net->initWifiStation(NVSConfig::getString(WIFI_SSID), NVSConfig::getString(WIFI_PASSWORD));
  }
  AmpleProtocolTask protocolTask(NVSConfig::getString(WEB_SOCKET_URL), ampleID, NVSConfig::getInt(AMPLE_PROTOCOL_ENABLE));

  /* Initialize PCAL6416A */
  I2C i2c(GPIO_NUM_21, GPIO_NUM_22, 400000);
  PCAL6416a *pcal6416a = PCAL6416a::getInstance();
  pcal6416a->initialize(false, &i2c);

  /*Initialize contactor GPIO pins*/
  pcal6416a->setDirectionOutput(CONFIG::CONTACTORS::preChargeCtrlPin);
  pcal6416a->setDirectionOutput(CONFIG::CONTACTORS::relayPCtrlPin);
  pcal6416a->setDirectionOutput(CONFIG::CONTACTORS::relayNCtrlPin);

  pcal6416a->gpioSetValue(CONFIG::CONTACTORS::preChargeCtrlPin,0);
  pcal6416a->gpioSetValue(CONFIG::CONTACTORS::relayPCtrlPin,0);
  pcal6416a->gpioSetValue(CONFIG::CONTACTORS::relayNCtrlPin,0);

  if (NVSConfig::getInt(AMPLE_LOGGING_ENABLE)) {
     /* Initialize logger */
     RingLog* ringLog = RingLog::getInstance();
     ringLog->init(CONFIG::RING_LOG::media, CONFIG::RING_LOG::logFileSize);
     TextLogger::initialize(ringLog, &protocolTask.client, ampleID);
     //MemLogger::initialize(1000);
     PlateLogger::initialize();

  }

  ABC150Controller abc150Controller(ampleID);
  ABC150TestManager testManager(abc150Controller);


  ABC150CANHandler *abc150Handler = abc150Controller.getABC150CANHandler();
  PlateCANHandler *plateHandler = abc150Controller.getPlateCANHandler();

  /* ABC150 Tests */
  PulseTest *pulseTest = new PulseTest[2]{{PULSE_WAIT_TIME_MS, ABC150CANHandler::A, abc150Handler, plateHandler}, {PULSE_WAIT_TIME_MS, ABC150CANHandler::B, abc150Handler, plateHandler}};
  CapacityTest *capacityTest = new CapacityTest[2]{{CAPACITY_WAIT_TIME_MS, ABC150CANHandler::A, abc150Handler, plateHandler},{CAPACITY_WAIT_TIME_MS, ABC150CANHandler::B, abc150Handler, plateHandler}};
  ChargeDischargeTest *chargeDischargeTest = new ChargeDischargeTest[2]{{ABC150CANHandler::A, abc150Handler, plateHandler},{ABC150CANHandler::B, abc150Handler, plateHandler}};
  
  PlateDriveCycleTest *plateDriveCycleTest = new PlateDriveCycleTest(DRIVECYCLE_WAIT_TIME_MS, abc150Handler, plateHandler);
  PlateChargeDischargeTest *plateChargeDischargeTest = new PlateChargeDischargeTest(abc150Handler, plateHandler);

  /* Registering tests with Test Manager */
  testManager.addSingleChannelTest(&pulseTest[0]);
  testManager.addSingleChannelTest(&pulseTest[1]);
  testManager.addSingleChannelTest(&capacityTest[0]);
  testManager.addSingleChannelTest(&capacityTest[1]);
  testManager.addSingleChannelTest(&chargeDischargeTest[0]);
  testManager.addSingleChannelTest(&chargeDischargeTest[1]);

  testManager.addDualChannelTest(plateDriveCycleTest);
  testManager.addDualChannelTest(plateChargeDischargeTest);

  while (1) {
    // if in packet mode, the loop call will block
    serial.clear();
    help();

    switch (serial.rx_char()) {

    case 'a':
      ABC150CANUserInterface::ABC150CANInterface(serial, abc150Controller.getABC150CANHandler());
      break;

    case 't':
      ABC150TestUserInterface::ABC150TestInterface(serial, &testManager);
      break;

    case 'b':
      manageBattery(abc150Controller, serial);
      break;


    case 'n':
      NVSConfig::NVSUserInterface(serial);
      break;

    case 'r':
      esp_restart();
      break;

    case 'l':
      manageLogger(serial);
      break;

    default:
      break;
    }
  }
}
