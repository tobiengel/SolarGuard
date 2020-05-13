#ifndef SOLAR_GUARD_H_
#define SOLAR_GUARD_H_
/*---------------------------------------------------------------------------*/
#include <string.h>
/*---------------------------------------------------------------------------*/
typedef struct mqtt_client_extension_s {
  void (*init)(void);
  char *(*value)(void);
} mqtt_client_extension_t;
/*---------------------------------------------------------------------------*/
#define MQTT_CLIENT_EXTENSIONS(...) \
  const mqtt_client_extension_t *mqtt_client_extensions[] = {__VA_ARGS__}; \
  const uint8_t mqtt_client_extension_count = \
    (sizeof(mqtt_client_extensions) / sizeof(mqtt_client_extensions[0]));
/*---------------------------------------------------------------------------*/
#endif /* SOLAR_GUARD_H_ */
/*---------------------------------------------------------------------------*/
