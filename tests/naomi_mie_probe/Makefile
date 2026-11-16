ifeq ($(KOS_SUBARCH),)
$(error KOS_SUBARCH is unset. Source the Naomi KOS environ.sh first)
endif
ifneq ($(KOS_SUBARCH),naomi)
$(error Wrong KOS target '$(KOS_SUBARCH)'. This probe requires KOS_SUBARCH=naomi)
endif
ifeq ($(KOS_BASE),)
$(error KOS_BASE is unset. Source the Naomi KOS environ.sh first)
endif

TARGET = vf3_mie_probe.elf
ROM = vf3_mie_probe.bin
OBJS = main.o vf3_input_map.o
NAOMIBINTOOL ?= $(KOS_BASE)/utils/naomibintool/naomibintool

all: $(TARGET)

rom: $(ROM)

$(NAOMIBINTOOL):
	$(MAKE) -C $(KOS_BASE)/utils/naomibintool CC=cc

$(TARGET): $(OBJS)
	kos-cc -o $(TARGET) $(OBJS)

$(ROM): $(TARGET) $(NAOMIBINTOOL)
	$(NAOMIBINTOOL) build $@ -d "CDMN" -t "VF3 MIE/JVS Probe" -e $(TARGET)

include $(KOS_BASE)/Makefile.rules

clean:
	-rm -f $(TARGET) $(ROM) $(OBJS)

.PHONY: all rom clean
