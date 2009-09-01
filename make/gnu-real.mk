CORE_TARGS = $(patsubst %.cpp,%.o,$(wildcard *.cpp) $(wildcard modes/*.cpp))
CMD_TARGS = $(patsubst %.cpp,%.so,$(wildcard commands/*.cpp))
MOD_TARGS = $(patsubst %.cpp,%.so,$(wildcard modules/*.cpp))

CORE_TARGS += socketengines/$(SOCKETENGINE).o threadengines/threadengine_pthread.o
MOD_TARGS += $(shell perl -e 'print join " ", grep s!/?$$!.so!, grep -d, <modules/m_*>')

DFILES = $(shell ../make/calcdep.pl -all)

all: inspircd commands modules

commands: $(CMD_TARGS)

modules: $(MOD_TARGS)

inspircd: $(CORE_TARGS)
	$(RUNCC) -o $@ $(CORELDFLAGS) $(LDLIBS) $(CORE_TARGS)

.%.d: %.cpp
	@../make/calcdep.pl -file $<

.%.d: %
	@../make/calcdep.pl -file $<

.PHONY: all alldep commands modules

-include $(DFILES)
