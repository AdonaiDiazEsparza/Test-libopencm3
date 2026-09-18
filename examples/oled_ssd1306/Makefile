PROJECT = oled_project
BUILD_DIR = bin

SHARED_DIR = 
CFILES = main.c
CFILES += minimal_oled.c
# AFILES += api-asm.S

# TODO - you will need to edit these two lines!
DEVICE=stm32f411ceu6
OOCD_FILE = board/stm32f411ceu6.cfg

# Need this flag for the resolution display
CFLAGS += -DCONFIG_RESOLUTION_128X64

# You shouldn't have to edit anything below here.
VPATH += $(SHARED_DIR)
INCLUDES += $(patsubst %,-I%, . $(SHARED_DIR))
OPENCM3_DIR=../../libopencm3

include $(OPENCM3_DIR)/mk/genlink-config.mk
include ../../rules.mk
include $(OPENCM3_DIR)/mk/genlink-rules.mk
