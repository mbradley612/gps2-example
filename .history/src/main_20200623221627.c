/*
 * Copyright (c) 2014-2018 Cesanta Software Limited
 * All rights reserved
 *
 * Licensed under the Apache License, Version 2.0 (the ""License"");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an ""AS IS"" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "mgos.h"
#include "gps2.h"
#include <inttypes.h>

#define UART_NO 2


static void timer_cb(void *arg) {
  static bool s_tick_tock = false;
  LOG(LL_INFO,
      ("%s uptime: %.2lf, RAM: %lu, %lu free", (s_tick_tock ? "Tick" : "Tock"),
       mgos_uptime(), (unsigned long) mgos_get_heap_size(),
       (unsigned long) mgos_get_free_heap_size()));
  s_tick_tock = !s_tick_tock;
#ifdef LED_PIN
  mgos_gpio_toggle(LED_PIN);
#endif
  (void) arg;
}

static void gps_handler(struct gps2 *gps_dev,
  int event, void *event_data, void *user_data) {

    switch (event) {
      case GPS_EV_INITIALIZED: {
        LOG(LL_INFO,("GPS Initialized event received"));
      } break;
      case GPS_EV_LOCATION_UPDATE: {
        struct gps2_location location;
        struct gps2_datetime datetime;
        int64_t age;
        
        gps2_get_location(&location, &age);
        LOG(LL_INFO,("Lon: %f, Lat %f, Age %"PRId64 "", location.longitude, location.latitude, age));
        
        gps2_get_datetime(&datetime,&age);
        LOG(LL_INFO,("Time is: %02d:%02d:%02d",datetime.hours,datetime.minutes,datetime.seconds));

      } break;
      case GPS_EV_FIX_ACQUIRED: {
          LOG(LL_INFO,("GPS fix acquired"));
       
      } break;
      case GPS_EV_FIX_LOST: {
          LOG(LL_INFO,("GPS fix lost"));
       
      } break;
    }

  }

/*
* Initializing the global GPS device is all through config. The only mandatory settings are  the
* UART number and baud rate. For the others you can rely on the defaults:
*
* config_schema:
  - ["gps.uart.no", 2]
  - ["gps.uart.baud", 9600]
*/
int init_global_gps_device(void) {
  if (gps2_get_global_device() ==NULL) {

    
    return false;
  }

  gps2_set_device_ev_handler(device, gps_handler,NULL);

  return true;
  
}

/*
* To initialize a GPS that is not the global device, create and configure an
* mgos_uart_config
*/


int init_gps_device(void) {


  struct mgos_uart_config ucfg;
  struct gps2 *device;

  mgos_uart_config_set_defaults(UART_NO,&ucfg);
  

  ucfg.num_data_bits = 8;
  ucfg.parity = MGOS_UART_PARITY_NONE;
  ucfg.stop_bits = MGOS_UART_STOP_BITS_1;
  ucfg.tx_buf_size = 512; /*mgos_sys_config_get_gps_uart_tx_buffer_size();*/
  ucfg.rx_buf_size = 128; /*mgos_sys_config_get_gps_uart_rx_buffer_size();*/
  ucfg.baud_rate=9600;

  device = gps2_create_uart(UART_NO, &ucfg, gps_handler, NULL);

  if (device ==NULL) {

    
    return false;
  } else {
    return true;
  }

}  

enum mgos_app_init_result mgos_app_init(void) {

  int gps_init;

  #ifdef GPS_GLOBAL_DEVICE
  gps_init = init_global_gps_device();
  #else
  gps_init = init_gps_device()

  #endif


    // default flashing LED behaviour
  #ifdef LED_PIN
    mgos_gpio_setup_output(LED_PIN, 0);
  #endif
    mgos_set_timer(1000 /* ms */, MGOS_TIMER_REPEAT, timer_cb, NULL);
    return MGOS_APP_INIT_SUCCESS;
}
