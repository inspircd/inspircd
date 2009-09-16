SRC = $(SOURCEPATH)/src
VPATH = $(SRC)
CORE_TARGS = $(patsubst $(SRC)/%.cpp,%.o,$(wildcard $(SRC)/*.cpp $(SRC)/modes/*.cpp))
MOD_TARGS = $(patsubst $(SRC)/%.cpp,%.so,$(wildcard $(SRC)/modules/*.cpp))

CORE_TARGS += socketengines/$(SOCKETENGINE).o threadengines/threadengine_pthread.o
MOD_TARGS += $(shell perl -e 'chdir "$$ENV{SOURCEPATH}/src"; print join " ", grep s!/?$$!.so!, grep -d, <modules/m_*>')

DFILES = $(shell perl $(SOURCEPATH)/make/calcdep.pl -all)

all: inspircd modules

modules: $(MOD_TARGS)

inspircd: $(CORE_TARGS)
	$(RUNCC) -o $@ $(CORELDFLAGS) $(LDLIBS) $^

.PHONY: all alldep modules

-include $(DFILES)
