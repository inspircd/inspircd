# Insp Makefile :p
#
# (C) ChatSpike development team
# Makefile by <Craig@ChatSpike.net>
# Makefile version 2 (dynamically linked core) by <brain@inspircd.org>
#

CC = im a cheezeball

CXXFLAGS = -I$../include ${FLAGS}

all: libIRCDhash.so libIRCDchannels.so libIRCDmode.so libIRCDxline.so libIRCDstring.so libIRCDasyncdns.so libIRCDbase.so libIRCDutil.so libIRCDio.so libIRCDconnection.so libIRCDmessage.so libIRCDcommands.so libIRCDdnsqueue.so libIRCDdynamic.so libIRCDusers.so libIRCDmodules.so libIRCDwildcard.so libIRCDservers.so libIRCDhelper.so inspircd

inspircd: inspircd.cpp ../include/base.h ../include/channels.h ../include/inspircd.h ../include/channels.h ../include/globals.h ../include/inspircd_config.h
	$(CXX) -I../include $(FLAGS) -rdynamic -L. inspircd.cpp -o inspircd $(LDLIBS) libIRCDchannels.so libIRCDmode.so libIRCDxline.so libIRCDstring.so libIRCDasyncdns.so libIRCDbase.so libIRCDutil.so libIRCDio.so libIRCDconnection.so libIRCDmessage.so libIRCDcommands.so libIRCDdnsqueue.so libIRCDdynamic.so libIRCDusers.so libIRCDmodules.so libIRCDwildcard.so libIRCDservers.so libIRCDhelper.so libIRCDhash.so

libIRCDhash.so: hashcomp.cpp ../include/base.h ../include/hashcomp.h ../include/inspircd.h ../include/users.h ../include/globals.h ../include/inspircd_config.h
	$(CC) -pipe -I../include $(FLAGS) -export-dynamic -c hashcomp.cpp
	$(CC) -shared -o libIRCDhash.so hashcomp.o

libIRCDhelper.so: helperfuncs.cpp ../include/base.h ../include/helperfuncs.h ../include/inspircd.h ../include/users.h ../include/globals.h ../include/inspircd_config.h
	$(CC) -pipe -I../include $(FLAGS) -export-dynamic -c helperfuncs.cpp
	$(CC) -shared -o libIRCDhelper.so helperfuncs.o

libIRCDchannels.so: channels.cpp ../include/base.h ../include/channels.h ../include/inspircd.h ../include/users.h ../include/globals.h ../include/inspircd_config.h
	$(CC) -pipe -I../include $(FLAGS) -export-dynamic -c channels.cpp
	$(CC) -shared -o libIRCDchannels.so channels.o

libIRCDmode.so: mode.cpp ../include/base.h ../include/mode.h ../include/inspircd.h ../include/channels.h ../include/users.h ../include/globals.h ../include/inspircd_config.h
	$(CC) -pipe -I../include $(FLAGS) -export-dynamic -c mode.cpp
	$(CC) -shared -o libIRCDmode.so mode.o

libIRCDxline.so: xline.cpp ../include/base.h ../include/xline.h ../include/inspircd.h ../include/channels.h ../include/users.h ../include/globals.h ../include/inspircd_config.h
	$(CC) -pipe -I../include $(FLAGS) -export-dynamic -c xline.cpp
	$(CC) -shared -o libIRCDxline.so xline.o

libIRCDstring.so: inspstring.cpp ../include/base.h ../include/inspstring.h ../include/inspircd.h ../include/channels.h ../include/users.h ../include/globals.h ../include/inspircd_config.h
	$(CC) -pipe -I../include $(FLAGS) -export-dynamic -c inspstring.cpp
	$(CC) -shared -o libIRCDstring.so inspstring.o

libIRCDasyncdns.so: dns.cpp ../include/base.h ../include/dns.h ../include/inspircd.h ../include/channels.h ../include/users.h ../include/globals.h ../include/inspircd_config.h
	$(CC) -pipe -I../include $(FLAGS) -export-dynamic -c dns.cpp
	$(CC) -shared -o libIRCDasyncdns.so dns.o

libIRCDbase.so: base.cpp ../include/base.h ../include/globals.h ../include/inspircd_config.h
	$(CC) -pipe -I../include $(FLAGS) -export-dynamic -c base.cpp
	$(CC) -shared -o libIRCDbase.so base.o

