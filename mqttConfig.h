/*
 * mqttConfig.h
 *
 *  Created on: 14 May 2020
 *      Author: worluk
 */

#ifndef MQTTCONFIG_H_
#define MQTTCONFIG_H_

/*---------------------------------------------------------------------------*/

#define MQTT_CLIENT_BROKER_IP_ADDR "fd00::1"
#define MQTT_CLIENT_ORG_ID "SolarGuard"
#define MQTT_CLIENT_AUTH_TOKEN "AUTHTOKEN"
#define MQTT_CLIENT_USERNAME "use-token-auth"

#define SOLARGUARD_CHANNEL_PREFIX "fhem/solar/"


/*---------------------------------------------------------------------------*/
#define CONFIG_ORG_ID_LEN        32
#define CONFIG_TYPE_ID_LEN       32
#define CONFIG_AUTH_TOKEN_LEN    32
#define CONFIG_EVENT_TYPE_ID_LEN 32
#define CONFIG_CMD_TYPE_LEN       8
#define CONFIG_IP_ADDR_STR_LEN   64

/*---------------------------------------------------------------------------*/
/* Maximum TCP segment size for outgoing segments of our socket */
#define MAX_TCP_SEGMENT_SIZE    32

/*---------------------------------------------------------------------------*/
/**
 * \brief Data structure declaration for the MQTT client configuration
 */
typedef struct mqtt_client_config {
  char org_id[CONFIG_ORG_ID_LEN];
  char type_id[CONFIG_TYPE_ID_LEN];
  char auth_token[CONFIG_AUTH_TOKEN_LEN];
  char event_type_id[CONFIG_EVENT_TYPE_ID_LEN];
  char broker_ip[CONFIG_IP_ADDR_STR_LEN];
  char cmd_type[CONFIG_CMD_TYPE_LEN];
  clock_time_t pub_interval;
  int def_rt_ping_interval;
  uint16_t broker_port;
} mqtt_client_config_t;



#endif /* MQTTCONFIG_H_ */
