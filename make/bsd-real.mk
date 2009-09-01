CORE_TARGS != perl -e 'print join " ", grep s/\.cpp/.o/, <*.cpp>, <modes/*.cpp>'
CMD_TARGS != perl -e 'print join " ", grep s/\.cpp/.so/, <commands/*.cpp>'
MOD_TARGS != perl -e 'print join " ", grep s/\.cpp/.so/, <modules/*.cpp>'
MDIR_TARGS != perl -e 'print join " ", grep s!/?$$!.so!, grep -d, <modules/m_*>'

CORE_TARGS += socketengines/$(SOCKETENGINE).o threadengines/threadengine_pthread.o

DFILES != perl ../make/calcdep.pl -all

all: inspircd commands modules

commands: $(CMD_TARGS)

modules: $(MOD_TARGS) $(MDIR_TARGS)

inspircd: $(CORE_TARGS)
	$(RUNCC) -o $@ $(CORELDFLAGS) $(LDLIBS) $(CORE_TARGS)

.for FILE in $(DFILES)
.include "$(FILE)"
.endfor
