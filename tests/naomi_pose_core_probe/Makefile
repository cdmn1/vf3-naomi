ifeq ($(KOS_SUBARCH),)
$(error KOS_SUBARCH is unset. Source the Naomi KOS environ.sh first)
endif
ifneq ($(KOS_SUBARCH),naomi)
$(error Wrong KOS target '$(KOS_SUBARCH)'. This target requires KOS_SUBARCH=naomi)
endif
ifeq ($(KOS_BASE),)
$(error KOS_BASE is unset)
endif
ifeq ($(VF3_DECOMP_DIR),)
$(error VF3_DECOMP_DIR must point to the pinned vf3-decomp checkout)
endif

BUILD_VERSION ?= dev
TARGET = vf3_naomi_pose_core_probe.elf
ROM = vf3_naomi_pose_core_probe-$(BUILD_VERSION).bin
NAOMIBINTOOL ?= $(KOS_BASE)/utils/naomibintool/naomibintool
NATIVE = $(VF3_DECOMP_DIR)/src/native
OBJS = main.o matrix.o aim_math.o motion_aim.o pose_orchestration.o seven_pass.o halfword_frame.o facing_update.o aim_midpoints.o matrix_basis.o

CPPFLAGS += -I$(NATIVE)

main.o: main.c
	kos-cc $(CFLAGS) $(CPPFLAGS) -c $< -o $@

matrix.o: $(NATIVE)/matrix.c
	kos-cc $(CFLAGS) $(CPPFLAGS) -c $< -o $@
aim_math.o: $(NATIVE)/aim_math.c
	kos-cc $(CFLAGS) $(CPPFLAGS) -c $< -o $@
motion_aim.o: $(NATIVE)/motion_aim.c
	kos-cc $(CFLAGS) $(CPPFLAGS) -c $< -o $@
pose_orchestration.o: $(NATIVE)/pose_orchestration.c
	kos-cc $(CFLAGS) $(CPPFLAGS) -c $< -o $@
seven_pass.o: $(NATIVE)/seven_pass.c
	kos-cc $(CFLAGS) $(CPPFLAGS) -c $< -o $@
halfword_frame.o: $(NATIVE)/halfword_frame.c
	kos-cc $(CFLAGS) $(CPPFLAGS) -c $< -o $@
facing_update.o: $(NATIVE)/facing_update.c
	kos-cc $(CFLAGS) $(CPPFLAGS) -c $< -o $@
aim_midpoints.o: $(NATIVE)/aim_midpoints.c
	kos-cc $(CFLAGS) $(CPPFLAGS) -c $< -o $@
matrix_basis.o: $(NATIVE)/matrix_basis.c
	kos-cc $(CFLAGS) $(CPPFLAGS) -c $< -o $@

all: $(TARGET)
$(TARGET): $(OBJS)
	kos-cc -o $@ $(OBJS) -lm
$(ROM): $(TARGET) $(NAOMIBINTOOL)
	$(NAOMIBINTOOL) build $(ROM) -d "CDMN" -t "VF3 Pose Core $(BUILD_VERSION)" -e $(TARGET)
rom: $(ROM)
$(NAOMIBINTOOL):
	$(MAKE) -C $(KOS_BASE)/utils/naomibintool CC=cc
include $(KOS_BASE)/Makefile.rules
clean:
	-rm -f $(TARGET) $(ROM) $(OBJS)
.PHONY: all rom clean
