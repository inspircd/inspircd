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
	@echo "Making modules..."
	${MAKE} -C src/modules DIRNAME="src/modules" $(MAKEARGS)
	@echo "*************************************"
	@echo "*        BUILD COMPLETE!            *"
	@echo "*   To install InspIRCd, type:      *"
	@echo "*         make install              *"
	@echo "*************************************"

ircd:
	@echo "Making ircd..."
	${MAKE} -C src DIRNAME="src" $(MAKEARGS)

install: all
	-install -m 0700 src/libIRCDaes.so $(LIBPATH)
	-install -m 0700 src/libIRCDasyncdns.so $(LIBPATH)
	-install -m 0700 src/libIRCDbase.so $(LIBPATH)
	-install -m 0700 src/libIRCDchannels.so $(LIBPATH)
	-install -m 0700 src/libIRCDcommand_parse.so $(LIBPATH)
	-install -m 0700 src/libIRCDcommands.so $(LIBPATH)
	-install -m 0700 src/libIRCDconfigreader.so $(LIBPATH)
	-install -m 0700 src/libIRCDcull_list.so $(LIBPATH)
	-install -m 0700 src/libIRCDdynamic.so $(LIBPATH)
	-install -m 0700 src/libIRCDhash.so $(LIBPATH)
	-install -m 0700 src/libIRCDhelper.so $(LIBPATH)
	-install -m 0700 src/libIRCDinspsocket.so $(LIBPATH)
	-install -m 0700 src/libIRCDmode.so $(LIBPATH)
	-install -m 0700 src/libIRCDmodules.so $(LIBPATH)
	-install -m 0700 src/libIRCDsocket.so $(LIBPATH)
	-install -m 0700 src/libIRCDsocketengine.so $(LIBPATH)
	-install -m 0700 src/libIRCDstring.so $(LIBPATH)
	-install -m 0700 src/libIRCDuserprocess.so $(LIBPATH)
	-install -m 0700 src/libIRCDusers.so $(LIBPATH)
	-install -m 0700 src/libIRCDwildcard.so $(LIBPATH)
	-install -m 0700 src/libIRCDxline.so $(LIBPATH)
	-install -m 0700 src/libIRCDtimer.so $(LIBPATH)
	-install -m 0700 src/libIRCDsnomasks.so $(LIBPATH)
	-install -m 0700 src/inspircd $(BINPATH)
	-install -m 0700 inspircd $(BASE) 2>/dev/null
	@-cp .gdbargs $(BASE)
	@-cp conf/inspircd.quotes.example $(CONPATH)
	@-cp conf/inspircd.rules.example $(CONPATH)
	@-cp conf/key.pem $(CONPATH)
	@-cp conf/cert.pem $(CONPATH)
	@-cp docs/inspircd.conf.example $(CONPATH)
	${MAKE} -C src/modules DIRNAME="src/modules" $(MAKEARGS) modinst
	@echo "*************************************"
	@echo "*        INSTALL COMPLETE!          *"
	@echo "* It is safe to ignore any messages *"
	@echo "* related to copying of files.      *"
	@echo "*                                   *"
	@echo "* REMEMBER TO EDIT YOUR CONFIG FILE *"
	@echo "*************************************"

clean:
	@echo Cleaning...
	@rm -rvf src/*.so src/*.o src/inspircd src/modules/*.so src/modules/*.o *~ src/*~ src/modules/*~ src/modules/extra/*~ src/modes/*~ src/modes/*.o src/modes/*.a
	@echo Completed.

modclean:
	rm -rf modules/*.so
	rm -rf src/modules/*.so
	rm -rf src/modules/*.o

squeakyclean: modclean clean
