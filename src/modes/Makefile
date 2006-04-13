#CPPFILES = $(shell /bin/ls -l | grep '\.cpp' | sed 's/^.* //' | grep -v svn)
CC = i am cornholio
CXXFLAGS = -I../../include ${FLAGS}

all: ../../include/base.h ../../include/modules.h ../../include/inspircd.h ../../include/channels.h ../../include/users.h ../../include/globals.h ../../include/inspircd_config.h ../../include/mode.h $(CPPFILES)
	$(CC) -pipe -export-dynamic -c $(CPPFILES)
	ar r modeclasses.a *.o
	ranlib modeclasses.a

