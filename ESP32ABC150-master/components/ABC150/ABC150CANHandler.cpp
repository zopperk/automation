/*
 * ABC150CANHandler.cpp
 */

#include "ABC150CANHandler.hpp"
#include "esp_log.h"
#include <sstream>
#include "TimeUtils.hpp"

#define DATA_A                      0x100 // PPS → PC's
#define DATA_B                      0x120 // PPS → PC's

#define LOWER_LIMITS_A              0x101 // PPS → PC's
#define UPPER_LIMITS_A              0x102 // PPS → PC's
#define CONNECTOR_STATUS_NEGATIVE   0x01
#define CONNECTOR_STATUS_POSITIVE   0x02
#define CONNECTOR_STATUS_INTERLOCK  0x04
#define STATUS_A                    0x103  // PPS → PC's
#define STATION_ID_A                0x104  // PPS → PC's

#define LOWER_LIMITS_B              0x121  // PPS → PC's
#define UPPER_LIMITS_B              0x122  // PPS → PC's
#define STATUS_B                    0x123  // PPS → PC's
#define STATION_ID_B                0x124  // PPS → PC's


#define GREETING                    0x140  // PPS → xxx
#define PC_GREETING                 0x1C0  // PC → PPS
#define FAULT_DATA                  0x141  // PPS → PC's
#define PACKET_PROBLEM              0x142  // PPS → PC's
#define COMMAND_A                   0x180  // PC → PPS
#define LOWER_LIMITS_A_OUT          0x181  // PC → PPS
#define UPPER_LIMITS_A_OUT          0x182  // PC → PPS
#define COMMAND_B                   0x1A0  // PC → PPS
#define LOWER_LIMITS_B_OUT          0x1A1  // PC → PPS
#define UPPER_LIMIT_B_OUT           0x1A2  // PC → PPS
#define CHANGE_CONTROL              0x1C1  // PC → PPS
#define RVS_MODE_REQUEST            0x1C2  // PC → PPS
#define REQUEST_PC                  0x160  // xxx → PC's
#define REQUEST_ABC                 0x1E0  // xxx → PPS

#define CONTROL_MODE                (0b11)
#define NORMAL_MODE                 (0b100)
#define ENABLE_MODE                 (0b1000)
#define LOAD_MODE                   (0b110000)
#define RVS_MODE                    (0b1000000)
#define VOLTAGE_SCALE               (0.02)
#define CURRENT_SCALE               (0.02)
#define POWER_SCALE                 (5)


ABC150CANHandler::ABC150CANHandler(AmpleCAN &_can):
                                  ampleCAN(_can),
                                  hardwareVersion(0),
                                  faultID(0),
                                  moduleID(0),
                                  problemID(0),
                                  suppID(0),
                                  abcDetected(false),
								                  xFrequency(500){
  ampleCAN.registerListener(DATA_A, this);
  ampleCAN.registerListener(DATA_B, this);
  ampleCAN.registerListener(LOWER_LIMITS_A, this);
  ampleCAN.registerListener(UPPER_LIMITS_A, this);
  ampleCAN.registerListener(STATUS_A, this);
  ampleCAN.registerListener(STATION_ID_A, this);
  ampleCAN.registerListener(LOWER_LIMITS_B, this);
  ampleCAN.registerListener(UPPER_LIMITS_B, this);
  ampleCAN.registerListener(STATUS_B, this);
  ampleCAN.registerListener(STATION_ID_B, this);
  ampleCAN.registerListener(GREETING, this);
  ampleCAN.registerListener(FAULT_DATA, this);
  ampleCAN.registerListener(PACKET_PROBLEM, this);
  ampleCAN.registerListener(REQUEST_PC, this);

  channelInfo[A].controlModeOut = Standby;
  channelInfo[A].loadModeOut = Independent;
  channelInfo[B].controlModeOut = Standby;
  channelInfo[B].loadModeOut = Independent;


  /* Create send task */
  if (xTaskCreate(&ABC150CANHandler::sendTaskWrapper, "ABC150 send", 4096, this, configMAX_PRIORITIES-2, &sendTaskHandle) != pdPASS){
    ESP_LOGE(TAG, "Failed to create ABC150 Send Task");
  }

  //sendPCGreeting();
  sendRequestABCPackage();
}


ABC150CANHandler::~ABC150CANHandler() {
  vTaskDelete(sendTaskHandle);
}

bool ABC150CANHandler::channelCheck(Channel channel) {
  if (channel != A && channel != B) {
    ESP_LOGE(TAG, "Invalid channel");
    return false;
  }
  return true;
}

