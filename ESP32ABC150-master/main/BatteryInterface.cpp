/*
 * BatteryInterface.cpp
 */

#include "BatteryInterface.hpp"


#include "AmpleSerial.hpp"
#include "PlateCANHandler.hpp"
#include "esp_log.h"
#include "AmpleConfig.hpp"


void batteryHelp(AmpleSerial &pc) {

  printf("  1: Enable 12v\r\n");
  printf("  2: Disable 12v\r\n\n");

  printf(RED "  3: Turn HV ON \r\n" RESET);
  printf("  4: Turn HV OFF\r\n\n");

  printf("  5: Set BM Online \r\n");
  printf("  6: Set BM Offline\r\n\n");

  printf("  l: List attached battery modules.\r\n");
  printf("  b: List Battery State Info\r\n");

  printf("  i: Report battery module info.\r\n");
  printf("\r\n");
  printf("  s: Request BMs update SOC\r\n");
  printf("  o: Request OTA\r\n");
  printf("\r\n");
  printf(RED "  q: go back to top menu\r\n" RESET);
  printf("\r\nHave fun and be careful!\r\n");
}

void printCellVoltages(float *cellVoltages, AmpleSerial &pc) {
  float currentVoltage;
  printf("== Cell Voltage Measurement ==\r\n");
  for (int i = 0; i < (NUM_CELLS/12); i++) {
    printf("[Bank #%d] ", i+1);
    for (int j = 0; j < 12; j++) {
      currentVoltage = cellVoltages[i*12 + j];
      printf((currentVoltage > 4.2 || currentVoltage < 3.0) ? "\033[1m\033[31m" : "\033[1m\033[32m");

      printf("%2d:%6.4f; ", 12*i+j+1, currentVoltage);
      printf("\033[0m"); // reset

      if (j == 5) {
        printf("\r\n          ");
      }
    }
    printf("\r\n");
  }
}

void printSensorTemperatures(float *sensorTemperatures, AmpleSerial &pc) {
  float currentTemp;
  printf("== Sensor Temperature Measurement ==\r\n");
  for (int i = 0; i < (NUM_THERMISTORS/16); i++) {
    printf("[Bank #%d] ", i+1);
    for (int j = 0; j < 16; j++) {
      currentTemp = sensorTemperatures[i*16 + j];
      printf((currentTemp > 46 ) ? "\033[1m\033[31m" : "\033[1m\033[32m");

      printf("%2d:%7.4f; ", 16*i+j+1, currentTemp);
      printf("\033[0m"); // reset

      if (j == 7) {
        printf("\r\n          ");
      }
    }
    printf("\r\n");
  }
}

void requestOTA(AmpleSerial &pc, PlateCANHandler &handler)
{
  char serverIP[20];
  struct in_addr addr;
  int port;
  int OTAVersion;
  int OTADeviceType;

  if (!pc.readString(serverIP, 20, "Enter server IP: "))
  {
    printf("Error reading server IP\r\n");
    return;
  }

  if (!inet_pton(AF_INET, serverIP, &addr))
  {
    printf("Invalid Server IP: %s\r\n", serverIP);
    return;
  }

  if (!pc.readNumber(port, "Enter server Port:"))
  {
    printf("Read error.\r\n");
    return;
  }

  if (!pc.readNumber(OTAVersion, "Enter version:"))
  {
    printf("Read error.\r\n");
    return;
  }

  if (!pc.readNumber(OTADeviceType, "Enter Device Type:"))
  {
    printf("Read error.\r\n");
    return;
  }

  if ((port == 0) || (port > UINT16_MAX))
  {
    printf("Invalid Port: %d\r\n", port);
    return;
  }

  handler.requestOTA(addr, htons(port), NVSConfig::getString(WIFI_SSID), NVSConfig::getString(WIFI_PASSWORD), OTAVersion, OTADeviceType);
}

