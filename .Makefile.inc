# Inspircd Main Makefile
#
# (C) ChatSpike development team
# DO NOT MODIFY UNLESS YOU KNOW WHAT YOU ARE DOING!
#####
# Original Makefile & ./configure by <Craig@ChatSpike.net>
# Small modifications by <brain@ChatSpike.net>
#

CC = g++
MAKE = @MAKEPROG@
PROGS = inspircd
FLAGS = @FLAGS@
LDLIBS = @LDLIBS@
CONPATH = @CONFIG_DIR@
MODPATH = @MODULE_DIR@
BINPATH = @BINARY_DIR@
MODULES = @MODULES@
MAKEARGS = 'PROGS=${PROGS}' 'FLAGS=${FLAGS}' 'CC=${CC}' 'LDLIBS=${LDLIBS}' 'MODULES=${MODULES}' 'MODPATH=$(MODPATH)'

all : ircd mods config bininst

mods:
	${MAKE} -C src/modules DIRNAME="src/modules" $(MAKEARGS)

ircd:
	${MAKE} -C src DIRNAME="src" $(MAKEARGS)

bininst:
	@-cp src/inspircd $(BINPATH)

config:
	@cp conf/inspire.motd $(CONPATH)/inspire.motd.example
	@cp conf/inspire.quotes $(CONPATH)/inspire.quotes.example
	@cp conf/inspire.rules $(CONPATH)/inspire.rules.example
	@cp docs/*.example $(CONPATH)
clean:
	rm -rf src/*.o
	rm -rf src/modules/*.so

modclean:
	rm -rf modules/*.so
	rm -rf src/modules/*.so

squeakyclean: modclean clean
