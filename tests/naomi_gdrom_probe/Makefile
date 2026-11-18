ifeq ($(KOS_SUBARCH),)
$(error KOS_SUBARCH is unset. Source the Naomi KOS environ.sh first)
endif
ifneq ($(KOS_SUBARCH),naomi)
$(error Wrong KOS target '$(KOS_SUBARCH)'. This target requires KOS_SUBARCH=naomi)
endif
ifeq ($(KOS_BASE),)
$(error KOS_BASE is unset. Source the Naomi KOS environ.sh first)
endif

BUILD_VERSION ?= dev
TARGET = vf3_naomi_gdrom_probe.elf
ROM = vf3_naomi_gdrom_probe-$(BUILD_VERSION).bin
OBJS = main.o
NAOMIBINTOOL ?= $(KOS_BASE)/utils/naomibintool/naomibintool

all: $(TARGET)

rom: $(ROM)

$(NAOMIBINTOOL):
	$(MAKE) -C $(KOS_BASE)/utils/naomibintool CC=cc

$(TARGET): $(OBJS)
	kos-cc -o $(TARGET) $(OBJS)

$(ROM): $(TARGET) $(NAOMIBINTOOL)
	$(NAOMIBINTOOL) build $(ROM) -d "CDMN" -t "VF3 GDROM $(BUILD_VERSION)" -e $(TARGET)

include $(KOS_BASE)/Makefile.rules

clean:
	-rm -f $(TARGET) $(ROM) $(OBJS)

.PHONY: all rom clean
