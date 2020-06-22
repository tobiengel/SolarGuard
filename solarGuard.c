#include "solarGuard.h"
#include "os/sys/log.h"
#include "board.h"
#include "contiki.h"
#include "mqtt.h"
#include "mqttConfig.h"
#include "helper.h"

#include "net/routing/routing.h"

#include "net/ipv6/uip.h"
#include "net/ipv6/uip-icmp6.h"
#include "net/ipv6/sicslowpan.h"
#include "sys/etimer.h"
#include "sys/ctimer.h"
#include "dev/leds.h"

#include <uart0-arch.h>
#include <serial-line.h>
#include <string.h>

/*---------------------------------------------------------------------------*/

static const char *broker_ip = MQTT_CLIENT_BROKER_IP_ADDR;
static struct timer connection_life;
static uint8_t connect_attempt;
/*---------------------------------------------------------------------------*/
/* Various states */
static uint8_t state;

#define reduceLoad

/*---------------------------------------------------------------------------*/
PROCESS_NAME(solarGuard);
AUTOSTART_PROCESSES(&solarGuard);

/*---------------------------------------------------------------------------*/

static struct mqtt_connection conn;

/*---------------------------------------------------------------------------*/
static struct mqtt_message *msg_ptr = 0;
static struct etimer publish_periodic_timer;
static struct ctimer ct;
static struct ctimer ledt;
/*---------------------------------------------------------------------------*/
/* Parent RSSI functionality */
static struct uip_icmp6_echo_reply_notification echo_reply_notification;
static struct etimer echo_request_timer;
static struct etimer cyclic_send_timer;
static int def_rt_rssi = 0;

/*---------------------------------------------------------------------------*/
PROCESS(solarGuard, "Solar Guard");

/*---------------------------------------------------------------------------*/
static void echo_reply_handler(uip_ipaddr_t *source, uint8_t ttl, uint8_t *data, uint16_t datalen) {

  if(uip_ip6addr_cmp(source, uip_ds6_defrt_choose())) {
    def_rt_rssi = sicslowpan_get_last_rssi();
  }

}

void allLeds_off(void *d);

/*---------------------------------------------------------------------------*/
static void publish_led_off(void *d) {

  leds_off(LEDS_CONF_ALL);
  leds_on(LEDS_BLUE);
  ctimer_set(&ledt, PUBLISH_CONNECTEDLED_DURATION, allLeds_off, NULL);

}

void allLeds_off(void *d) {

    leds_off(LEDS_CONF_ALL);
    ctimer_set(&ledt, PUBLISH_CONNECTEDLED_DURATION*5, publish_led_off, NULL);
}



/*---------------------------------------------------------------------------*/
static void pub_handler(const char *topic, uint16_t topic_len, uint8_t *chunk, uint16_t chunk_len) {

    LOG_DBG("Pub Handler: topic='%s' (len=%u), chunk_len=%u\n", topic, topic_len, chunk_len);
    return;

}
/*---------------------------------------------------------------------------*/
static void mqtt_event(struct mqtt_connection *m, mqtt_event_t event, void *data) {

  switch(event) {
  case MQTT_EVENT_CONNECTED: {
    LOG_DBG("Application has a MQTT connection\n");
    timer_set(&connection_life, CONNECTION_STABLE_TIME);
    state = STATE_CONNECTED;
    break;
  }
  case MQTT_EVENT_DISCONNECTED: {
    LOG_DBG("MQTT Disconnect. Reason %u\n", *((mqtt_event_t *)data));

    state = STATE_DISCONNECTED;
    process_poll(&solarGuard);
    break;
  }
  case MQTT_EVENT_PUBLISH: {
    msg_ptr = data;

    /* Implement first_flag in publish message? */
    if(msg_ptr->first_chunk) {
      msg_ptr->first_chunk = 0;
      LOG_DBG("Application received publish for topic '%s'. Payload "
              "size is %i bytes.\n", msg_ptr->topic, msg_ptr->payload_length);
    }

    pub_handler(msg_ptr->topic, strlen(msg_ptr->topic),
                msg_ptr->payload_chunk, msg_ptr->payload_length);
    break;
  }
  default:
    LOG_DBG("Application got a unhandled MQTT event: %i\n", event);
    break;
  }
}


/*---------------------------------------------------------------------------*/
static void update_config(void)
{
  if(construct_client_id() == 0) {
    /* Fatal error. Client ID larger than the buffer */
    state = STATE_CONFIG_ERROR;
    return;
  }


  state = STATE_INIT;

  /*
   * Schedule next timer event ASAP
   *
   * If we entered an error state then we won't do anything when it fires.
   *
   * Since the error at this stage is a config error, we will only exit this
   * error state if we get a new config.
   */
  etimer_set(&publish_periodic_timer, 0);

  return;
}