bool ABC150CANHandler::setLowerVoltageLimit(Channel channel, float voltage) {
  if (!channelCheck(channel)) return false;
  channelInfo[channel].lowerVoltageLimitOut = voltage;
  return true;
}

bool ABC150CANHandler::setLowerCurrentLimit(Channel channel, float current) {
  if (!channelCheck(channel)) return false;
  channelInfo[channel].lowerCurrentLimitOut = current;
  return true;
}

bool ABC150CANHandler::setLowerPowerLimit(Channel channel, float power) {
  if (!channelCheck(channel)) return false;
  channelInfo[channel].lowerPowerLimitOut = power;
  return true;
}

bool ABC150CANHandler::setUpperVoltageLimit(Channel channel, float voltage) {
  if (!channelCheck(channel)) return false;
  channelInfo[channel].upperVoltageLimitOut = voltage;
  return true;
}

bool ABC150CANHandler::setUpperCurrentLimit(Channel channel, float current) {
  if (!channelCheck(channel)) return false;
  channelInfo[channel].upperCurrentLimitOut = current;
  return true;
}

bool ABC150CANHandler::setUpperPowerLimit(Channel channel, float power) {
  if (!channelCheck(channel)) return false;
  channelInfo[channel].upperPowerLimitOut = power;
  return true;
}

void ABC150CANHandler::setControlMode(Channel channel, ControlMode controlMode) {
  channelInfo[channel].controlModeOut = controlMode;
}

bool ABC150CANHandler::setVoltage(Channel channel, float voltage) {
  if (!channelCheck(channel)) return false;
  channelInfo[channel].commandOut = voltage;
  setControlMode(channel, Voltage);
  return true;
}

bool ABC150CANHandler::setCurrent(Channel channel, float current) {
  if (!channelCheck(channel)) return false;
  channelInfo[channel].commandOut = current;
  setControlMode(channel, Current);
  return true;
}

void ABC150CANHandler::sendPackage(Channel channel) {
	  sendCommandPackage(channel);
	  vTaskDelay(pdMS_TO_TICKS(1));
	  sendLowerLimits(channel);
	  vTaskDelay(pdMS_TO_TICKS(1));
	  sendUpperLimits(channel);
	  vTaskDelay(pdMS_TO_TICKS(1));
}

bool ABC150CANHandler::setPower(Channel channel, float power) {
  if (!channelCheck(channel)) return false;
  channelInfo[channel].commandOut = power;
  setControlMode(channel, Power);
  return true;
}

bool ABC150CANHandler::setLoadMode(Channel channel, LoadMode loadMode) {
  if (!channelCheck(channel)) return false;
  channelInfo[channel].loadModeOut = loadMode;
  return true;
}

std::string ABC150CANHandler::getPacketProblemString(uint8_t problem, uint8_t supp) {
  std::stringstream ss;
      if (suppID == 0) ss << "Channel A\t";
      else if (suppID == 1) ss << "Channel B\t";
      else ss << "Channel Unknown\t";

      switch (problem) {
      case Command_out_of_limits :
        ss << "Command_out_of_limits";
        break;
      case UVL_too_high :
        ss << "UVL_too_high";
        break;
      case UVL_too_low :
        ss << "UVL_too_low";
        break;
      case LVL_too_high :
        ss << "LVL_too_high";
        break;
      case LVL_too_low :
        ss << "LVL_too_low";
        break;
      case UCL_too_high :
        ss << "UCL_too_high";
        break;
      case UCL_too_low :
        ss << "UCL_too_low";
        break;
      case LCL_too_high :
        ss << "LCL_too_high";
        break;
      case LCL_too_low :
        ss << "LCL_too_low";
        break;
      case UPL_too_high :
        ss << "UPL_too_high";
        break;
      case UPL_too_low :
        ss << "UPL_too_low";
        break;
      case LPL_too_high :
        ss << "LPL_too_high";
        break;
      case LPL_too_low :
        ss << "LPL_too_low";
        break;
      case Invalid_config :
        ss << "Invalid_config";
        break;
      case Invalid_mode :
        ss << "Invalid_mode";
        break;
      case UV_lower_than_LV :
        ss << "UV_lower_than_LV";
        break;
      case UI_lower_than_LI :
        ss << "UI_lower_than_LI";
        break;
      case UP_lower_than_LP :
        ss << "UP_lower_than_LP";
        break;
      case Invalid_control_source :
        ss << "Invalid_control_source";
        break;
      case Invalid_mode1 :
        ss << "Invalid_mode1";
        break;
      case Not_in_remote_control :
        ss << "Not_in_remote_control";
        break;
      case Invalid_station_id_received :
        ss << "Invalid_station_id_received";
        break;
      case load_v_incompatible :
        ss << "load_v_incompatible";
        break;
      case power_allocation_erro :
        ss << "power_allocation_erro";
        break;
      case too_much_command :
        ss << "too_much_command";
        break;
      case out_of_op_space :
        ss << "out_of_op_space";
        break;
      case invalid_channel :
        ss << "invalid_channel";
        break;
      case invalid_sw_version :
        ss << "invalid_sw_version";
        break;
      case wrong_length :
        ss << "wrong_length";
        break;
      case unknown_type :
        ss << "unknown_type";
        break;
      default:
        ss << "Problem not in list";
        break;
      }
      ss << std::endl;

      return ss.str();
}

