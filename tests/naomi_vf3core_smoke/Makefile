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

TARGET = vf3_naomi_mt_parser.elf
ROM = vf3_naomi_mt_parser.bin
OBJS = main.o mt.o
NAOMIBINTOOL ?= $(KOS_BASE)/utils/naomibintool/naomibintool

CPPFLAGS += -I$(VF3_DECOMP_DIR)/src/media

all: $(TARGET)

rom: $(ROM)

mt.o: $(VF3_DECOMP_DIR)/src/media/mt.c $(VF3_DECOMP_DIR)/src/media/mt.h
	kos-cc $(CFLAGS) $(CPPFLAGS) -c $< -o $@

$(NAOMIBINTOOL):
	$(MAKE) -C $(KOS_BASE)/utils/naomibintool CC=cc

$(TARGET): $(OBJS)
	kos-cc -o $(TARGET) $(OBJS)

$(ROM): $(TARGET) $(NAOMIBINTOOL)
	$(NAOMIBINTOOL) build $(ROM) -d "CDMN" -t "VF3 MT Parser Smoke" -e $(TARGET)

include $(KOS_BASE)/Makefile.rules

clean:
	-rm -f $(TARGET) $(ROM) $(OBJS)

.PHONY: all rom clean
