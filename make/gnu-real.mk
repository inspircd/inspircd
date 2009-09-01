CORE_TARGS = $(patsubst %.cpp,%.o,$(wildcard *.cpp))
MODE_TARGS = $(patsubst %.cpp,%.o,$(wildcard modes/*.cpp))
CMD_TARGS = $(patsubst %.cpp,%.so,$(wildcard commands/*.cpp))
MOD_TARGS = $(patsubst %.cpp,%.so,$(wildcard modules/*.cpp))
SPANNINGTREE_TARGS = $(patsubst %.cpp,%.o,$(wildcard modules/m_spanningtree/*.cpp))

CORE_TARGS += threadengines/threadengine_pthread.o
CORE_TARGS += socketengines/$(SOCKETENGINE).o
CORE_TARGS += $(MODE_TARGS)
MOD_TARGS += modules/m_spanningtree.so

DFILES = $(shell perl -e 'print join " ", grep s!([^/]+)\.cpp!.$$1.d!, <*.cpp>, <commands/*.cpp>, <modes/*.cpp>, <modules/*.cpp>, <modules/m_spanningtree/*.cpp>')
DFILES += socketengines/.$(SOCKETENGINE).d threadengines/.threadengine_pthread.d

all: inspircd commands modules

commands: $(CMD_TARGS)

modules: $(MOD_TARGS)

modules/m_spanningtree.so: $(SPANNINGTREE_TARGS)
	$(RUNCC) $(FLAGS) $(PICLDFLAGS) -o $@ $(SPANNINGTREE_TARGS)

inspircd: $(CORE_TARGS)
	$(RUNCC) $(FLAGS) $(CORE_FLAGS) -o $@ $(LDLIBS) $(CORE_TARGS)

.%.d: %.cpp
	@../make/calcdep.pl $<

.PHONY: all alldep commands modules

-include $(DFILES)
