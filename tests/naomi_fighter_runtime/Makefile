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
TARGET = vf3_naomi_akira_static.elf
ROM = vf3_naomi_akira_static-$(BUILD_VERSION).bin
OBJS = main.o assets.o pol.o pol_mesh.o mt.o motion.o matrix.o motion_vectors.o motion_pose_kos.o

CPPFLAGS += -I$(VF3_DECOMP_DIR)/src/media -I$(VF3_DECOMP_DIR)/src/native

all: $(TARGET)

assets.o: assets.c MKAO_AKI_POL.inc ROB_AKI_POL.inc
	kos-cc $(CFLAGS) -c $< -o $@

pol.o: $(VF3_DECOMP_DIR)/src/media/pol.c $(VF3_DECOMP_DIR)/src/media/pol.h
	kos-cc $(CFLAGS) $(CPPFLAGS) -c $< -o $@

pol_mesh.o: $(VF3_DECOMP_DIR)/src/media/pol_mesh.c $(VF3_DECOMP_DIR)/src/media/pol_mesh.h
	kos-cc $(CFLAGS) $(CPPFLAGS) -c $< -o $@

mt.o: $(VF3_DECOMP_DIR)/src/media/mt.c $(VF3_DECOMP_DIR)/src/media/mt.h
	kos-cc $(CFLAGS) $(CPPFLAGS) -c $< -o $@

motion.o: $(VF3_DECOMP_DIR)/src/native/motion.c $(VF3_DECOMP_DIR)/src/native/motion.h
	kos-cc $(CFLAGS) $(CPPFLAGS) -c $< -o $@

matrix.o: $(VF3_DECOMP_DIR)/src/native/matrix.c $(VF3_DECOMP_DIR)/src/native/matrix.h
	kos-cc $(CFLAGS) $(CPPFLAGS) -c $< -o $@

motion_vectors.o: $(VF3_DECOMP_DIR)/src/native/motion_vectors.c $(VF3_DECOMP_DIR)/src/native/motion_vectors.h
	kos-cc $(CFLAGS) $(CPPFLAGS) -c $< -o $@

motion_pose_kos.o: motion_pose_kos.c motion_pose_kos.h
	kos-cc $(CFLAGS) $(CPPFLAGS) -c $< -o $@

$(TARGET): $(OBJS)
	kos-cc -o $(TARGET) $(OBJS) -lm

$(ROM): $(TARGET) $(NAOMIBINTOOL)
	$(NAOMIBINTOOL) build $(ROM) -d "CDMN" -t "VF3 Akira Static $(BUILD_VERSION)" -e $(TARGET)

rom: $(ROM)

$(NAOMIBINTOOL):
	$(MAKE) -C $(KOS_BASE)/utils/naomibintool CC=cc

include $(KOS_BASE)/Makefile.rules

clean:
	-rm -f $(TARGET) $(ROM) $(OBJS)

.PHONY: all rom clean
