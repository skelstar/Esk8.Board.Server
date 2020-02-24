#define DEBUG_OUT Serial
#define PRINTSTREAM_FALLBACK
#include "Debug.hpp"

#include <Arduino.h>
#include <VescData.h>
#include <elapsedMillis.h>
#include <Smoothed.h>

#include <RF24Network.h>
#include <NRF24L01Lib.h>

#include <Fsm.h>

#define SPI_CE 33
#define SPI_CS 26

#define COMMS_BOARD 00
#define COMMS_CONTROLLER 01

//------------------------------------------------------------------

VescData board_packet;

ControllerData controller_packet;
ControllerConfig controller_config;

NRF24L01Lib nrf24;

RF24 radio(SPI_CE, SPI_CS);
RF24Network network(radio);

#define NUM_RETRIES 5

elapsedMillis since_last_controller_packet;
bool controller_connected = true;

Smoothed<int> smoothed_throttle;

class BoardState
{
public:
  bool vesc_data_ready;
} state;

#include <LedLightsLib.h>
LedLightsLib light;

//------------------------------------------------------------------
enum xEvent
{
  xEV_MOVING,
  xEV_STOPPED,
};

xQueueHandle xLightsEventQueue;

void send_to_event_queue(xEvent e)
{
  xQueueSendToFront(xLightsEventQueue, &e, pdMS_TO_TICKS(10));
}

//------------------------------------------------------------------

void send_to_vesc(uint8_t throttle, bool cruise_control);

#include <controller_comms.h>
#include <vesc_comms_2.h>
#include <peripherals.h>
#include <utils.h>

#include <core0.h>

//-------------------------------------------------------
void setup()
{
  Serial.begin(115200);

  nrf24.begin(&radio, &network, COMMS_BOARD, packet_available_cb);
  vesc.init(VESC_UART_BAUDRATE);

  button_init();

  xTaskCreatePinnedToCore(lightTask_0, "lightTask_0", 10000, NULL, /*priority*/ 3, NULL, 0);

  xLightsEventQueue = xQueueCreate(1, sizeof(xEvent));
}

elapsedMillis since_sent_to_board, since_smoothed_report;

void loop()
{
  nrf24.update();

  button0.loop();

  vesc_update();

  if (controller_timed_out() && controller_connected)
  {
    controller_connected = false;
    DEBUG("controller_timed_out!!!");
  }

  vTaskDelay(10);
}