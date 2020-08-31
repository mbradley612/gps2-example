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
#define PMKT 1


/**
 Different commands to set the update rate from once a second (1 Hz) to 10 times
 a second (10Hz) Note that these only control the rate at which the position is
 echoed, to actually speed up the position fix you must also send one of the
 position fix rate commands below too. */
#define PMTK_SET_NMEA_UPDATE_100_MILLIHERTZ                                    \
  "$PMTK220,10000*2F" ///< Once every 10 seconds, 100 millihertz.
#define PMTK_SET_NMEA_UPDATE_200_MILLIHERTZ                                    \
  "$PMTK220,5000*1B" ///< Once every 5 seconds, 200 millihertz.
#define PMTK_SET_NMEA_UPDATE_1HZ "$PMTK220,1000*1F" ///<  1 Hz
#define PMTK_SET_NMEA_UPDATE_2HZ "$PMTK220,500*2B"  ///<  2 Hz
#define PMTK_SET_NMEA_UPDATE_5HZ "$PMTK220,200*2C"  ///<  5 Hz
#define PMTK_SET_NMEA_UPDATE_10HZ "$PMTK220,100*2F" ///< 10 Hz
// Position fix update rate commands.
#define PMTK_API_SET_FIX_CTL_100_MILLIHERTZ                                    \
  "$PMTK300,10000,0,0,0,0*2C" ///< Once every 10 seconds, 100 millihertz.
#define PMTK_API_SET_FIX_CTL_200_MILLIHERTZ                                    \
  "$PMTK300,5000,0,0,0,0*18" ///< Once every 5 seconds, 200 millihertz.
#define PMTK_API_SET_FIX_CTL_1HZ "$PMTK300,1000,0,0,0,0*1C" ///< 1 Hz
#define PMTK_API_SET_FIX_CTL_5HZ "$PMTK300,200,0,0,0,0*2F"  ///< 5 Hz
// Can't fix position faster than 5 times a second!

#define PMTK_SET_BAUD_115200 "$PMTK251,115200*1F" ///< 115200 bps
#define PMTK_SET_BAUD_57600 "$PMTK251,57600*2C"   ///<  57600 bps
#define PMTK_SET_BAUD_9600 "$PMTK251,9600*17"     ///<   9600 bps


/*
* comment this out if you don't want the global GPS device
*/
#define GPS_GLOBAL_DEVICE

// have we sent the 10hz command
bool hasSent10hz = false;

// have we changed the baud rate
bool hasChangedBaudRate = false;



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

static void send_5hz_command(void) {
  LOG(LL_INFO,("Sending 5HZ fix command"));
  gps2_send_command(mg_mk_str(PMTK_API_SET_FIX_CTL_5HZ));

} 
static void send_57600_baud_command(void) {
  LOG(LL_INFO,("Sending 57600 baud command"));
  gps2_send_command(mg_mk_str(PMTK_SET_BAUD_57600));

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
      case GPS_EV_CONNECTED: {
          LOG(LL_INFO,("GPS connected"));
    
          if (!hasSent10hz) {


            LOG(LL_INFO,("Sending 5Hz command to ESP32"));
          
            send_5hz_command();
            hasSent10hz = true;
          }
        
      } break;
      case GPS_EV_TIMEDOUT: {
        LOG(LL_INFO,("GPS device timeout"));
        /* we need some logic to handle cycling through UART baude rates if we either don't
        get a connect at startup or we get a timeout */
      } break;


       
    }

  }

void proprietary_sentence_handler(struct mg_str line, struct gps2 *gps_dev) {
  LOG(LL_DEBUG,("In proprietary sentence handler"));

  struct mg_str line_buffer;
  struct mg_str line_buffer_nul;

  line_buffer = mg_mk_str_n(line.p,line.len);

  line_buffer_nul = mg_strdup_nul(line_buffer);

  LOG(LL_INFO,("Proprietary sentence is: %s",line_buffer_nul.p));
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

  /*
  * Check that the device got created.
  */
  if (gps2_get_global_device() ==NULL) {

    return false;
  }

  /*
  * Set our event handler. 
  */

  gps2_set_ev_handler(gps_handler,NULL);
  gps2_set_proprietary_sentence_parser(proprietary_sentence_handler);
  
  LOG(LL_INFO,("Sending change baud command to GPS"));
  
  send_57600_baud_command();

  LOG(LL_INFO,("Sending change baud to ESP32"));
  gps2_set_uart_baud(57600);
  hasChangedBaudRate = true;
  
  
  gps2_enable_disconnect_timer(1000);



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
  ucfg.tx_buf_size = 512; 
  ucfg.rx_buf_size = 128; 
  ucfg.baud_rate=9600;

  device = gps2_create_uart(UART_NO, &ucfg, gps_handler, NULL);

  if (device ==NULL) {

    
    return false;
  }

  gps2_set_device_proprietary_sentence_parser(device, proprietary_sentence_handler);
  gps2_enable_device_disconnect_timer(device, 1000);
  return true;
  

}  

enum mgos_app_init_result mgos_app_init(void) {

  int gps_init;

  #ifdef GPS_GLOBAL_DEVICE
  gps_init = init_global_gps_device();
  #else
  gps_init = init_gps_device();
  #endif



  if (gps_init == false) {
    return MGOS_INIT_APP_INIT_FAILED;
  }


    // default flashing LED behaviour
  #ifdef LED_PIN
    mgos_gpio_setup_output(LED_PIN, 0);
  #endif
    mgos_set_timer(1000 /* ms */, MGOS_TIMER_REPEAT, timer_cb, NULL);
    return MGOS_APP_INIT_SUCCESS;
}
