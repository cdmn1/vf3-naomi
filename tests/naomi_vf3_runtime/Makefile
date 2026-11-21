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
TARGET = vf3_naomi_mt_asset_probe.elf
ROM = vf3_naomi_mt_asset_probe-$(BUILD_VERSION).bin
OBJS = main.o mt_asset.o
NAOMIBINTOOL ?= $(KOS_BASE)/utils/naomibintool/naomibintool

all: $(TARGET)

mt_asset.o: MTAKIPAI.BIN
	sh-elf-objcopy -I binary -O elf32-sh -B sh --rename-section .data=.rodata,alloc,load,readonly,data,contents --set-section-alignment .rodata=32 $< $@

$(NAOMIBINTOOL):
	$(MAKE) -C $(KOS_BASE)/utils/naomibintool CC=cc

$(TARGET): $(OBJS)
	kos-cc -o $(TARGET) $(OBJS)

$(ROM): $(TARGET) $(NAOMIBINTOOL)
	$(NAOMIBINTOOL) build $(ROM) -d "CDMN" -t "VF3 MT Asset $(BUILD_VERSION)" -e $(TARGET)

rom: $(ROM)

include $(KOS_BASE)/Makefile.rules

clean:
	-rm -f $(TARGET) $(ROM) $(OBJS)

.PHONY: all rom clean
