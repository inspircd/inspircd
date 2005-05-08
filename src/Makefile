# Insp Makefile :p
#
# (C) ChatSpike development team
# Makefile by <Craig@ChatSpike.net>
# Makefile version 2 (dynamically linked core) by <brain@inspircd.org>
#

CC = im a cheezeball

CXXFLAGS = -I$../include ${FLAGS}

all: libIRCDchannels.so libIRCDmode.so libIRCDxline.so libIRCDstring.so libIRCDasyncdns.so libIRCDbase.so libIRCDutil.so libIRCDio.so libIRCDconnection.so libIRCDmessage.so libIRCDcommands.so libIRCDdnsqueue.so libIRCDdynamic.so libIRCDusers.so libIRCDmodules.so libIRCDwildcard.so libIRCDservers.so inspircd

inspircd: inspircd.cpp ../include/base.h ../include/channels.h ../include/inspircd.h ../include/channels.h ../include/globals.h ../include/inspircd_config.h
	$(CXX) -I../include $(FLAGS) -rdynamic -L. inspircd.cpp -o inspircd $(LDLIBS) libIRCDchannels.so libIRCDmode.so libIRCDxline.so libIRCDstring.so libIRCDasyncdns.so libIRCDbase.so libIRCDutil.so libIRCDio.so libIRCDconnection.so libIRCDmessage.so libIRCDcommands.so libIRCDdnsqueue.so libIRCDdynamic.so libIRCDusers.so libIRCDmodules.so libIRCDwildcard.so libIRCDservers.so

libIRCDchannels.so: channels.cpp ../include/base.h ../include/channels.h ../include/inspircd.h ../include/users.h ../include/globals.h ../include/inspircd_config.h
	$(CC) -I../include $(FLAGS) -shared -o libIRCDchannels.so channels.cpp

libIRCDmode.so: mode.cpp ../include/base.h ../include/mode.h ../include/inspircd.h ../include/channels.h ../include/users.h ../include/globals.h ../include/inspircd_config.h
	$(CC) -I../include $(FLAGS) -shared -o libIRCDmode.so mode.cpp

libIRCDxline.so: xline.cpp ../include/base.h ../include/xline.h ../include/inspircd.h ../include/channels.h ../include/users.h ../include/globals.h ../include/inspircd_config.h
	$(CC) -I../include $(FLAGS) -shared -o libIRCDxline.so xline.cpp

libIRCDstring.so: inspstring.cpp ../include/base.h ../include/inspstring.h ../include/inspircd.h ../include/channels.h ../include/users.h ../include/globals.h ../include/inspircd_config.h
	$(CC) -I../include $(FLAGS) -shared -o libIRCDstring.so inspstring.cpp

libIRCDasyncdns.so: dns.cpp ../include/base.h ../include/dns.h ../include/inspircd.h ../include/channels.h ../include/users.h ../include/globals.h ../include/inspircd_config.h
	$(CC) -I../include $(FLAGS) -shared -o libIRCDasyncdns.so dns.cpp

libIRCDbase.so: base.cpp ../include/base.h ../include/globals.h ../include/inspircd_config.h
	$(CC) -I../include $(FLAGS) -shared -o libIRCDbase.so base.cpp

libIRCDutil.so: inspircd_util.cpp ../include/base.h ../include/inspircd_util.h ../include/inspircd.h ../include/channels.h ../include/users.h ../include/globals.h ../include/inspircd_config.h
	$(CC) -I../include $(FLAGS) -shared -o libIRCDutil.so inspircd_util.cpp

libIRCDio.so: inspircd_io.cpp ../include/base.h ../include/inspircd_io.h ../include/inspircd.h ../include/channels.h ../include/users.h ../include/globals.h ../include/inspircd_config.h
	$(CC) -I../include $(FLAGS) -shared -o libIRCDio.so inspircd_io.cpp

libIRCDconnection.so: connection.cpp ../include/base.h ../include/connection.h ../include/inspircd.h ../include/channels.h ../include/users.h ../include/globals.h ../include/inspircd_config.h
	$(CC) -I../include $(FLAGS) -shared -o libIRCDconnection.so connection.cpp

libIRCDmessage.so: message.cpp ../include/base.h ../include/message.h ../include/inspircd.h ../include/channels.h ../include/users.h ../include/globals.h ../include/inspircd_config.h
	$(CC) -I../include $(FLAGS) -shared -o libIRCDmessage.so message.cpp

libIRCDcommands.so: commands.cpp ../include/base.h ../include/commands.h ../include/inspircd.h ../include/channels.h ../include/users.h ../include/globals.h ../include/inspircd_config.h
	$(CC) -I../include $(FLAGS) -shared -o libIRCDcommands.so commands.cpp

libIRCDdnsqueue.so: dnsqueue.cpp ../include/base.h ../include/dnsqueue.h ../include/inspircd.h ../include/channels.h ../include/users.h ../include/globals.h ../include/inspircd_config.h
	$(CC) -I../include $(FLAGS) -shared -o libIRCDdnsqueue.so dnsqueue.cpp

libIRCDdynamic.so: dynamic.cpp ../include/base.h ../include/dynamic.h ../include/inspircd.h ../include/channels.h ../include/users.h ../include/globals.h ../include/inspircd_config.h
	$(CC) -I../include $(FLAGS) -shared -o libIRCDdynamic.so dynamic.cpp

libIRCDusers.so: users.cpp ../include/base.h ../include/users.h ../include/inspircd.h ../include/channels.h ../include/users.h ../include/globals.h ../include/inspircd_config.h
	$(CC) -I../include $(FLAGS) -shared -o libIRCDusers.so users.cpp

libIRCDmodules.so: modules.cpp ../include/base.h ../include/modules.h ../include/inspircd.h ../include/channels.h ../include/users.h ../include/globals.h ../include/inspircd_config.h
	$(CC) -I../include $(FLAGS) -shared -o libIRCDmodules.so modules.cpp

libIRCDwildcard.so: wildcard.cpp ../include/base.h ../include/wildcard.h ../include/inspircd.h ../include/channels.h ../include/users.h ../include/globals.h ../include/inspircd_config.h
	$(CC) -I../include $(FLAGS) -shared -o libIRCDwildcard.so wildcard.cpp

libIRCDservers.so: servers.cpp ../include/base.h ../include/servers.h ../include/inspircd.h ../include/channels.h ../include/users.h ../include/globals.h ../include/inspircd_config.h
	$(CC) -I../include $(FLAGS) -shared -o libIRCDservers.so servers.cpp