/*---------------------------------------------------------------------------*/
static int init_config() {

  /* Populate configuration with default values */
  memset(&conf, 0, sizeof(mqtt_client_config_t));

  memcpy(conf.org_id, MQTT_CLIENT_ORG_ID, strlen(MQTT_CLIENT_ORG_ID));
  memcpy(conf.type_id, DEFAULT_TYPE_ID, strlen(DEFAULT_TYPE_ID));
  memcpy(conf.auth_token, MQTT_CLIENT_AUTH_TOKEN,
         strlen(MQTT_CLIENT_AUTH_TOKEN));
  memcpy(conf.event_type_id, DEFAULT_EVENT_TYPE_ID,
         strlen(DEFAULT_EVENT_TYPE_ID));
  memcpy(conf.broker_ip, broker_ip, strlen(broker_ip));
  memcpy(conf.cmd_type, DEFAULT_SUBSCRIBE_CMD_TYPE, 1);

  conf.broker_port = DEFAULT_BROKER_PORT;
  conf.pub_interval = DEFAULT_PUBLISH_INTERVAL;
  conf.def_rt_ping_interval = DEFAULT_RSSI_MEAS_INTERVAL;

  return 1;
}

/*---------------------------------------------------------------------------*/
static void publishStr(char* topic, char* msg) {
    //leds_on(LEDS_GREEN);
    //timer_set(&ledt, PUBLISH_CONNECTEDLED_DURATION, allLeds_off, NULL);
    LOG_DBG("Publish!\n");
    mqtt_publish(&conn, NULL, topic, (uint8_t *)msg, strlen(msg), MQTT_QOS_LEVEL_0, MQTT_RETAIN_OFF);

}

/*---------------------------------------------------------------------------*/
static void connect_to_broker(void) {

  /* Connect to MQTT server */
  mqtt_connect(&conn, conf.broker_ip, conf.broker_port, (conf.pub_interval * 3) / CLOCK_SECOND, MQTT_CLEAN_SESSION_ON);
  state = STATE_CONNECTING;
}

/*---------------------------------------------------------------------------*/
static void ping_parent(void) {

  if(uip_ds6_get_global(ADDR_PREFERRED) == NULL) return;

  uip_icmp6_send(uip_ds6_defrt_choose(), ICMP6_ECHO_REQUEST, 0, ECHO_REQ_PAYLOAD_LEN);
}

/*---------------------------------------------------------------------------*/
static void state_machine(void) {

  char def_rt_str[64];
  switch(state) {

//=========================
// STATE_INIT
//=========================
  case STATE_INIT:
    /* If we have just been configured register MQTT connection */
    mqtt_register(&conn, &solarGuard, client_id, mqtt_event, MAX_TCP_SEGMENT_SIZE);

    /* _register() will set auto_reconnect. We don't want that. */
    conn.auto_reconnect = 1;
    connect_attempt = 1;

    state = STATE_REGISTERED;
    LOG_DBG("Init\n");
    /* Continue */

//=========================
// STATE_REGISTERED
//=========================
  case STATE_REGISTERED:

      memset(def_rt_str, 0, sizeof(def_rt_str));

      for(uip_ds6_addr_t * locaddr = uip_ds6_if.addr_list; locaddr < uip_ds6_if.addr_list + UIP_DS6_ADDR_NB; locaddr++) {
          ipaddr_sprintf(def_rt_str, sizeof(def_rt_str), &(locaddr->ipaddr));
          LOG_DBG("IP %s\n", def_rt_str);
      }
      LOG_DBG("----\n");

    if(uip_ds6_get_global(ADDR_PREFERRED) != NULL) {
      /* Registered and with a public IP. Connect */
      LOG_DBG("Registered. Connect attempt %u\n", connect_attempt);
      ping_parent();
      connect_to_broker();
    } else {
      leds_on(MQTT_CLIENT_STATUS_LED);
      ctimer_set(&ct, NO_NET_LED_DURATION, publish_led_off, NULL);
    }
    etimer_set(&publish_periodic_timer, NET_CONNECT_PERIODIC);
    return;
    break;

//=========================
// STATE_CONNECTING
//=========================
  case STATE_CONNECTING:

    leds_on(MQTT_CLIENT_STATUS_LED);
    ctimer_set(&ct, CONNECTING_LED_DURATION, publish_led_off, NULL);
    /* Not connected yet. Wait */
    LOG_DBG("Connecting (%u)\n", connect_attempt);
    break;

//====================================
// STATE_CONNECTED && STATE_PUBLISHING
//====================================
  case STATE_CONNECTED:
  case STATE_PUBLISHING:
    /* If the timer expired, the connection is stable. */
    if(timer_expired(&connection_life)) {
      /*
       * Intentionally using 0 here instead of 1: We want RECONNECT_ATTEMPTS
       * attempts if we disconnect after a successful connect
       */
      connect_attempt = 0;
    }

    if(mqtt_ready(&conn) && conn.out_buffer_sent) {
      /* Connected. Publish */
        //printf("Connected\r\n");
      if(state == STATE_CONNECTED) {
          leds_on(LEDS_BLUE); //
     //     printf("Callo subscribe\r\n");
        state = STATE_PUBLISHING;

      } else {
       // leds_off(LEDS_BLUE); //
        //leds_on(LEDS_GREEN);
        //ctimer_set(&ct, PUBLISH_LED_ON_DURATION, publish_led_off, NULL);
        LOG_DBG("Publishing\n");
        //publish();
      }
      etimer_set(&publish_periodic_timer, conf.pub_interval);
      /* Return here so we don't end up rescheduling the timer */
      return;
    } else {
      /*
       * Our publish timer fired, but some MQTT packet is already in flight
       * (either not sent at all, or sent but not fully ACKd).
       *
       * This can mean that we have lost connectivity to our broker or that
       * simply there is some network delay. In both cases, we refuse to
       * trigger a new message and we wait for TCP to either ACK the entire
       * packet after retries, or to timeout and notify us.
       */
      LOG_DBG("Publishing... (MQTT state=%d, q=%u)\n", conn.state,
              conn.out_queue_full);
    }
    break;

//=========================
// STATE_DISCONNECTED
//=========================
  case STATE_DISCONNECTED:
    LOG_DBG("Disconnected\n");
    leds_off(LEDS_BLUE); //
    if(connect_attempt < RECONNECT_ATTEMPTS ||
       RECONNECT_ATTEMPTS == RETRY_FOREVER) {
      /* Disconnect and backoff */
      clock_time_t interval;
      mqtt_disconnect(&conn);
      connect_attempt++;

      interval = connect_attempt < 3 ? RECONNECT_INTERVAL << connect_attempt :
        RECONNECT_INTERVAL << 3;

      LOG_DBG("Disconnected. Attempt %u in %lu ticks\n", connect_attempt, interval);

      etimer_set(&publish_periodic_timer, interval);

      state = STATE_REGISTERED;
      return;
    } else {
      /* Max reconnect attempts reached. Enter error state */
      state = STATE_ERROR;
      LOG_DBG("Aborting connection after %u attempts\n", connect_attempt - 1);
    }
    break;

//=========================
// STATE_CONFIG_ERROR
//=========================
  case STATE_CONFIG_ERROR:
      leds_off(LEDS_BLUE); //
    /* Idle away. The only way out is a new config */
    LOG_ERR("Bad configuration.\n");
    return;

//=========================
// STATE_ERROR
//=========================
  case STATE_ERROR:
  default:
      leds_on(MQTT_CLIENT_STATUS_LED); //LEDS_RED
    /*
     * 'default' should never happen.
     *
     * If we enter here it's because of some error. Stop timers. The only thing
     * that can bring us out is a new config event
     */
    LOG_ERR("Default case: State=0x%02x\n", state);
    return;
  }

  /* If we didn't return so far, reschedule ourselves */
  etimer_set(&publish_periodic_timer, STATE_MACHINE_PERIODIC);
}


