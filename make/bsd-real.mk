VPATH = $(SOURCEPATH)/src
CORE_TARGS != perl -e 'chdir "$$ENV{SOURCEPATH}/src"; print join " ", grep s/\.cpp/.o/, <*.cpp>, <modes/*.cpp>'
MOD_TARGS != perl -e 'chdir "$$ENV{SOURCEPATH}/src"; print join " ", grep s/\.cpp/.so/, <modules/*.cpp>'
MDIR_TARGS != perl -e 'chdir "$$ENV{SOURCEPATH}/src"; print join " ", grep s!/?$$!.so!, grep -d, <modules/m_*>'

CORE_TARGS += socketengines/$(SOCKETENGINE).o threadengines/threadengine_pthread.o

DFILES != perl $(SOURCEPATH)/make/calcdep.pl -all

all: inspircd modules

modules: $(MOD_TARGS) $(MDIR_TARGS)

inspircd: $(CORE_TARGS)
	$(RUNCC) -o $@ $(CORELDFLAGS) $(LDLIBS) $^

.for FILE in $(DFILES)
.include "$(FILE)"
.endfor
