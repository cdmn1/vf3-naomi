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
OBJS = main.o gdrom_syscall_client.o resident_shim_blob.o syscalls.o
NAOMIBINTOOL ?= $(KOS_BASE)/utils/naomibintool/naomibintool
all: $(TARGET)
rom: $(ROM)
$(NAOMIBINTOOL):
	$(MAKE) -C $(KOS_BASE)/utils/naomibintool CC=cc
SH_ELF_BIN ?= $(KOS_CC_BASE)/bin
SH_ELF_LD ?= $(SH_ELF_BIN)/sh-elf-ld
SH_ELF_OBJCOPY ?= $(SH_ELF_BIN)/sh-elf-objcopy
SH_ELF_NM ?= $(SH_ELF_BIN)/sh-elf-nm

gdrom_syscall_resident.o: gdrom_syscall.c gdrom_syscall.h vf3_asset_map.h
	kos-cc -fno-jump-tables -c gdrom_syscall.c -o $@

chainload.o: chainload.S
	kos-cc -c chainload.S -o $@

resident_shim.elf: gdrom_syscall_resident.o chainload.o
	$(SH_ELF_LD) -EL -e _vf3_gd_install \
		--section-start=.vf3_resident_text=0x8D000000 \
		--section-start=.vf3_resident_rodata=0x8D010000 \
		--section-start=.vf3_resident_data=0x8D020000 \
		-o $@ gdrom_syscall_resident.o chainload.o

resident_shim.bin: resident_shim.elf
	$(SH_ELF_OBJCOPY) -O binary --gap-fill=0x00 \
		--only-section=.vf3_resident_text \
		--only-section=.vf3_resident_rodata \
		--only-section=.vf3_resident_data $< $@

resident_shim_blob.o: resident_shim.bin
	$(SH_ELF_OBJCOPY) -I binary -O elf32-shl -B sh $< $@

resident_offsets.h: resident_shim.elf resident_shim.bin
	python3 ../../tools/generate_resident_offsets.py --nm $(SH_ELF_NM) --elf resident_shim.elf --bin resident_shim.bin --output $@

gdrom_syscall_client.o: gdrom_syscall_client.c resident_offsets.h
	kos-cc -c gdrom_syscall_client.c -o $@

$(TARGET): $(OBJS)
	kos-cc -o $(TARGET) $(OBJS)
$(ROM): $(TARGET) $(NAOMIBINTOOL)
	$(NAOMIBINTOOL) build $(ROM) -d "CDMN" -t "VF3 GD Syscall $(BUILD_VERSION)" -e $(TARGET)
include $(KOS_BASE)/Makefile.rules
CFLAGS += -fno-jump-tables

clean:
	-rm -f $(TARGET) $(ROM) $(OBJS) gdrom_syscall_resident.o chainload.o resident_shim.elf resident_shim.bin resident_offsets.h
.PHONY: all rom clean