std::string ABC150CANHandler::getControlModeString(ControlMode controlMode) {
  std::stringstream ss;

  if (controlMode == Voltage) ss << "Voltage";
  else if (controlMode == Current) ss << "Current";
  else if (controlMode == Power) ss << "Power";
  else if (controlMode == Standby) ss << "Standby";
  else ss << "Unknown";
  ss << std::endl;
  return ss.str();
}

std::string ABC150CANHandler::getLoadModeString(LoadMode loadMode) {
  std::stringstream ss;

  if (loadMode == Independent) ss << "Independent";
  else if (loadMode == Parallel) ss << "Parallel";
  else if (loadMode == Differential) ss << "Differential";
  else if (loadMode == Do_not_Change) ss << "Do_not_Change";
  else ss << "Unknown" << std::endl;
  ss << std::endl;
  return ss.str();
}

void ABC150CANHandler::printInfo() {
  std::stringstream ss;
  for (int i = 0; i < 2; i++) {
    ss << "*****************************************" << std::endl;
    ss << "\n\n\r\tChannel " << i << std::endl;
    ss << "Voltage: " << channelInfo[i].voltage << std::endl;
    ss << "Current: " << channelInfo[i].current << std::endl;
    ss << "timestamp: " << channelInfo[i].timestamp << std::endl;
    ss << "lowerVoltageLimit: " << channelInfo[i].lowerVoltageLimit << std::endl;
    ss << "lowerCurrentLimit: " << channelInfo[i].lowerCurrentLimit << std::endl;
    ss << "lowerPowerLimit: " << channelInfo[i].lowerPowerLimit << std::endl;
    ss << "upperVoltageLimit: " << channelInfo[i].upperVoltageLimit << std::endl;
    ss << "upperCurrentLimit: " << channelInfo[i].upperCurrentLimit << std::endl;
    ss << "upperPowerLimit: " << channelInfo[i].upperPowerLimit << std::endl;
    ss << "command: " << channelInfo[i].command << std::endl;

    ss << "converterStatus: ";
    if (channelInfo[i].converterStatus == Local) {
      ss << "Local" << std::endl;
    } else if (channelInfo[i].converterStatus == Remote) {
      ss << "Remote" << std::endl;
    } else if (channelInfo[i].converterStatus == J1850) {
      ss << "J1850" << std::endl;
    } else ss << "Unknown" << std::endl;

    ss << "controlMode: ";
    ss << getControlModeString(channelInfo[i].controlMode);

    ss << "normalMode: ";
    if (channelInfo[i].normalMode == Normal) {
      ss << "Normal" << std::endl;
    } else if (channelInfo[i].normalMode == Protected_Standby) {
      ss << "Protected_Standby" << std::endl;
    } else ss << "Unknown" << std::endl;

    ss << "enableMode: ";
    if (channelInfo[i].enableMode == Enabled) {
      ss << "Enabled" << std::endl;
    } else if (channelInfo[i].enableMode == Disabled) {
      ss << "Disabled" << std::endl;
    } else ss << "Unknown" << std::endl;

    ss << "loadMode: ";
    ss << getLoadModeString(channelInfo[i].loadMode);

    ss << "rvsMode: ";
    if (channelInfo[i].rvsMode == RVS_off) {
      ss << "RVS_off" << std::endl;
    } else if (channelInfo[i].rvsMode == RVS_on) {
      ss << "RVS_on" << std::endl;
    } else ss << "Unknown" << std::endl;

    ss << "stationID: " << channelInfo[i].stationID << std::endl;
    ss << "stationIDSet: " << channelInfo[i].stationIDSet << std::endl;
    ss << "counterStamp: " << (int)channelInfo[i].counterStamp << std::endl;
    ss << "lowerVoltageLimitOut: " << channelInfo[i].lowerVoltageLimitOut << std::endl;
    ss << "lowerCurrentLimitOut: " << channelInfo[i].lowerCurrentLimitOut << std::endl;
    ss << "lowerPowerLimitOut: " << channelInfo[i].lowerPowerLimitOut << std::endl;
    ss << "upperVoltageLimitOut: " << channelInfo[i].upperVoltageLimitOut << std::endl;
    ss << "upperCurrentLimitOut: " << channelInfo[i].upperCurrentLimitOut << std::endl;
    ss << "upperPowerLimitOut: " << channelInfo[i].upperPowerLimitOut << std::endl;
    ss << "commandOut: " << channelInfo[i].commandOut << std::endl;

    ss << "controlModeOut: ";
    ss << getControlModeString(channelInfo[i].controlModeOut);

    ss << "loadModeOut: ";
    ss << getLoadModeString(channelInfo[i].loadModeOut);

    ss << "enable: " << channelInfo[i].enable << std::endl;
  }

  ss << "*****************************************" << std::endl;

  ss << "hardwareVersion: " << hardwareVersion << std::endl;
  ss << "swVersion: " << swVersion << std::endl;

  ss << "faultID: " << (int)faultID << std::endl;
  ss << "moduleID: " << (int)moduleID << std::endl;
  ss << getPacketProblemString(problemID, suppID);

  ss << "abcDetected: " << abcDetected << std::endl;
  std::cout << ss.str();

}

