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
TARGET = vf3_naomi_gdrom_syscall_probe.elf
ROM = vf3_naomi_gdrom_syscall_probe-$(BUILD_VERSION).bin
OBJS = main.o gdrom_syscall.o syscalls.o
NAOMIBINTOOL ?= $(KOS_BASE)/utils/naomibintool/naomibintool
all: $(TARGET)
rom: $(ROM)
$(NAOMIBINTOOL):
	$(MAKE) -C $(KOS_BASE)/utils/naomibintool CC=cc
RESIDENT_LDFLAGS = -Wl,--section-start=.vf3_resident_text=0x8D000000 \
	-Wl,--section-start=.vf3_resident_rodata=0x8D010000 \
	-Wl,--section-start=.vf3_resident_data=0x8D020000 \
	-Wl,--defsym=__vf3_resident_start=0x8D000000 \
	-Wl,--defsym=__vf3_resident_end=0x8D030000

$(TARGET): $(OBJS)
	kos-cc $(RESIDENT_LDFLAGS) -o $(TARGET) $(OBJS)
$(ROM): $(TARGET) $(NAOMIBINTOOL)
	$(NAOMIBINTOOL) build $(ROM) -d "CDMN" -t "VF3 GD Syscall $(BUILD_VERSION)" -e $(TARGET)
include $(KOS_BASE)/Makefile.rules
CFLAGS += -fno-jump-tables

clean:
	-rm -f $(TARGET) $(ROM) $(OBJS)
.PHONY: all rom clean
