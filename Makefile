CONTIKI_PROJECT = solarGuard
all: $(CONTIKI_PROJECT)

TARGET = simplelink
BOARD = sensortag/cc1352r
CFLAGS += -g

CONTIKI = ${CONTIKI_ROOT}

include $(CONTIKI)/Makefile.dir-variables
MODULES += $(CONTIKI_NG_APP_LAYER_DIR)/mqtt
-include $(CONTIKI)/Makefile.identify-target

MODULES_REL += arch/platform/$(TARGET)

PLATFORMS_ONLY = simplelink

include $(CONTIKI)/Makefile.include
