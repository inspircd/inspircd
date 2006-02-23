# Inspircd Main Makefile
#
# (C) ChatSpike development team
# DO NOT MODIFY UNLESS YOU KNOW WHAT YOU ARE DOING!
#####
# Original Makefile & ./configure by <Craig@ChatSpike.net>
# Small modifications by <brain@ChatSpike.net>
#

CC = @CC@
MAKE = @MAKEPROG@
PROGS = inspircd
FLAGS = @FLAGS@
LDLIBS = @LDLIBS@
BASE = @BASE_DIR@
CONPATH = @CONFIG_DIR@
MODPATH = @MODULE_DIR@
BINPATH = @BINARY_DIR@
LIBPATH = @LIBRARY_DIR@
MODULES = @MODULES@
STATICLIBS = @STATICLIBS@
MAKEARGS = 'PROGS=${PROGS}' 'FLAGS=${FLAGS}' 'CC=${CC}' 'LDLIBS=${LDLIBS}' 'MODULES=${MODULES}' 'MODPATH=${MODPATH}' 'LIBPATH=${LIBPATH}' 'STATICLIBS=${STATICLIBS}'

all : @MAKEORDER@

mods:
	${MAKE} -C src/modules DIRNAME="src/modules" $(MAKEARGS)

ircd:
	${MAKE} -C src DIRNAME="src" $(MAKEARGS)

bininst:
	-install -v -m 0700 src/libIRCDaes.so $(LIBPATH)
	-install -v -m 0700 src/libIRCDasyncdns.so $(LIBPATH)
	-install -v -m 0700 src/libIRCDbase.so $(LIBPATH)
	-install -v -m 0700 src/libIRCDchannels.so $(LIBPATH)
	-install -v -m 0700 src/libIRCDcommand_parse.so $(LIBPATH)
	-install -v -m 0700 src/libIRCDcommands.so $(LIBPATH)
	-install -v -m 0700 src/libIRCDcull_list.so $(LIBPATH)
	-install -v -m 0700 src/libIRCDdnsqueue.so $(LIBPATH)
	-install -v -m 0700 src/libIRCDdynamic.so $(LIBPATH)
	-install -v -m 0700 src/libIRCDhash.so $(LIBPATH)
	-install -v -m 0700 src/libIRCDhelper.so $(LIBPATH)
	-install -v -m 0700 src/libIRCDio.so $(LIBPATH)
	-install -v -m 0700 src/libIRCDmessage.so $(LIBPATH)
	-install -v -m 0700 src/libIRCDmode.so $(LIBPATH)
	-install -v -m 0700 src/libIRCDmodules.so $(LIBPATH)
	-install -v -m 0700 src/libIRCDsocket.so $(LIBPATH)
	-install -v -m 0700 src/libIRCDsocketengine.so $(LIBPATH)
	-install -v -m 0700 src/libIRCDstring.so $(LIBPATH)
	-install -v -m 0700 src/libIRCDuserprocess.so $(LIBPATH)
	-install -v -m 0700 src/libIRCDusers.so $(LIBPATH)
	-install -v -m 0700 src/libIRCDwildcard.so $(LIBPATH)
	-install -v -m 0700 src/libIRCDxline.so $(LIBPATH)
	-install -v -m 0700 src/libIRCDtimer.so $(LIBPATH)
	-install -v -m 0700 src/inspircd $(BINPATH)
	@-install -v -m 0700 inspircd $(BASE) 2>/dev/null
	@echo "*************************************"
	@echo "*      BUILD COMPLETE!              *"
	@echo "* It is safe to ignore any messages *"
	@echo "* related to copying of files.      *"
	@echo "*************************************"

config:
	@-cp conf/inspircd.quotes.example $(CONPATH)
	@-cp conf/inspircd.rules.example $(CONPATH)
	@-cp docs/inspircd.conf.example $(CONPATH)
clean:
	@echo Cleaning...
	@rm -rvf src/*.so src/*.o src/inspircd src/modules/*.so src/modules/*.o *~ src/*~ src/modules/*~ src/modules/extra/*~
	@echo Completed.

modclean:
	rm -rf modules/*.so
	rm -rf src/modules/*.so
	rm -rf src/modules/*.o

squeakyclean: modclean clean