void ABC150CANHandler::handleData(Channel channel, CAN_frame_t &msg) {
  channelInfo[channel].voltage = ((int16_t)((uint16_t)msg.data.u8[0] << 8) | (uint16_t)msg.data.u8[1]) * VOLTAGE_SCALE;
  channelInfo[channel].current = ((int16_t)((uint16_t)msg.data.u8[2] << 8) | (uint16_t)msg.data.u8[3]) * CURRENT_SCALE;
  channelInfo[channel].timestamp = (((uint32_t)msg.data.u8[4] << 24) |
                                    ((uint32_t)msg.data.u8[5] << 16) |
                                    ((uint32_t)msg.data.u8[6] << 8) |
                                     (uint32_t)msg.data.u8[7]);
}

void ABC150CANHandler::handleLowerLimits(Channel channel, CAN_frame_t &msg) {
  channelInfo[channel].lowerVoltageLimit = ((int16_t)((uint16_t)msg.data.u8[0] << 8) | (uint16_t)msg.data.u8[1]) * VOLTAGE_SCALE;
  channelInfo[channel].lowerCurrentLimit = ((int16_t)((uint16_t)msg.data.u8[2] << 8) | (uint16_t)msg.data.u8[3]) * CURRENT_SCALE;
  channelInfo[channel].lowerPowerLimit = ((int16_t)((uint16_t)msg.data.u8[4] << 8) | (uint16_t)msg.data.u8[5]) * POWER_SCALE;
}

void ABC150CANHandler::handleUpperLimits(Channel channel, CAN_frame_t &msg) {
  channelInfo[channel].upperVoltageLimit = ((int16_t)((uint16_t)msg.data.u8[0] << 8) | (uint16_t)msg.data.u8[1]) * VOLTAGE_SCALE;
  channelInfo[channel].upperCurrentLimit = ((int16_t)((uint16_t)msg.data.u8[2] << 8) | (uint16_t)msg.data.u8[3]) * CURRENT_SCALE;
  channelInfo[channel].upperPowerLimit = ((int16_t)((uint16_t)msg.data.u8[4] << 8) | (uint16_t)msg.data.u8[5]) * POWER_SCALE;
}

