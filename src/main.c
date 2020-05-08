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

  }

enum mgos_app_init_result mgos_app_init(void) {
  struct gps2 *gps_dev = NULL;
  struct gps2_cfg cfg;
  

  // create a GPS config
  gps2_config_set_default(&cfg);


  // check these numbers!
  cfg.uart_baud_rate = 9600;
  cfg.uart_no = 2;
  cfg.handler = gps_handler;

  if (NULL == (gps_dev = gps2_create_uart(&cfg))) {
    LOG(LL_ERROR,("Did not connect to GPS"));
    return false;
  }


  // default flashing LED behaviour
#ifdef LED_PIN
  mgos_gpio_setup_output(LED_PIN, 0);
#endif
  mgos_set_timer(1000 /* ms */, MGOS_TIMER_REPEAT, timer_cb, NULL);
  return MGOS_APP_INIT_SUCCESS;
}
