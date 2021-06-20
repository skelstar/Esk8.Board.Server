#pragma once

#include <TaskBase.h>
#include <QueueManager.h>
#include <tasks/queues/QueueFactory.h>
#include <VescData.h>
#include <vesc_comms.h>
#include <vesc_utils.h>

#define VESC_UART_BAUDRATE 115200

namespace nsVescCommsTask
{
  vesc_comms vesc;

  // prototypes
  bool get_vesc_values(VescData &vescData);
}

class VescCommsTask : public TaskBase
{
public:
  bool printWarnings = true,
       printReadFromVesc = false,
       printSentToVesc = false;

private:
  // BatteryInfo Prototype;

  Queue1::Manager<ControllerData> *controllerQueue = nullptr;
  Queue1::Manager<VescData> *vescDataQueue = nullptr;

  VescData vescData;

  bool mockMovingLoop = false;

  elapsedMillis sinceLastMock, sinceReadFromVesc;
  const ulong mockMovinginterval = 5000;

public:
  VescCommsTask() : TaskBase("VescCommsTask", 5000)
  {
    _core = CORE_0;
  }

private:
  void _initialise()
  {
    controllerQueue = createQueue<ControllerData>("(VescCommsTask) controllerQueue");
    controllerQueue->read(); // clear the queue

    vescDataQueue = createQueue<VescData>("(VescCommsTask) vescDataQueue");
    vescDataQueue->printMissedPacket = false;

    nsVescCommsTask::vesc.init(VESC_UART_BAUDRATE);
  }

#define IGNORE_X_AXIS 127
#define IGNORE_UPPER_BUTTON 0

  void doWork()
  {
    if (vescDataQueue->hasValue())
      // from MockTask maybe?
      vescData.moving = vescDataQueue->payload.moving;

    if (controllerQueue->hasValue())
      _handleControllerPacket(controllerQueue->payload);

    if (sinceReadFromVesc > GET_FROM_VESC_INTERVAL)
    {
      sinceReadFromVesc = 0;
      _readFromVesc();
    }
  } // doWork

  void cleanup()
  {
    delete (controllerQueue);
    delete (vescDataQueue);
  }

  void _readFromVesc()
  {
    if (SEND_TO_VESC == 0)
      return;

    // half the packet are successful (right length), speed/interval makes do difference
    bool success = nsVescCommsTask::get_vesc_values(vescData);
    if (success)
    {
      vescDataQueue->send(&vescData);
    }

    if (printReadFromVesc)
      vescData.printThis("[VescCommsTask]-->");
  }

  void _handleControllerPacket(ControllerData packet)
  {
    vescData.id = packet.id;
    vescData.txTime = packet.txTime;
    vescData.version = VERSION;

    if (SEND_TO_VESC == 1)
    {
      nsVescCommsTask::vesc.setNunchuckValues(
          IGNORE_X_AXIS,
          /*y*/ packet.throttle,
          packet.cruise_control,
          IGNORE_UPPER_BUTTON);

      if (printSentToVesc)
        Serial.printf("sending throttle=%d cruise=%d\n", packet.throttle, packet.cruise_control);

      vescDataQueue->send(&vescData);
    }
    else
    {
      if (mockMovingLoop && sinceLastMock > mockMovinginterval)
      {
        sinceLastMock = 0;
        vescData.moving = !vescData.moving;
      }
      // reply immediately
      vescDataQueue->send(&vescData);
    }
  }
};

VescCommsTask vescCommsTask;

namespace nsVescCommsTask
{
  void task1(void *parameters)
  {
    vescCommsTask.task(parameters);
  }
  //-----------------------------------------------------------------------
  bool get_vesc_values(VescData &vescData)
  {
    uint8_t vesc_packet[PACKET_MAX_LENGTH];

    int numBytes = vesc.fetch_packet(vesc_packet);

    if (numBytes == 0 || numBytes != 70)
      return false;

    if (vesc.get_voltage(vesc_packet) < 5.0)
      return false;

    int32_t rpm_raw = vesc.get_rpm(vesc_packet);

    vescData.batteryVoltage = vesc.get_voltage(vesc_packet);
    vescData.moving = rpm_raw > RPM_AT_MOVING;
    // Serial.printf("numBytes: %d rpm: %d, batt volts: %.1fV\n", numBytes, rpm_raw, vescData.batteryVoltage);
    // TODO initial_amphours etc
    vescData.ampHours = vesc.get_amphours_discharged(vesc_packet) - initial_ampHours;
    vescData.motorCurrent = vesc.get_motor_current(vesc_packet);
    vescData.odometer = get_distance_in_meters(vesc.get_tachometer(vesc_packet)) - initial_odometer;
    vescData.temp_mosfet = vesc.get_temp_mosfet(vesc_packet);

    return true;
  }
}
