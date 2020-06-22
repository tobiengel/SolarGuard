/*
 * helper.h
 *
 *  Created on: 14 May 2020
 *      Author: worluk
 */

#ifndef HELPER_H_
#define HELPER_H_

#include "contiki.h"        //types
#include "net/ipv6/uip.h"   //uip_ipaddr_t
#include "mqttConfig.h"    //mqtt_client_config_t

#define LOG_MODULE "solarGuard"


/*---------------------------------------------------------------------------*/
/* -- Buffers for Client ID -------------------------------------------------*/
#define BUFFER_SIZE 64
extern char client_id[BUFFER_SIZE];

extern mqtt_client_config_t conf;



int ipaddr_sprintf(char *buf, uint8_t buf_len, const uip_ipaddr_t *addr);
int construct_client_id(void);


typedef enum topicID_e  {
    topic_err = 0xFF,
    topic_V = 0,
    topic_I,
    topic_VPV,
    topic_PPV,
    topic_CS,
    topic_MPPT,
    topic_ERR,
    topic_IL,
    topic_H19,
    topic_H20,
    topic_H21,
    topic_H22,
    topic_H23,
    topic_HSDS,
    topic_LOAD,
    topic_SER,
    topicCount
} topicID;


topicID getTopicID(char*);
int getTopicValue(topicID, const char*);

extern int loaderValues[(int)topicCount];
extern char loadStatus[4];
#endif /* HELPER_H_ */