libIRCDutil.so: inspircd_util.cpp ../include/base.h ../include/inspircd_util.h ../include/inspircd.h ../include/channels.h ../include/users.h ../include/globals.h ../include/inspircd_config.h
	$(CC) -pipe -I../include $(FLAGS) -export-dynamic -c inspircd_util.cpp
	$(CC) -shared -o libIRCDutil.so inspircd_util.o

libIRCDio.so: inspircd_io.cpp ../include/base.h ../include/inspircd_io.h ../include/inspircd.h ../include/channels.h ../include/users.h ../include/globals.h ../include/inspircd_config.h
	$(CC) -pipe -I../include $(FLAGS) -export-dynamic -c inspircd_io.cpp
	$(CC) -shared -o libIRCDio.so inspircd_io.o

libIRCDconnection.so: connection.cpp ../include/base.h ../include/connection.h ../include/inspircd.h ../include/channels.h ../include/users.h ../include/globals.h ../include/inspircd_config.h
	$(CC) -pipe -I../include $(FLAGS) -export-dynamic -c connection.cpp
	$(CC) -shared -o libIRCDconnection.so connection.o

libIRCDmessage.so: message.cpp ../include/base.h ../include/message.h ../include/inspircd.h ../include/channels.h ../include/users.h ../include/globals.h ../include/inspircd_config.h
	$(CC) -pipe -I../include $(FLAGS) -export-dynamic -c message.cpp
	$(CC) -shared -o libIRCDmessage.so message.o

libIRCDcommands.so: commands.cpp ../include/base.h ../include/commands.h ../include/inspircd.h ../include/channels.h ../include/users.h ../include/globals.h ../include/inspircd_config.h
	$(CC) -pipe -I../include $(FLAGS) -export-dynamic -c commands.cpp
	$(CC) -shared -o libIRCDcommands.so commands.o

libIRCDdnsqueue.so: dnsqueue.cpp ../include/base.h ../include/dnsqueue.h ../include/inspircd.h ../include/channels.h ../include/users.h ../include/globals.h ../include/inspircd_config.h
	$(CC) -pipe -I../include $(FLAGS) -export-dynamic -c dnsqueue.cpp
	$(CC) -shared -o libIRCDdnsqueue.so dnsqueue.o

libIRCDdynamic.so: dynamic.cpp ../include/base.h ../include/dynamic.h ../include/inspircd.h ../include/channels.h ../include/users.h ../include/globals.h ../include/inspircd_config.h
	$(CC) -pipe -I../include $(FLAGS) -export-dynamic -c dynamic.cpp
	$(CC) -shared -o libIRCDdynamic.so dynamic.o

libIRCDusers.so: users.cpp ../include/base.h ../include/users.h ../include/inspircd.h ../include/channels.h ../include/users.h ../include/globals.h ../include/inspircd_config.h ../include/connection.h
	$(CC) -pipe -I../include $(FLAGS) -export-dynamic -c users.cpp
	$(CC) -shared -o libIRCDusers.so users.o

libIRCDmodules.so: modules.cpp ../include/base.h ../include/modules.h ../include/inspircd.h ../include/channels.h ../include/users.h ../include/globals.h ../include/inspircd_config.h
	$(CC) -pipe -I../include $(FLAGS) -export-dynamic -c modules.cpp
	$(CC) -shared -o libIRCDmodules.so modules.o

libIRCDwildcard.so: wildcard.cpp ../include/base.h ../include/wildcard.h ../include/inspircd.h ../include/channels.h ../include/users.h ../include/globals.h ../include/inspircd_config.h
	$(CC) -pipe -I../include $(FLAGS) -export-dynamic -c wildcard.cpp
	$(CC) -shared -o libIRCDwildcard.so wildcard.o

libIRCDservers.so: servers.cpp ../include/base.h ../include/servers.h ../include/inspircd.h ../include/channels.h ../include/users.h ../include/globals.h ../include/inspircd_config.h ../include/connection.h
	$(CC) -pipe -I../include $(FLAGS) -export-dynamic -c servers.cpp
	$(CC) -shared -o libIRCDservers.so servers.o

