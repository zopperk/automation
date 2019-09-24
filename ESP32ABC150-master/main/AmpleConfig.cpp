/*
 * AmpleConfig.cpp
 */

#include "AmpleConfig.hpp"
#include <limits.h>
#include "PlateCANHandler.hpp"
#include "PlateLogger.hpp"

namespace CONFIG {

namespace COMMON {
const int TIMER_BLOCK_TIME_MS               = 1000;
const int V_REF                             = 1100;

}

namespace WAITTIMES {
const int CAPACITY_WAIT_TIME_MS             = 900000;
const int PULSE_WAIT_TIME_MS                = 10000;
const int DRIVECYCLE_WAIT_TIME_MS           = 900000;
}

namespace CONTACTORS {
const PCAL6416a::gpio preChargeCtrlPin = PCAL6416a::P0_5;
const PCAL6416a::gpio relayNCtrlPin = PCAL6416a::P0_6;
const PCAL6416a::gpio relayPCtrlPin = PCAL6416a::P0_7;
}

namespace UI {
/* UI */
const uint8_t DEBUG_LOG_TIME_SEC            = 1;
}

namespace RING_LOG {
const RingLog::Media media                  = RingLog::SD_CARD;
const uint32_t logFileSize                  = 1 * 1024 * 1024 * 1024; //1 GB
//const uint32_t logFileSize                  = 40000; //1 KB
}

namespace SIM100 {
const uint16_t MAX_WORKING_VOLTAGE          = 402;
}


namespace NVS {

/* 1) If NVS_UPDATE is false, then none of the values from
 *    NVSConfig are updated. NVS is checked for the presence
 *    of each key and an existing value.
 * 2) If NVS_UPDATE is true:
 *          a) If the value != INT_MIN, NVS is update with new value
 *          b) If value == INT_MIN, NVS is checked for the presence of
 *             key similar to case 1)
 *
 * Example int value:
 * {AMPLE_ID,           {1},                       NVS_INT32}
 *
 * Example float value:
 * {CURRENT_OFFSET,     {.floatVal = 1.681},       NVS_FLOAT},
 *
 * Example INT_MIN value that should not be updated:
 * {CURRENT_OFFSET,     {INT_MIN},       NVS_FLOAT},
 * */

const bool NVS_UPDATE                        = false;

const char wifiSsid[]       = "Ample WiFi";
const char wifiPass[]       = "notapple";
const char webSocketURL[]   = "ws://192.168.0.132:8080";

const NVSConfigRow NVSConfig[] = {
    {AMPLE_ID,                      {1},                       NVS_INT32},
    {WIFI_SSID,                     {.charPtr = wifiSsid},     NVS_STRING},
    {WIFI_PASSWORD,                 {.charPtr = wifiPass},     NVS_STRING},
    {WEB_SOCKET_URL,                {.charPtr = webSocketURL}, NVS_STRING},
    {ESP32_CAN_ENABLE,              {1},                       NVS_INT32},
    {MCP2515_CAN_ENABLE,            {1},                       NVS_INT32},
    {WIFI_ENABLE,                   {1},                       NVS_INT32},
    {AMPLE_PROTOCOL_ENABLE,         {1},                       NVS_INT32},
    {AMPLE_LOGGING_ENABLE,          {1},                       NVS_INT32},
    {SIM100_ENABLE,                 {1},                       NVS_INT32},
    {MIN_CAR_OPERATING_VOLTAGE,     {288},                     NVS_INT32},
    {MAX_CAR_OPERATING_VOLTAGE,     {400},                     NVS_INT32},
    {BM_LOG_CRITICAL,               {1},                       NVS_INT32},
    {BM_LOG_GENERAL,                {0},                       NVS_INT32},
    {BM_LOG_FLAGS,                  {0},                       NVS_INT32},
    {BM_LOG_ADDITIONAL,             {0},                       NVS_INT32},
    {PLATE_LOGGER_INTERVAL_MS,      {100},                     NVS_INT32},
    {PLATE_LOGGER_DETAILED_INTERVAL_MS,  {60000},              NVS_INT32},
};
}

}
