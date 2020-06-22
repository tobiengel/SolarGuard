#include "helper.h"
#include "os/sys/log.h"
#include <stdlib.h>

char client_id[BUFFER_SIZE];
mqtt_client_config_t conf;

int loaderValues[(int)topicCount];
char loadStatus[4];

int getTopicValue(topicID t, const char* val){

    if(t < topicCount - 1) {
        return atoi(val);
    }
    else {
        return -1;
    }

}

topicID getTopicID(char* chan){

    if(strcmp(chan, "V") == 0)              return topic_V;
    else if (strcmp(chan, "I")      == 0)   return topic_I;
    else if (strcmp(chan, "VPV")    == 0)   return topic_VPV;
    else if (strcmp(chan, "PPV")    == 0)   return topic_PPV;
    else if (strcmp(chan, "CS")     == 0)   return topic_CS;
    else if (strcmp(chan, "MPPT")   == 0)   return topic_MPPT;
    else if (strcmp(chan, "ERR")    == 0)   return topic_ERR;
    else if (strcmp(chan, "LOAD")   == 0)   return topic_LOAD;
    else if (strcmp(chan, "IL")     == 0)   return topic_IL;
    else if (strcmp(chan, "H19")    == 0)   return topic_H19;
    else if (strcmp(chan, "H20")    == 0)   return topic_H20;
    else if (strcmp(chan, "H21")    == 0)   return topic_H21;
    else if (strcmp(chan, "H22")    == 0)   return topic_H22;
    else if (strcmp(chan, "H23")    == 0)   return topic_H23;
    else if (strcmp(chan, "HSDS")   == 0)   return topic_HSDS;
    else if (strcmp(chan, "SER#")   == 0)   return topic_SER;
    else return topic_err;
}


/*---------------------------------------------------------------------------*/
int ipaddr_sprintf(char *buf, uint8_t buf_len, const uip_ipaddr_t *addr) {

  uint16_t a;
  uint8_t len = 0;
  int i, f;
  for(i = 0, f = 0; i < sizeof(uip_ipaddr_t); i += 2) {
    a = (addr->u8[i] << 8) + addr->u8[i + 1];
    if(a == 0 && f >= 0) {
      if(f++ == 0) {
        len += snprintf(&buf[len], buf_len - len, "::");
      }
    } else {
      if(f > 0) {
        f = -1;
      } else if(i > 0) {
        len += snprintf(&buf[len], buf_len - len, ":");
      }
      len += snprintf(&buf[len], buf_len - len, "%x", a);
    }
  }

  return len;
}


/*---------------------------------------------------------------------------*/
int construct_client_id(void) {

  int len = snprintf(client_id, BUFFER_SIZE, "d:%s:%s:%02x%02x%02x%02x%02x%02x",
                     conf.org_id, conf.type_id,
                     linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1],
                     linkaddr_node_addr.u8[2], linkaddr_node_addr.u8[5],
                     linkaddr_node_addr.u8[6], linkaddr_node_addr.u8[7]);


  if(len < 0 || len >= BUFFER_SIZE) {
    //LOG_ERR("Client ID: %d, Buffer %d\n", len, BUFFER_SIZE);
    return 0;
  }

  return 1;
}
