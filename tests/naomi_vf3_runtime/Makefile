ifeq ($(KOS_SUBARCH),)
$(error KOS_SUBARCH is unset. Source the Naomi KOS environ.sh first)
endif
ifneq ($(KOS_SUBARCH),naomi)
$(error Wrong KOS target '$(KOS_SUBARCH)'. This target requires KOS_SUBARCH=naomi)
endif
ifeq ($(KOS_BASE),)
$(error KOS_BASE is unset. Source the Naomi KOS environ.sh first)
endif
ifeq ($(VF3_DECOMP_DIR),)
$(error VF3_DECOMP_DIR must point to the pinned vf3-decomp checkout)
endif

BUILD_VERSION ?= dev
TARGET = vf3_naomi_mt_asset_probe.elf
ROM = vf3_naomi_mt_asset_probe-$(BUILD_VERSION).bin
OBJS = main.o mt_asset.o mt.o motion.o matrix.o
NAOMIBINTOOL ?= $(KOS_BASE)/utils/naomibintool/naomibintool

CPPFLAGS += -I$(VF3_DECOMP_DIR)/src/media -I$(VF3_DECOMP_DIR)/src/native

all: $(TARGET)

mt_asset.o: mt_asset.c MTAKIPAI.inc
	kos-cc $(CFLAGS) -O0 -c $< -o $@

mt.o: $(VF3_DECOMP_DIR)/src/media/mt.c $(VF3_DECOMP_DIR)/src/media/mt.h
	kos-cc $(CFLAGS) $(CPPFLAGS) -c $< -o $@

motion.o: $(VF3_DECOMP_DIR)/src/native/motion.c $(VF3_DECOMP_DIR)/src/native/motion.h
	kos-cc $(CFLAGS) $(CPPFLAGS) -c $< -o $@

matrix.o: $(VF3_DECOMP_DIR)/src/native/matrix.c $(VF3_DECOMP_DIR)/src/native/matrix.h
	kos-cc $(CFLAGS) $(CPPFLAGS) -c $< -o $@

$(NAOMIBINTOOL):
	$(MAKE) -C $(KOS_BASE)/utils/naomibintool CC=cc

$(TARGET): $(OBJS)
	kos-cc -o $(TARGET) $(OBJS) -lm

$(ROM): $(TARGET) $(NAOMIBINTOOL)
	$(NAOMIBINTOOL) build $(ROM) -d "CDMN" -t "VF3 MT Eval $(BUILD_VERSION)" -e $(TARGET)

rom: $(ROM)

include $(KOS_BASE)/Makefile.rules

clean:
	-rm -f $(TARGET) $(ROM) $(OBJS)

.PHONY: all rom clean
