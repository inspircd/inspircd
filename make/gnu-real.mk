CORE_TARGS = $(patsubst %.cpp,%.o,$(wildcard *.cpp))
MODE_TARGS = $(patsubst %.cpp,%.o,$(wildcard modes/*.cpp))
CMD_TARGS = $(patsubst %.cpp,%.so,$(wildcard commands/*.cpp))
MOD_TARGS = $(patsubst %.cpp,%.so,$(wildcard modules/*.cpp))

CORE_TARGS += modeclasses.a threadengines/threadengine_pthread.o
CORE_TARGS += socketengines/$(SOCKETENGINE).o

DFILES = $(patsubst %.cpp,%.d,$(wildcard *.cpp))
DFILES += $(patsubst %.cpp,%.d,$(wildcard commands/*.cpp))
DFILES += $(patsubst %.cpp,%.d,$(wildcard modes/*.cpp))
DFILES += $(patsubst %.cpp,%.d,$(wildcard modules/*.cpp))
DFILES += socketengines/$(SOCKETENGINE).d threadengines/threadengine_pthread.d

all: inspircd commands modules

commands: $(CMD_TARGS)

modules: $(MOD_TARGS)

modeclasses.a: $(MODE_TARGS)
	@../make/run-cc.pl ar crs modeclasses.a $(MODE_TARGS)

inspircd: $(CORE_TARGS)
	$(RUNCC) $(FLAGS) -rdynamic -L. -o inspircd $(LDLIBS) $(CORE_TARGS)

%.d: %.cpp
	@../make/calcdep.pl $<

.PHONY: all commands modules

-include $(DFILES)
