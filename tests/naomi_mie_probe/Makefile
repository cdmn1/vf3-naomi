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
TEST_TARGET = vf3_mie_test.elf
ROM = vf3_mie_probe.bin
TEST_ROM = vf3_mie_test.bin
OBJS = main.o vf3_input_map.o
TEST_OBJS = test_main.o
NAOMIBINTOOL ?= $(KOS_BASE)/utils/naomibintool/naomibintool

all: $(TARGET) $(TEST_TARGET)

rom: $(ROM)

$(NAOMIBINTOOL):
	$(MAKE) -C $(KOS_BASE)/utils/naomibintool CC=cc

$(TARGET): $(OBJS)
	kos-cc -o $(TARGET) $(OBJS)

$(TEST_TARGET): $(TEST_OBJS)
	kos-cc -o $(TEST_TARGET) $(TEST_OBJS)

$(ROM): $(TARGET) $(TEST_TARGET) $(NAOMIBINTOOL) combine_naomi_test_rom.py
	$(NAOMIBINTOOL) build $(ROM).main -d "CDMN" -t "VF3 MIE/JVS Probe" -e $(TARGET)
	$(NAOMIBINTOOL) build $(TEST_ROM) -d "CDMN" -t "VF3 MIE/JVS Test" -e $(TEST_TARGET)
	python3 combine_naomi_test_rom.py $(ROM).main $(TEST_ROM) $(ROM)
	rm -f $(ROM).main $(TEST_ROM)

include $(KOS_BASE)/Makefile.rules

clean:
	-rm -f $(TARGET) $(TEST_TARGET) $(ROM) $(TEST_ROM) $(ROM).main $(OBJS) $(TEST_OBJS)

.PHONY: all rom clean
