# Insp Makefile :p
#
# (C) ChatSpike development team
# Makefile by <Craig@ChatSpike.net>
# Small modifications by <brain@ChatSpike.net>
#

CC = im a cheezeball

SRC_1 = inspircd.cpp inspircd_util.cpp inspircd_io.cpp connection.cpp message.cpp commands.cpp
SRC_2 = dynamic.cpp users.cpp modules.cpp wildcard.cpp servers.cpp channels.cpp mode.cpp

SRC = $(SRC_1) $(SRC_2)
OBJS = $(SRC:.cpp=.o)

CXXFLAGS = -I$../../include ${FLAGS}

all: inspircd

inspircd: $(OBJS)
	$(CXX) -rdynamic $^ -o $@ $(LDLIBS)