void ABC150CANHandler::handleStatus(Channel channel, CAN_frame_t &msg) {
  if ((channelInfo[channel].converterStatus) != (ConverterStatus)msg.data.u8[2]) {
    ESP_LOGI(TAG, "Converter status changed to %d", msg.data.u8[2]);
  }
  channelInfo[channel].converterStatus = (ConverterStatus)msg.data.u8[2];
  channelInfo[channel].connectorStatusPositive = ((msg.data.u8[4] & CONNECTOR_STATUS_POSITIVE) == CONNECTOR_STATUS_POSITIVE);
  channelInfo[channel].connectorStatusNegative = ((msg.data.u8[4] & CONNECTOR_STATUS_NEGATIVE) == CONNECTOR_STATUS_NEGATIVE);
  channelInfo[channel].connectorStatusInterlock = ((msg.data.u8[4] & CONNECTOR_STATUS_INTERLOCK) == CONNECTOR_STATUS_INTERLOCK);
  channelInfo[channel].controlMode = (ControlMode)(msg.data.u8[3] & CONTROL_MODE);
  channelInfo[channel].normalMode = (NormalMode)((msg.data.u8[3] & NORMAL_MODE) >> 2);
  channelInfo[channel].enableMode = (EnableMode)((msg.data.u8[3] & ENABLE_MODE) >> 3);
  channelInfo[channel].loadMode = (LoadMode)((msg.data.u8[3] & LOAD_MODE) >> 4);
  channelInfo[channel].rvsMode = (RVSMode)((msg.data.u8[3] & RVS_MODE) >> 6);
  switch(channelInfo[channel].controlMode) {
  case Voltage:
  case Current:
    channelInfo[channel].command = ((int16_t)((uint16_t)msg.data.u8[0] << 8) | (uint16_t)msg.data.u8[1]) * VOLTAGE_SCALE;
    break;
  case Power:
    channelInfo[channel].command = ((int16_t)((uint16_t)msg.data.u8[0] << 8) | (uint16_t)msg.data.u8[1]) * POWER_SCALE;
    break;
  default:
    channelInfo[channel].command = ((int16_t)((uint16_t)msg.data.u8[0] << 8) | (uint16_t)msg.data.u8[1]);
    break;
  }
}

void ABC150CANHandler::handleStationID(Channel channel, CAN_frame_t &msg) {
  if (!channelInfo[channel].stationIDSet) {
    channelInfo[channel].stationID = (((uint64_t)msg.data.u8[0] << 32) |
        ((uint64_t)msg.data.u8[1] << 24) |
        ((uint64_t)msg.data.u8[2] << 16) |
         ((uint64_t)msg.data.u8[3] << 8) |
         (msg.data.u8[4]));
    channelInfo[channel].stationIDSet = true;
  }
}

void ABC150CANHandler::handleGreeting(CAN_frame_t &msg) {
  swVersion = (((uint32_t)msg.data.u8[0] << 24) |
      ((uint32_t)msg.data.u8[1] << 16) |
       ((uint32_t)msg.data.u8[2] << 8) |
       (msg.data.u8[3]));
  hardwareVersion = msg.data.u8[4];
  if (hardwareVersion == 0x0D) {
    ESP_LOGI(TAG, "ABC150 Detected");
    abcDetected = true;
    return;
  }  else {
    ESP_LOGE(TAG, "Hardware Version: %d", hardwareVersion);
  }
  abcDetected = false;
}

void ABC150CANHandler::handleFaultData(CAN_frame_t &msg) {
  faultID = msg.data.u8[0];
  moduleID = msg.data.u8[1];
  ESP_LOGE(TAG, "Received faultID: %d; moduleID: %d", faultID, moduleID);
}

void ABC150CANHandler::handlePacketProblem(CAN_frame_t &msg) {
  uint8_t problem = msg.data.u8[0];
  uint8_t supp = msg.data.u8[1];

  if ((problemID != problem) || suppID != supp) {

    ESP_LOGE(TAG, "%s", getPacketProblemString(problem, supp).c_str());
  }
  problemID = problem;
  suppID = supp;
}

void ABC150CANHandler::handleRequestPC(CAN_frame_t &msg) {
  uint16_t canID = (((uint16_t)msg.data.u8[0] << 8) | (uint16_t)msg.data.u8[1]);

  if (canID == PC_GREETING) {
    sendPCGreeting();
  } else {
    ESP_LOGI(TAG, "Request for CAN ID 0x%02x", canID);
  }
}


void ABC150CANHandler::msgReceived(CAN_frame_t &msg) {
  switch(msg.MsgID) {
  case DATA_A:
    handleData(A, msg);
    break;
  case DATA_B:
    handleData(B, msg);
    break;
  case LOWER_LIMITS_A:
    handleLowerLimits(A, msg);
    break;
  case UPPER_LIMITS_A:
    handleUpperLimits(A, msg);
    break;
  case STATUS_A:
    handleStatus(A, msg);
    break;
  case STATION_ID_A:
    handleStationID(A, msg);
    break;
  case LOWER_LIMITS_B:
    handleLowerLimits(B, msg);
    break;
  case UPPER_LIMITS_B:
    handleUpperLimits(B, msg);
    break;
  case STATUS_B:
    handleStatus(B, msg);
    break;
  case STATION_ID_B:
    handleStationID(B, msg);
    break;
  case GREETING:
    handleGreeting(msg);
    break;
  case FAULT_DATA:
    handleFaultData(msg);
    break;
  case PACKET_PROBLEM:
    handlePacketProblem(msg);
    break;
  case REQUEST_PC:
    handleRequestPC(msg);
    break;
  }
}

