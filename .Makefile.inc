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
	@-rm -f $(BINPATH)/*.so
	@-rm -f $(BINPATH)/inspircd
	@-cp src/*.so $(LIBPATH)
	@-cp src/inspircd $(BINPATH)
	@-cp inspircd $(BASE)
	@-chmod 0700 $(BINPATH)/inspircd
	@-chmod 0700 $(LIBPATH)/*.so
	@echo *************************************
	@echo *      BUILD COMPLETE!              *
	@echo * It is safe to ignore any messages *
	@echo * related to copying of files.      *
	@echo *************************************

config:
	@cp conf/inspire.motd $(CONPATH)/inspire.motd.example
	@cp conf/inspire.quotes $(CONPATH)/inspire.quotes.example
	@cp conf/inspire.rules $(CONPATH)/inspire.rules.example
	@cp docs/*.example $(CONPATH)
clean:
	@echo Cleaning...
	@rm -rvf src/*.so src/*.o src/inspircd src/modules/*.so src/modules/*.o *~ src/*~ src/modules/*~ src/modules/extra/*~
	@echo Completed.

modclean:
	rm -rf modules/*.so
	rm -rf src/modules/*.so
	rm -rf src/modules/*.o

squeakyclean: modclean clean
