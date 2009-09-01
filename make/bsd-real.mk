CORE_TARGS != perl -e 'print join " ", grep s/\.cpp/.o/, <*.cpp>, <modes/*.cpp>'
CMD_TARGS != perl -e 'print join " ", grep s/\.cpp/.so/, <commands/*.cpp>'
MOD_TARGS != perl -e 'print join " ", grep s/\.cpp/.so/, <modules/*.cpp>'
MDIR_TARGS != perl -e 'print join " ", grep s!/?$$!.so!, <modules/m_*/>'

CORE_TARGS += socketengines/$(SOCKETENGINE).o threadengines/threadengine_pthread.o

DFILES != perl -e 'print join " ", grep s!([^/]+)\.cpp!.$$1.d!, <*.cpp>, <commands/*.cpp>, <modes/*.cpp>, <modules/*.cpp>, <modules/m_*/*.cpp>'
DFILES2 != perl -e 'print join " ", grep s!([^/]+)/?$$!.$$1.d!, <modules/m_*/>'
DFILES += $(DFILES2) socketengines/.$(SOCKETENGINE).d threadengines/.threadengine_pthread.d

all: inspircd commands modules

commands: $(CMD_TARGS)

modules: $(MOD_TARGS) $(MDIR_TARGS)

inspircd: $(CORE_TARGS)
	$(RUNCC) -o $@ $(CORELDFLAGS) $(LDLIBS) $(CORE_TARGS)

.for FILE in $(DFILES)
.include "$(FILE)"
.endfor