void ABC150CANHandler::sendPCGreeting() {
  CAN_frame_t msg;
  msg.MsgID = PC_GREETING;
  msg.FIR.B.FF = CAN_frame_std;
  msg.FIR.B.DLC = 6;
//  msg.data.u8[0] = ((swVersion >> 24) & 0xFF);
//  msg.data.u8[1] = ((swVersion >> 16) & 0xFF);
//  msg.data.u8[2] = ((swVersion >> 8) & 0xFF);
//  msg.data.u8[3] = (swVersion & 0xFF);
//  msg.data.u8[5] = 0x0;
//  msg.data.u8[4] = hardwareVersion;
  ampleCAN.can.CAN_write_frame(&msg);
}

void ABC150CANHandler::sendCommandPackage(Channel channel) {
  CAN_frame_t msg;
  int16_t scaledValue = 0;
  msg.MsgID = COMMAND_A + (channel * 0x20);
  msg.FIR.B.FF = CAN_frame_std;
  msg.FIR.B.DLC = 4;
  msg.data.u8[0] = channelInfo[channel].counterStamp;
  switch(channelInfo[channel].controlModeOut) {
    case Voltage:
      scaledValue = channelInfo[channel].commandOut / VOLTAGE_SCALE;
      break;
    case Current:
      scaledValue = channelInfo[channel].commandOut / CURRENT_SCALE;
      break;
    case Power:
      scaledValue = channelInfo[channel].commandOut / POWER_SCALE;
      break;
    default:
      scaledValue = 0;
      break;
    }

  msg.data.u8[1] = (scaledValue >> 8) & 0xFF;
  msg.data.u8[2] = (scaledValue) & 0xFF;
  if (channelInfo[channel].enable) {
    msg.data.u8[3] = channelInfo[channel].controlModeOut | (channelInfo[channel].loadModeOut << 4) ;
  } else {
    // Bit5-4 : 11: Do not Change
    // Bit1-0 : 11: Standby
    msg.data.u8[3] = Standby | (channelInfo[channel].loadModeOut << 4);
  }
  ampleCAN.can.CAN_write_frame(&msg);
}

void ABC150CANHandler::sendLowerLimits(Channel channel) {
  CAN_frame_t msg;
  msg.MsgID = LOWER_LIMITS_A_OUT + (channel * 0x20);;
  msg.FIR.B.FF = CAN_frame_std;
  msg.FIR.B.DLC = 7;
  int16_t lvoltage = channelInfo[channel].lowerVoltageLimitOut / VOLTAGE_SCALE;
  int16_t lcurrent = channelInfo[channel].lowerCurrentLimitOut / CURRENT_SCALE;
  int16_t lpower = channelInfo[channel].lowerPowerLimitOut / POWER_SCALE;
  msg.data.u8[0] = channelInfo[channel].counterStamp;
  msg.data.u8[1] = ((lvoltage >> 8) & 0xFF);
  msg.data.u8[2] = lvoltage & 0xFF;
  msg.data.u8[3] = ((lcurrent >> 8) & 0xFF);
  msg.data.u8[4] = lcurrent & 0xFF;
  msg.data.u8[5] = ((lpower >> 8) & 0xFF);
  msg.data.u8[6] = lpower & 0xFF;
  ampleCAN.can.CAN_write_frame(&msg);
}

void ABC150CANHandler::sendUpperLimits(Channel channel) {
  CAN_frame_t msg;
  msg.MsgID = UPPER_LIMITS_A_OUT + (channel * 0x20);;
  msg.FIR.B.FF = CAN_frame_std;
  msg.FIR.B.DLC = 7;
  int16_t uvoltage = channelInfo[channel].upperVoltageLimitOut / VOLTAGE_SCALE;
  int16_t ucurrent = channelInfo[channel].upperCurrentLimitOut / CURRENT_SCALE;
  int16_t upower = channelInfo[channel].upperPowerLimitOut / POWER_SCALE;
  msg.data.u8[0] = channelInfo[channel].counterStamp;
  msg.data.u8[1] = ((uvoltage >> 8) & 0xFF);
  msg.data.u8[2] = uvoltage & 0xFF;
  msg.data.u8[3] = ((ucurrent >> 8) & 0xFF);
  msg.data.u8[4] = ucurrent & 0xFF;
  msg.data.u8[5] = ((upower >> 8) & 0xFF);
  msg.data.u8[6] = upower & 0xFF;
  ampleCAN.can.CAN_write_frame(&msg);

  /* Counter stamp gets incremented here */
  channelInfo[channel].counterStamp++;

}

