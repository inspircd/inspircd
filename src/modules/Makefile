# (C) ChatSpike development team
# Makefile by <Craig@ChatSpike.net>
# Many Thanks to Andrew Church <achurch@achurch.org>
#    for assisting with making this work right.

all: $(MODULES)

%.so: %.cpp
	$(CC) -I../../include $(FLAGS) -shared -o $@ $<
	@cp $@ $(MODPATH)/
