TARGET = vf3_mie_probe.elf
OBJS = main.o

all: rm-elf $(TARGET)

include $(KOS_BASE)/Makefile.rules

clean: rm-elf
	-rm -f $(OBJS) $(TARGET).bin

rm-elf:
	-rm -f $(TARGET)

$(TARGET): $(OBJS)
	kos-cc -o $(TARGET) $(OBJS)

dist: $(TARGET)
	$(KOS_STRIP) $(TARGET)
	$(KOS_OBJCOPY) -R .stack -O binary $(TARGET) $(TARGET).bin
