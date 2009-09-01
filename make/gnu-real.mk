CORE_TARGS = $(patsubst %.cpp,%.o,$(wildcard *.cpp))
MODE_TARGS = $(patsubst %.cpp,%.o,$(wildcard modes/*.cpp))
CMD_TARGS = $(patsubst %.cpp,%.so,$(wildcard commands/*.cpp))
MOD_TARGS = $(patsubst %.cpp,%.so,$(wildcard modules/*.cpp))
SPANNINGTREE_TARGS = $(patsubst %.cpp,%.o,$(wildcard modules/m_spanningtree/*.cpp))

CORE_TARGS += modeclasses.a threadengines/threadengine_pthread.o
CORE_TARGS += socketengines/$(SOCKETENGINE).o
MOD_TARGS += modules/m_spanningtree.so

DFILES = $(shell perl -e 'print join " ", grep s!([^/]+)\.cpp!.$$1.d!, <*.cpp>, <commands/*.cpp>, <modes/*.cpp>, <modules/*.cpp>, <modules/m_spanningtree/*.cpp>')
DFILES += socketengines/.$(SOCKETENGINE).d threadengines/.threadengine_pthread.d

all: inspircd commands modules

commands: $(CMD_TARGS)

modules: $(MOD_TARGS)

modeclasses.a: $(MODE_TARGS)
	@../make/run-cc.pl ar crs modeclasses.a $(MODE_TARGS)

modules/m_spanningtree.so: $(SPANNINGTREE_TARGS)
	$(RUNCC) $(FLAGS) -shared -export-dynamic -o $@ $(SPANNINGTREE_TARGS)

inspircd: $(CORE_TARGS)
	$(RUNCC) $(FLAGS) $(CORE_FLAGS) -o inspircd $(LDLIBS) $(CORE_TARGS)

.%.d: %.cpp
	@$(VDEP_IN)
	@../make/calcdep.pl $<
	@$(VDEP_OUT)

.PHONY: all alldep commands modules

-include $(DFILES)
