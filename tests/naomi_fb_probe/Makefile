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
TARGET = vf3_naomi_fb_probe.elf
ROM = vf3_naomi_fb_probe-$(BUILD_VERSION).bin
all: $(TARGET)
rom: $(ROM)
$(TARGET): main.c
	kos-cc -O2 -Wall -Wextra -o $@ $<
$(ROM): $(TARGET)
	$(KOS_BASE)/utils/naomibintool/naomibintool build $@ -d "CDMN" -t "VF3 FB $(BUILD_VERSION)" -e $(TARGET)
include $(KOS_BASE)/Makefile.rules
clean:
	-rm -f $(TARGET) $(ROM)
.PHONY: all rom clean
