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
RENDERER_TARGET = vf3_naomi_exception_renderer_probe.elf
RENDERER_ROM = vf3_naomi_exception_renderer_probe-$(BUILD_VERSION).bin
ENTRY_TARGET = vf3_naomi_exception_entry_probe.elf
ENTRY_ROM = vf3_naomi_exception_entry_probe-$(BUILD_VERSION).bin
STACK_TARGET = vf3_naomi_exception_stack_probe.elf
STACK_ROM = vf3_naomi_exception_stack_probe-$(BUILD_VERSION).bin
COMMON_OBJS = gdrom_syscall_client.o resident_shim_blob.o syscalls.o
OBJS = main.o $(COMMON_OBJS)
NAOMIBINTOOL ?= $(KOS_BASE)/utils/naomibintool/naomibintool
all: $(TARGET) $(RENDERER_TARGET) $(ENTRY_TARGET) $(STACK_TARGET)
rom: $(ROM) $(RENDERER_ROM) $(ENTRY_ROM) $(STACK_ROM)
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

exception_diag.o: exception_diag.c
	kos-cc -fno-jump-tables -c exception_diag.c -o $@

exception_entry.o: exception_entry.S
	kos-cc -c exception_entry.S -o $@

exception_stack_probe.o: exception_stack_probe.S
	kos-cc -c exception_stack_probe.S -o $@

resident_shim.elf: gdrom_syscall_resident.o chainload.o exception_diag.o exception_entry.o exception_stack_probe.o
	$(SH_ELF_LD) -EL -e _vf3_gd_install \
		--section-start=.vf3_resident_text=0x8D000000 \
		--section-start=.vf3_resident_rodata=0x8D010000 \
		--section-start=.vf3_resident_data=0x8D020000 \
		-o $@ gdrom_syscall_resident.o chainload.o exception_diag.o exception_entry.o exception_stack_probe.o

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

exception_entry_probe_main.o: exception_entry_probe_main.c
	kos-cc -c exception_entry_probe_main.c -o $@

$(ENTRY_TARGET): exception_entry_probe_main.o $(COMMON_OBJS)
	kos-cc -o $(ENTRY_TARGET) exception_entry_probe_main.o $(COMMON_OBJS)

exception_stack_probe_main.o: exception_stack_probe_main.c
	kos-cc -c exception_stack_probe_main.c -o $@

$(STACK_TARGET): exception_stack_probe_main.o $(COMMON_OBJS)
	kos-cc -o $(STACK_TARGET) exception_stack_probe_main.o $(COMMON_OBJS)

renderer_probe_main.o: renderer_probe_main.c
	kos-cc -c renderer_probe_main.c -o $@

$(TARGET): $(OBJS)
	kos-cc -o $(TARGET) $(OBJS)
$(RENDERER_TARGET): renderer_probe_main.o $(COMMON_OBJS)
	kos-cc -o $(RENDERER_TARGET) renderer_probe_main.o $(COMMON_OBJS)
$(ROM): $(TARGET) $(NAOMIBINTOOL)
	$(NAOMIBINTOOL) build $(ROM) -d "CDMN" -t "VF3 GD $(BUILD_VERSION)" -e $(TARGET)
$(RENDERER_ROM): $(RENDERER_TARGET) $(NAOMIBINTOOL)
	$(NAOMIBINTOOL) build $(RENDERER_ROM) -d "CDMN" -t "VF3 RENDER $(BUILD_VERSION)" -e $(RENDERER_TARGET)
$(ENTRY_ROM): $(ENTRY_TARGET) $(NAOMIBINTOOL)
	$(NAOMIBINTOOL) build $(ENTRY_ROM) -d "CDMN" -t "VF3 ENTRY $(BUILD_VERSION)" -e $(ENTRY_TARGET)
$(STACK_ROM): $(STACK_TARGET) $(NAOMIBINTOOL)
	$(NAOMIBINTOOL) build $(STACK_ROM) -d "CDMN" -t "VF3 STACK $(BUILD_VERSION)" -e $(STACK_TARGET)
include $(KOS_BASE)/Makefile.rules
CFLAGS += -fno-jump-tables

clean:
	-rm -f $(TARGET) $(ROM) $(RENDERER_TARGET) $(RENDERER_ROM) $(ENTRY_TARGET) $(ENTRY_ROM) $(STACK_TARGET) $(STACK_ROM) exception_entry_probe_main.o exception_stack_probe_main.o renderer_probe_main.o $(OBJS) gdrom_syscall_resident.o chainload.o resident_shim.elf resident_shim.bin resident_offsets.h exception_diag.o exception_entry.o exception_stack_probe.o
.PHONY: all rom clean