/*---------------------------------------------------------------------------*/
int handleInput(char* d){

    if(strlen(d) < 4)
        return 0;

    char topic[25] = SOLARGUARD_CHANNEL_PREFIX;

    int pos = (int)(strchr(d, '\t') - d);
    char* payload = d+1+pos;
    char* cmd = d;
    cmd[pos] = '\0';

    strncat(topic, cmd, strlen(cmd));
    LOG_DBG("%s %s\n", topic, payload);
#ifndef reduceLoad
    publishStr(topic, payload);
#else

    topicID tid = getTopicID(cmd);
    if(tid == topic_SER) return 1; //I dont care about the serial
    int val = getTopicValue(tid, payload);
    volatile int oldval = loaderValues[tid];

    LOG_DBG("%d : %d\n", val, oldval);

    if(tid < topicCount - 1 && val != oldval) {
        loaderValues[tid] = val;
        publishStr(topic, payload);
    }

    if(tid == topic_LOAD && strcmp(loadStatus, payload) != 0){
        memcpy(loadStatus, payload, strlen(payload));
        publishStr(topic, payload);
    }
#endif
    return 1;
}


/*---------------------------------------------------------------------------*/
PROCESS_THREAD(solarGuard, ev, data)
{

  PROCESS_BEGIN();

  if(init_config() != 1) {
    PROCESS_EXIT();
  }

  serial_line_init();
  update_config();

  def_rt_rssi = 0x8000000;

  uip_icmp6_echo_reply_callback_add(&echo_reply_notification, echo_reply_handler);

  etimer_set(&echo_request_timer, conf.def_rt_ping_interval);
  uart0_set_callback(serial_line_input_byte);
  /* Main loop */
  while(1) {

    PROCESS_YIELD();

    if(ev == serial_line_event_message && (state == STATE_CONNECTED || state == STATE_PUBLISHING)) {
        handleInput((char*)data);
    }

    if((ev == PROCESS_EVENT_TIMER && data == &publish_periodic_timer) || ev == PROCESS_EVENT_POLL) {
        state_machine();
    }

    if(ev == PROCESS_EVENT_TIMER && data == &echo_request_timer) {
        ping_parent();
        etimer_set(&cyclic_send_timer, conf.def_rt_ping_interval);
    }

  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
