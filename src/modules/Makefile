# (C) ChatSpike development team
# Makefile by <Craig@ChatSpike.net>
# Small modifications by <brain@ChatSpike.net>
#

CXXFLAGS = -I../../include ${FLAGS}

MODS = `/bin/ls *.cpp | sed 's/.cpp//'`
MODULES = $(MODS)

MODS2 = `/bin/ls *.cpp`
MODULES2 = $(MODS2:.cpp=.so)

main:
	rm -f .make ; \
	for i in $(MODULES) ; do \
		echo "$(CXX) $(CXXFLAGS) -shared -o $$i `echo $$i | sed 's/.so/.cpp/'`">>.make ; \
	done ; chmod u+x .make ; ./.make


all: main

