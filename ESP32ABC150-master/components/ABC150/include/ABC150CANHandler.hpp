/*
 * ABC150CANHandler.hpp
 */

#ifndef _ABC150CANHANDLER_HPP_
#define _ABC150CANHANDLER_HPP_

#include "AmpleCAN.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "AmpleSerial.hpp"


class ABC150CANHandler: public AmpleCANListener {
public:
  enum Problem_ID               {
                                       Command_out_of_limits = 0x1,
                                       UVL_too_high,
                                       UVL_too_low,
                                       LVL_too_high,
                                       LVL_too_low,
                                       UCL_too_high,
                                       UCL_too_low,
                                       LCL_too_high,
                                       LCL_too_low,
                                       UPL_too_high,
                                       UPL_too_low,
                                       LPL_too_high,
                                       LPL_too_low,
                                       Invalid_config,
                                       Invalid_mode,
                                       UV_lower_than_LV,
                                       UI_lower_than_LI,
                                       UP_lower_than_LP,
                                       Invalid_control_source,
                                       Invalid_mode1 = 0x15,
                                       Not_in_remote_control = 0x20,
                                       Invalid_station_id_received = 0x21,
                                       load_v_incompatible = 0x80,
                                       power_allocation_erro = 0x81,
                                       too_much_command = 0x82,
                                       out_of_op_space = 0x8F,
                                       invalid_channel = 0xF0,
                                       invalid_sw_version = 0xFD,
                                       wrong_length = 0xFE,
                                       unknown_type = 0xFF
                                      };
  enum Supp_ID                  {channel_A, channel_B};
  enum Channel                  {A,B};
  enum ConverterStatus          {Local, Remote, J1850};
  enum ControlMode              {Voltage, Current, Power, Standby};
  enum NormalMode               {Normal, Protected_Standby};
  enum EnableMode               {Enabled, Disabled};
  enum LoadMode                 {Independent, Parallel, Differential, Do_not_Change};
  enum RVSMode                  {RVS_off, RVS_on};
private:
  AmpleCAN &ampleCAN;

  class ChannelInfo {
  public:
    float voltage;
    float current;
    uint32_t timestamp;
    float lowerVoltageLimit;
    float lowerCurrentLimit;
    float lowerPowerLimit;
    float upperVoltageLimit;
    float upperCurrentLimit;
    float upperPowerLimit;
    float command;
    ConverterStatus converterStatus;
    ControlMode controlMode; //voltage, current, power, standby
    NormalMode normalMode;  // normal, protected standby
    EnableMode enableMode; // enabled, disabled
    LoadMode loadMode; // independent, parallel, differential, unselected
    RVSMode rvsMode;
    bool connectorStatusNegative;
    bool connectorStatusPositive;
    bool connectorStatusInterlock;
    uint64_t stationID;
    bool stationIDSet;
    uint8_t counterStamp;
    float lowerVoltageLimitOut;
    float lowerCurrentLimitOut;
    float lowerPowerLimitOut;
    float upperVoltageLimitOut;
    float upperCurrentLimitOut;
    float upperPowerLimitOut;
    float commandOut;
    ControlMode controlModeOut; //voltage, current, power, standby
    LoadMode loadModeOut; // independent, parallel, differential, unselected
    bool enable;
    bool sending;
    int64_t sendingStartTime;
  };
  ChannelInfo channelInfo[2] = {};

  //unsigned int versionNumber;
  uint32_t swVersion;
  uint16_t hardwareVersion;
  uint8_t faultID;
  uint8_t moduleID;
  uint8_t problemID;
  uint8_t suppID;

  bool abcDetected;

  TaskHandle_t sendTaskHandle;
  TickType_t xLastWakeTime;
  TickType_t xFrequency;
  const char* TAG = "ABC150CANHandler";


  void setControlMode(Channel channel, ControlMode controlMode);

public:
  ABC150CANHandler(AmpleCAN &_can);
  virtual ~ABC150CANHandler();
  void msgReceived(CAN_frame_t &msg);

  /* Set send task frequency */
  void setFrequency(int timeDelta);
  void setDefaultFrequency();

  /* Send task */
  static void sendTaskWrapper(void *arg);
  void sendTask();

  bool channelCheck(Channel channel);
  /* Used to change control of channel to remote */
  bool setLowerVoltageLimit(Channel channel, float voltage);
  bool setLowerCurrentLimit(Channel channel, float current);
  bool setLowerPowerLimit(Channel channel, float power);
  bool setUpperVoltageLimit(Channel channel, float voltage);
  bool setUpperCurrentLimit(Channel channel, float current);
  bool setUpperPowerLimit(Channel channel, float power);
  bool setVoltage(Channel channel, float voltage);
  bool setCurrent(Channel channel, float current);
  bool setPower(Channel channel, float power);
  bool setLoadMode(Channel channel, LoadMode loadMode);
  std::string getPacketProblemString(uint8_t problem, uint8_t supp);
  std::string getControlModeString(ControlMode controlMode);
  std::string getLoadModeString(LoadMode loadMode);
  void printInfo();

  void handleData(Channel channel, CAN_frame_t &msg);
  void handleLowerLimits(Channel channel, CAN_frame_t &msg);
  void handleUpperLimits(Channel channel, CAN_frame_t &msg);
  void handleStatus(Channel channel, CAN_frame_t &msg);
  void handleStationID(Channel channel, CAN_frame_t &msg);
  void handleGreeting(CAN_frame_t &msg);
  void handleFaultData(CAN_frame_t &msg);
  void handlePacketProblem(CAN_frame_t &msg);
  void handleRequestPC(CAN_frame_t &msg);

  void sendPCGreeting();
  void sendCommandPackage(Channel channel);
  void sendLowerLimits(Channel channel);
  void sendUpperLimits(Channel channel);
  void sendChangeControl(Channel channel);
  void sendRequestABCPackage();
  void sendPackage(Channel channel);

  void takeControl(Channel channel);
  void releaseControl(Channel channel);
  bool enable(Channel channel);
  bool disable(Channel channel);
  bool isDetected();

  float getVoltage(Channel channel);
  float getCurrent(Channel channel);
  uint32_t getTimeStamp(Channel channel);
  ControlMode getControlModeOut(Channel ch);
  LoadMode getLoadModeOut(Channel ch);
  ConverterStatus getConverterStatus(Channel ch);
  float getCommand(Channel ch);

};


class ABC150CANUserInterface {

private:
  static void help();

public:
  static bool getInput(const char *printString, AmpleSerial &pc, ABC150CANHandler::Channel *channel, float *value);
  static void ABC150CANInterface(AmpleSerial &pc, ABC150CANHandler *canHandler);

};



#endif /* _ABC150CANHANDLER_HPP_ */
