# (C) ChatSpike development team
# Makefile by <Craig@ChatSpike.net>
# Small modifications by <brain@ChatSpike.net>
#

all: $(MODULES)

%.so:
	@$(MAKE) --no-print-directory TARGET=$(@:.so=)

$(TARGET).so:
	$(CC) -I../../include $(FLAGS) -shared -o $@ $(TARGET).cpp
	@cp $(TARGET).so $(MODPATH)$(TARGET).so