void manageBattery(ABC150Controller &controller, AmpleSerial &pc) {
  int bmCount;
  BatteryModuleInfo **bms;
  BatteryModuleCollection &collection = BatteryModuleCollection::collection();
  BatteryInfo *batteryInfo = collection.getBatteryInfo();
  PlateCANHandler* plateHandler = controller.getPlateCANHandler();

  pc.clear();
  batteryHelp(pc);

  while(1)
{
  int input;
  int batteryModuleID;

    switch(input = pc.getKey()) {

    case '1':
      controller.turn12vOn();
      break;

    case '2':
      controller.turn12vOff();
      break;

    case '3':
      if (pc.readNumber(batteryModuleID, "Which battery module?\r\n")) {
        BatteryModuleInfo *bmInfo = collection.getBatteryModuleByBatteryID(batteryModuleID);
        if (bmInfo) {
          printf("Turning HV on for BM 0x%02x", batteryModuleID);
          plateHandler->HVOn(batteryModuleID);
        } else {
          printf("Battery module with id 0x%02x does not exist\r\n", batteryModuleID);
        }
      }
      break;

    case '4':
      if (pc.readNumber(batteryModuleID, "Which battery module?\r\n")) {
        BatteryModuleInfo *bmInfo = collection.getBatteryModuleByBatteryID(batteryModuleID);
        if (bmInfo) {
          printf("Turning HV off for BM 0x%02x", batteryModuleID);
          plateHandler->HVOff(batteryModuleID);
        } else {
          printf("Battery module with id 0x%02x does not exist\r\n", batteryModuleID);
        }
      }
      break;

    case '5':
      if (pc.readNumber(batteryModuleID, "Which battery module?\r\n")) {
        BatteryModuleInfo *bmInfo = collection.getBatteryModuleByBatteryID(batteryModuleID);
        if (bmInfo) {
          printf("Turning BM 0x%02x online", batteryModuleID);
          plateHandler->setBMState(batteryModuleID, true);
        } else {
          printf("Battery module with id 0x%02x does not exist\r\n", batteryModuleID);
        }
      }
      break;

    case '6':
      if (pc.readNumber(batteryModuleID, "Which battery module?\r\n")) {
        BatteryModuleInfo *bmInfo = collection.getBatteryModuleByBatteryID(batteryModuleID);
        if (bmInfo) {
          printf("Turning BM 0x%02x offline", batteryModuleID);
          plateHandler->setBMState(batteryModuleID, false);
        } else {
          printf("Battery module with id 0x%02x does not exist\r\n", batteryModuleID);
        }
      }
      break;

      case 'l':
        bmCount = collection.count();
        bms = new BatteryModuleInfo *[bmCount];
        bmCount = collection.getAllBatteryModules(bms, bmCount);
        printf("%d battery modules present\r\n", bmCount);

        if (bmCount != 0) {
          printf("ID\t|B-ID\t|Online\t|<-Stat\t|FOff|Chg|HV|SC|CellVErr|VDiffErr|busVDiffErr|TemErr|CurErr|V     |BusV\t|BMV\t|Cur(A)\t|Ver\r\n");
          for(int i = 0; i < 135; i++){
            printf("-");
          }
          printf("\r\n");
          for (int i = 0; i < bmCount; i++) {
            printf("0x%04X\t|0x%02X\t|%d\t|%d\t|%d   |%d  |%d |%d |%d\t|%d\t |%d\t     |%d\t    |%d     |%.02f|%.02f\t|%.02f\t|%.02f\t |%x\r\n\n",
                bms[i]->id,
                bms[i]->batteryID,
                bms[i]->online,
                bms[i]->onlineStatus,
                bms[i]->manualForceOffline,
                bms[i]->charging,
                bms[i]->HVOn,
                bms[i]->shortCircuitError,
                bms[i]->cellVoltageError,
                bms[i]->voltageDiffError,
                bms[i]->busVoltageDiffError,
                bms[i]->tempError,
                bms[i]->currentError,
                bms[i]->voltage,
                bms[i]->busVoltage,
                bms[i]->bmVoltage,
                bms[i]->current,
                bms[i]->softwareVersion
            );
          }

          printf("ID\t|B-ID\t|AvgT(C)|MaxT(C)|ID\t|minC(V)|ID\t|maxC(V)|ID\t|SOC\t|AP\t|AE\t|CP\t|CC\t|TF|VF|FETF|OF\r\n");
                    for(int i = 0; i < 135; i++){
                      printf("-");
                    }
                    printf("\r\n");
                    for (int i = 0; i < bmCount; i++) {
                      printf("%04X\t|0x%02X\t|%.02f\t|%.02f\t|%d\t|%.02f\t|%d\t|%.02f\t|%d\t|%.02f\t|%.02f\t|%.02f\t|%.02f\t|%.02f\t|%d |%d |%d |%d\r\n\n",
                          bms[i]->id,
                          bms[i]->batteryID,
                          bms[i]->avgTemp,
                          bms[i]->maxTemp,
                          bms[i]->maxTempID,
                          bms[i]->minCellVoltage,
                          bms[i]->minCellVoltageID,
                          bms[i]->maxCellVoltage,
                          bms[i]->maxCellVoltageID,
                          bms[i]->soc,
                          bms[i]->availablePower,
                          bms[i]->availableEnergy,
                          bms[i]->chargingPower,
                          bms[i]->chargingCurrent,
                          bms[i]->tempSensingFailure,
                          bms[i]->voltageSensingFailure,
                          bms[i]->FETFailure,
                          bms[i]->otherHardwareFailure
                      );
                    }
        }
        delete bms;
        break;

      case 'b':
		  printf("Battery State Info:\r\n"
			  "Voltage: %f\r\n"
			  "Current: %f\r\n"
			  "AvgTemp: %f\r\n"
			  "MaxTemp: %f\r\n"
			  "MinCellVoltage: %f\r\n"
			  "MaxCellVoltage: %f\r\n"
			  "SOC: %f\r\n"
			  "A-Power: %f\r\n"
			  "A-Energy: %f\r\n"
			  "C-Power: %f\r\n"
			  "C-Current: %f\r\n"
			  "OnlineCount: %d\r\n"
			  "HVOnCount: %d\r\n",
		   batteryInfo->voltage,
		   batteryInfo->current,
		   batteryInfo->avgTemp,
		   batteryInfo->maxTemp,
		   batteryInfo->minCellVoltage,
		   batteryInfo->maxCellVoltage,
		   batteryInfo->soc,
		   batteryInfo->availablePower,
		   batteryInfo->availableEnergy,
		   batteryInfo->chargingPower,
		   batteryInfo->chargingCurrent,
		   batteryInfo->onlineCount,
		   batteryInfo->HVOnCount);
		  break;


      case 'i':
        if (pc.readNumber(batteryModuleID, "Which battery module?\r\n")) {
          BatteryModuleInfo *bmInfo = collection.getBatteryModuleByBatteryID(batteryModuleID);
          if (bmInfo) {
            printCellVoltages(bmInfo->cellVoltages, pc);
            printSensorTemperatures(bmInfo->sensorTemperatures, pc);
          } else {
            printf("Battery module with id 0x%02x does not exist\r\n", batteryModuleID);
          }
        }
    break;

      case 's':
        printf("Requesting SOC update\r\n");
        plateHandler->updateSOC(0x0);
        break;

      case 'o':
        requestOTA(pc, *plateHandler);
        break;

      case 'h':
        batteryHelp(pc);
        break;

      case 'q':
        return;

      default:
        break;
    }
  }
}