void ABC150CANHandler::sendChangeControl(Channel channel) {
  CAN_frame_t msg;
  msg.MsgID = CHANGE_CONTROL;
  msg.FIR.B.FF = CAN_frame_std;
  msg.FIR.B.DLC = 8;
  msg.data.u8[0] = channel;
  msg.data.u8[0] = msg.data.u8[0] | (channelInfo[channel].converterStatus << 1); //From
  msg.data.u8[0] = msg.data.u8[0] | (Remote << 4); //To
  msg.data.u8[1] = hardwareVersion; // Hardware ID for ABC150
  msg.data.u8[3] = ((channelInfo[channel].stationID >> 32) & 0xFF);
  msg.data.u8[4] = ((channelInfo[channel].stationID >> 24) & 0xFF);
  msg.data.u8[5] = ((channelInfo[channel].stationID >> 16) & 0xFF);
  msg.data.u8[6] = ((channelInfo[channel].stationID >> 8) & 0xFF);
  msg.data.u8[7] = ((channelInfo[channel].stationID) & 0xFF);
  ampleCAN.can.CAN_write_frame(&msg);
}

void ABC150CANHandler::sendRequestABCPackage() {
  CAN_frame_t msg;
  msg.MsgID = REQUEST_ABC;
  msg.FIR.B.FF = CAN_frame_std;
  msg.FIR.B.DLC = 2;
  msg.data.u8[0] = (GREETING >> 8) & 0xFF;
  msg.data.u8[1] = (GREETING) & 0xFF;
  ampleCAN.can.CAN_write_frame(&msg);
}

void  ABC150CANHandler::takeControl(Channel channel) {
  sendPCGreeting();
  sendChangeControl(channel);
  channelInfo[channel].sending = true;
}

void  ABC150CANHandler::releaseControl(Channel channel) {
  channelInfo[channel].sending = false;
}



bool ABC150CANHandler::enable(Channel channel) {
  if (abcDetected == false) {
    ESP_LOGE(TAG, "ABC150 not detected");
    return false;
  }

  if (channelInfo[channel].controlModeOut > Power) {
    ESP_LOGE(TAG, "ABC150 controlModeOut not set");
    return false;
  }

  channelInfo[channel].enable = true;
  return true;
}

bool ABC150CANHandler::disable(Channel channel) {
  channelInfo[channel].enable = false;
  channelInfo[channel].commandOut = 0;
  channelInfo[channel].controlModeOut = Standby;
  return true;
}

bool ABC150CANHandler::isDetected() {
  return abcDetected;
}

float ABC150CANHandler::getVoltage(Channel channel) {
  return channelInfo[channel].voltage;
}

float ABC150CANHandler::getCurrent(Channel channel) {
  return channelInfo[channel].current;
}

uint32_t ABC150CANHandler::getTimeStamp(Channel channel) {
  return channelInfo[channel].timestamp;
}

ABC150CANHandler::ControlMode ABC150CANHandler::getControlModeOut(Channel ch){
	return channelInfo[ch].controlModeOut;
}

ABC150CANHandler::LoadMode ABC150CANHandler::getLoadModeOut(Channel ch){
	return channelInfo[ch].loadModeOut;
}
ABC150CANHandler::ConverterStatus ABC150CANHandler::getConverterStatus(Channel ch){
	return channelInfo[ch].converterStatus;
}
float ABC150CANHandler::getCommand(Channel ch){
	return channelInfo[ch].command;
}


void ABC150CANHandler::sendTaskWrapper(void *arg) {
  ABC150CANHandler * obj =  (ABC150CANHandler *)arg;
  obj->sendTask();
}

void ABC150CANHandler::setFrequency(int timeDelta) {
	int taskFrequency = timeDelta;
	xFrequency = pdMS_TO_TICKS(taskFrequency); //Run 500ms
}

void ABC150CANHandler::setDefaultFrequency() {
	xFrequency = pdMS_TO_TICKS(500);
}

void ABC150CANHandler::sendTask() {
  CAN_frame_t canMsg;
  xLastWakeTime = xTaskGetTickCount();
  while (1) {
      // Wait for the next cycle.
      vTaskDelayUntil(&xLastWakeTime, xFrequency);
      if (channelInfo[A].sending && channelInfo[A].converterStatus == Remote) {
        sendPackage(A);
      }

      if (channelInfo[B].sending && channelInfo[B].converterStatus == Remote) {
        sendPackage(B);
      }
  }

}

