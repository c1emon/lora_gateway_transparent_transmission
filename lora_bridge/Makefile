APP_NAME = lora_bridge
### constant symbols
ARCH ?=
CROSS_COMPILE ?= arm-linux-gnueabihf-
CC := $(CROSS_COMPILE)gcc
AR := $(CROSS_COMPILE)ar

OBJDIR = obj
INCLUDES = $(wildcard inc/*.h)
OBJECTS = $(wildcard src/*.c)
INCOBJS = $(filter-out src/$(APP_NAME).o,$(OBJECTS:.c=.o))

CFLAGS=-O2 -Wall -Wextra -std=c99 -Iinc -I.

## set liblgw path
LGW_PATH = ../lora_gateway/libloragw
LGW_INC = $(LGW_PATH)/inc/loragw_hal.h $(LGW_PATH)/inc/loragw_aux.h $(LGW_PATH)/inc/config.h

LIBS := -lloragw -lrt -lm -lpthread

## make all
all : $(APP_NAME)

## clean
clean:
	rm -f obj/*.o
	rm -f $(APP_NAME)

### HAL library (do no force multiple library rebuild even with 'make -B')

$(LGW_PATH)/inc/config.h:
	@if test ! -f $@; then \
	$(MAKE) all -C $(LGW_PATH); \
	fi

$(LGW_PATH)/libloragw.a: $(LGW_INC)
	@if test ! -f $@; then \
	$(MAKE) all -C $(LGW_PATH); \
	fi

### Main program compilation and assembly

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(OBJDIR)/%.o: src/%.c $(INCLUDES) | $(OBJDIR)
	$(CC) -c $(CFLAGS) -I$(LGW_PATH)/inc $< -o $@

$(OBJDIR)/$(APP_NAME).o: src/$(APP_NAME).c $(LGW_INC) $(INCLUDES) | $(OBJDIR)
	$(CC) -c $(CFLAGS) -I$(LGW_PATH)/inc $< -o $@

$(APP_NAME): $(OBJDIR)/$(APP_NAME).o $(LGW_PATH)/libloragw.a $(OBJDIR)/parson.o $(OBJDIR)/parson_config.o
	$(CC) -L$(LGW_PATH) $< $(OBJDIR)/parson.o $(OBJDIR)/parson_config.o -o $@ $(LIBS)

### EOF


