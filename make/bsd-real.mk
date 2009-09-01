CORE_TARGS != perl -e 'print join " ", grep s/\.cpp/.o/, <*.cpp>'
MODE_TARGS != perl -e 'print join " ", grep s/\.cpp/.o/, <modes/*.cpp>'
CMD_TARGS != perl -e 'print join " ", grep s/\.cpp/.so/, <commands/*.cpp>'
MOD_TARGS != perl -e 'print join " ", grep s/\.cpp/.so/, <modules/*.cpp>'
CORE_TARGS += modeclasses.a threadengines/threadengine_pthread.o
CORE_TARGS += socketengines/$(SOCKETENGINE).o

DFILES != perl -e 'print join " ", grep s/\.cpp/.d/, <*.cpp>, <commands/*.cpp>, <modes/*.cpp>, <modules/*.cpp>'
DFILES += socketengines/$(SOCKETENGINE).d threadengines/threadengine_pthread.d

all: inspircd commands modules

commands: $(CMD_TARGS)

modules: $(MOD_TARGS)

modeclasses.a: $(MODE_TARGS)
	@../make/run-cc.pl ar crs modeclasses.a $(MODE_TARGS)

inspircd: $(CORE_TARGS)
	$(RUNCC) $(FLAGS) -rdynamic -L. -o inspircd $(LDLIBS) $(CORE_TARGS)

.for FILE in $(DFILES)
.include "$(FILE)"
.endfor
