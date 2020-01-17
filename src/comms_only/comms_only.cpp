#define DEBUG_OUT Serial
#define PRINTSTREAM_FALLBACK
#include "Debug.hpp"

#include <Button2.h>
#include <Fsm.h>
#include <VescData.h>
#include <elapsedMillis.h>
#include <Smoothed.h>

// prototypes
void controller_packet_available_cb(uint16_t from_id, uint8_t type);

VescData vescdata, initialVescData, board_packet;

ControllerData controller_packet, old_packet;

bool sent_first_packet = false;
bool vescOnline = false;

void button_init();
void button_loop();

#define SECONDS 1000

#ifdef USE_TEST_VALUES
#define CONTROLLER_TIMEOUT 1100
#define SEND_TO_VESC
#else
#define CONTROLLER_SEND_MS  200
#define CONTROLLER_TIMEOUT CONTROLLER_SEND_MS + 100
// #define SEND_TO_VESC
#endif

#ifdef USING_BUTTONS

#define BUTTON_0 0
Button2 button0(BUTTON_0);

#endif

#define NUM_PIXELS 21
#define PIXEL_PIN 4
#define BRIGHT_MAX 10

#include "vesc_utils.h"
// #include <LedLightsLib.h>

// LedLightsLib light;

// #include "state_machine.h"

//------------------------------------------------------------------

xQueueHandle xEventQueue;
xQueueHandle xControllerTaskQueue;
xQueueHandle xSendToVescQueue;

SemaphoreHandle_t xVescDataSemaphore;

//------------------------------------------------------------------

#include "peripherals.h"

//------------------------------------------------------------------

//------------------------------------------------------------------

void fsm_event_handler()
{
  // EventsEnum e;
  // bool event_ready = xQueueReceive(xEventQueue, &e, pdMS_TO_TICKS(0)) == pdPASS;

  // if (event_ready)
  // {
  //   TRIGGER(e);
  // }
}
//----------------------------------------------------------

Smoothed <float> retry_log;


bool send_to_packet_controller(ReasonType reason);

#include "nrf_fsm.h"

#include "core0.h"
#include "core1.h"

void setup()
{
  Serial.begin(115200);
  Serial.println("Start");

  nrf24.begin(&radio, &network, /*address*/ 0, controller_packet_available_cb);

  retry_log.begin(SMOOTHED_AVERAGE, 10000 / CONTROLLER_SEND_MS);

  add_nrf_fsm_transitions();

#ifdef USE_TEST_VALUES
  Serial.printf("\n");
  Serial.printf("/********************************************************/\n");
  Serial.printf("/*               WARNING: Using test values!            */\n");
  Serial.printf("/********************************************************/\n");
  Serial.printf("\n");
#endif

#ifdef FAKE_VESC_ONLINE
  Serial.printf("\n");
  Serial.printf("/********************************************************/\n");
  Serial.printf("/*               WARNING: FAKE VESC ONLINE !            */\n");
  Serial.printf("/********************************************************/\n");
  Serial.printf("\n");
#endif

  light_init();

  button_init();

  xTaskCreatePinnedToCore(vescTask_0, "vescTask_0", 10000, NULL, /*priority*/ 4, NULL, 0);
  xVescDataSemaphore = xSemaphoreCreateMutex();
  // xEventQueue = xQueueCreate(1, sizeof(EventsEnum));
  xSendToVescQueue = xQueueCreate(1, sizeof(uint8_t));

  controller_packet.throttle = 127;

  // fsm_add_transitions();
  // fsm.run_machine();

#ifdef USING_BUTTONS
  button_init();
  button_loop();
#endif
}
//----------------------------------------------------------
elapsedMillis since_sent_to_controller = 0;

void loop()
{
  nrf_fsm.run_machine();

#ifdef USING_BUTTONS
  button_loop();
#endif

  // fsm_event_handler();

  nrf24.update();
}
//----------------------------------------------------------
