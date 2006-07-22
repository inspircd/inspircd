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
DARWIN = @IS_DARWIN@
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
	@INSTALL_OUTPUT@
	@-install -v -m 0700 src/inspircd $(BINPATH)
	@-install -v -m 0700 inspircd $(BASE) 2>/dev/null
	@-cp conf/inspircd.quotes.example $(CONPATH)
	@-cp conf/inspircd.rules.example $(CONPATH)
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
	@rm -rvf src/*.so src/*.o src/inspircd src/modules/*.so src/modules/*.o *~ src/*~ src/modules/*~ src/modules/extra/*~
	@echo Completed.

modclean:
	rm -rf modules/*.so
	rm -rf src/modules/*.so
	rm -rf src/modules/*.o

squeakyclean: modclean clean
