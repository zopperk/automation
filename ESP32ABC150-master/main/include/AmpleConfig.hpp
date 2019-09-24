/*
 * AmpleConfig.hpp
 */

#ifndef _AMPLECONFIG_HPP_
#define _AMPLECONFIG_HPP_

#include "esp_system.h"
#include "NVSConfig.hpp"
#include "RingLog.hpp"
#include "PCAL6416a.hpp"
#include <driver/adc.h>

namespace CONFIG {

namespace COMMON {
extern const int TIMER_BLOCK_TIME_MS;
extern const int V_REF;
}

namespace WAITTIMES {
extern const int CAPACITY_WAIT_TIME_MS;
extern const int PULSE_WAIT_TIME_MS;
extern const int DRIVECYCLE_WAIT_TIME_MS;
}

namespace CONTACTORS {
extern const PCAL6416a::gpio preChargeCtrlPin;
extern const PCAL6416a::gpio relayNCtrlPin;
extern const PCAL6416a::gpio relayPCtrlPin;
}
namespace RING_LOG {
extern const RingLog::Media media;
extern const uint32_t logFileSize;
}

namespace UI {
/* UI */
extern const uint8_t DEBUG_LOG_TIME_SEC;
}

namespace SIM100 {
extern const uint16_t MAX_WORKING_VOLTAGE;
}

namespace NVS {
extern const char wifiSsid[];
extern const char wifiPass[];
extern const char webSocketURL[];

extern const bool NVS_UPDATE;
extern const NVSConfigRow NVSConfig[18];
}
}

#endif /* _AMPLECONFIG_HPP_ */