void ABC150CANUserInterface::help() {
  printf("\r\n");
  printf(GREEN "ABC150 CAN Interface.\r\n" RESET);

  printf("  p: Print info\r\n");

  printf("  1: setLowerVoltageLimit\r\n");
  printf("  2: setLowerCurrentLimit\r\n");
  printf("  3: setLowerPowerLimit\r\n");
  printf("  4: setUpperVoltageLimit\r\n");
  printf("  5: setUpperCurrentLimit\r\n");
  printf("  6: setUpperPowerLimit\r\n");
  printf("  7: setVoltage\r\n");
  printf("  8: setCurrent\r\n");
  printf("  9: setPower\r\n");
  printf("  0: setLoadMode\r\n");
  printf("  e: enable\r\n");
  printf("  d: disable\r\n");
  printf("  t: take control\r\n");
  printf("  r: release control\r\n");
  printf("  s: sendRequestABCPackage\r\n");

  printf("  h: Print this help again\r\n");
  printf("  q: Quit\r\n\n\n");

}

bool ABC150CANUserInterface::getInput(const char *printString, AmpleSerial &pc, ABC150CANHandler::Channel *ch, float *val = NULL) {
  int channel;
  printf("%s:\r\n", printString);
  if(pc.readNumber(channel, "Enter channel (0 for A and 1 for B): ")) {
    if (channel == 0 || channel == 1) {
      *ch = (ABC150CANHandler::Channel)channel;
      if (val == NULL) return true;
      if (pc.readFloatNumber(*val, "Enter value: ")) {
        return true;
      }
    } else {
      printf("Invalid channel\r\n");
    }
  }
  return false;
}


void ABC150CANUserInterface::ABC150CANInterface(AmpleSerial &pc, ABC150CANHandler *canHandler) {
  ABC150CANHandler::Channel ch;
  float val;

  pc.clear();
  ABC150CANUserInterface::help();
  while(1) {
      char input;
      printf("ABC150> ");
      fflush(stdout);
      input = pc.rx_char();
      printf("%c\r\n", input);
      switch(input) {
      case 'p':
        canHandler->printInfo();
        break;

      case '1':
        if (getInput("setLowerVoltageLimit", pc, &ch, &val)) {
          canHandler->setLowerVoltageLimit(ch, val);
        }
        break;

      case '2':
        if (getInput("setLowerCurrentLimit", pc, &ch, &val)) {
          canHandler->setLowerCurrentLimit(ch, val);
        }
        break;

      case '3':
        if (getInput("setLowerPowerLimit", pc, &ch, &val)) {
          canHandler->setLowerPowerLimit(ch, val);
        }
        break;

      case '4':
        if (getInput("setUpperVoltageLimit", pc, &ch, &val)) {
          canHandler->setUpperVoltageLimit(ch, val);
        }
        break;

      case '5':
        if (getInput("setUpperCurrentLimit", pc, &ch, &val)) {
          canHandler->setUpperCurrentLimit(ch, val);
        }
        break;

      case '6':
        if (getInput("setUpperPowerLimit", pc, &ch, &val)) {
          canHandler->setUpperPowerLimit(ch, val);
        }
        break;

      case '7':
        if (getInput("setVoltage", pc, &ch, &val)) {
          canHandler->setVoltage(ch, val);
        }
        break;

      case '8':
        if (getInput("setCurrent", pc, &ch, &val)) {
          canHandler->setCurrent(ch, val);
        }
        break;

      case '9':
        if (getInput("setPower", pc, &ch, &val)) {
          canHandler->setPower(ch, val);
        }
        break;

      case '0':
        if (getInput("setLoadMode", pc, &ch, &val)) {
          canHandler->setLoadMode(ch, (ABC150CANHandler::LoadMode)val);
        }
        break;

      case 'e':
        if (getInput("Enable", pc, &ch)) {
          canHandler->enable(ch);
        }
        break;

      case 'd':
        if (getInput("Disable", pc, &ch)) {
          canHandler->disable(ch);
        }
        break;

      case 't':
        if (getInput("Take control", pc, &ch)) {
          canHandler->takeControl(ch);
        }
        break;

      case 'r':
        if (getInput("Release control", pc, &ch)) {
          canHandler->releaseControl(ch);
        }
        break;

      case 's':
        printf("Sending request ABC Package \r\n");
          canHandler->sendRequestABCPackage();
        break;

      case 'h':
        ABC150CANUserInterface::help();
        break;

      case 'q':
        return;
        break;

      default:
        break;

      }
  }
}
