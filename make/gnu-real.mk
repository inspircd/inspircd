CORE_TARGS = $(patsubst %.cpp,%.o,$(wildcard *.cpp))
MODE_TARGS = $(patsubst %.cpp,%.o,$(wildcard modes/*.cpp))
CMD_TARGS = $(patsubst %.cpp,%.so,$(wildcard commands/*.cpp))
MOD_TARGS = $(patsubst %.cpp,%.so,$(wildcard modules/*.cpp))

CORE_TARGS += threadengines/threadengine_pthread.o
CORE_TARGS += socketengines/$(SOCKETENGINE).o
CORE_TARGS += $(MODE_TARGS)
MOD_TARGS += $(shell perl -e 'print join " ", grep s!([^/]+)/$$!$$1.so!, <modules/m_*/>')

DFILES = $(shell perl -e 'print join " ", grep s!([^/]+)\.cpp!.$$1.d!, <*.cpp>, <commands/*.cpp>, <modes/*.cpp>, <modules/*.cpp>, <modules/m_*/*.cpp>')
DFILES += $(shell perl -e 'print join " ", grep s!([^/]+)/?$$!.$$1.d!, <modules/m_*/>')
DFILES += socketengines/.$(SOCKETENGINE).d threadengines/.threadengine_pthread.d

all: inspircd commands modules

commands: $(CMD_TARGS)

modules: $(MOD_TARGS)

inspircd: $(CORE_TARGS)
	$(RUNCC) -o $@ $(CORELDFLAGS) $(LDLIBS) $(CORE_TARGS)

.%.d: %.cpp
	@../make/calcdep.pl $<

.%.d: %
	@../make/calcdep.pl $<

.PHONY: all alldep commands modules

-include $(DFILES)
