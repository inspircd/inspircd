# Insp Makefile :p
#
# (C) ChatSpike development team
# Makefile by <Craig@ChatSpike.net>
# Makefile version 2 (dynamically linked core) by <brain@inspircd.org>
#

CC = im a cheezeball

CXXFLAGS = -I$../include ${FLAGS}

all: inspircd

inspircd: inspircd.cpp libchannels.so libmode.so libxline.so libinspstring.so libasyncdns.so libbase.so libinspircdutil.so libinspircdio.so libconnection.so libmessage.so libcommands.so libdnsqueue.so libdynamic.so libusers.so libmodules.so libwildcard.so libservers.so
	$(CXX) -I../include $(FLAGS) -rdynamic -L. inspircd.cpp -o inspircd $(LDLIBS) libchannels.so libmode.so libxline.so libinspstring.so libasyncdns.so libbase.so libinspircdutil.so libinspircdio.so libconnection.so libmessage.so libcommands.so libdnsqueue.so libdynamic.so libusers.so libmodules.so libwildcard.so libservers.so

libchannels.so: channels.cpp ../include/base.h ../include/channels.h ../include/inspircd.h
	$(CC) -I../include $(FLAGS) -shared -o libchannels.so channels.cpp

libmode.so: mode.cpp ../include/base.h ../include/mode.h ../include/inspircd.h
	$(CC) -I../include $(FLAGS) -shared -o libmode.so mode.cpp

libxline.so: xline.cpp ../include/base.h ../include/xline.h ../include/inspircd.h
	$(CC) -I../include $(FLAGS) -shared -o libxline.so xline.cpp

libinspstring.so: inspstring.cpp ../include/base.h ../include/inspstring.h ../include/inspircd.h
	$(CC) -I../include $(FLAGS) -shared -o libinspstring.so inspstring.cpp

libasyncdns.so: dns.cpp ../include/base.h ../include/dns.h ../include/inspircd.h
	$(CC) -I../include $(FLAGS) -shared -o libasyncdns.so dns.cpp

libbase.so: base.cpp ../include/base.h
	$(CC) -I../include $(FLAGS) -shared -o libbase.so base.cpp

libinspircdutil.so: inspircd_util.cpp ../include/base.h ../include/inspircd_util.h ../include/inspircd.h
	$(CC) -I../include $(FLAGS) -shared -o libinspircdutil.so inspircd_util.cpp

libinspircdio.so: inspircd_io.cpp ../include/base.h ../include/inspircd_io.h ../include/inspircd.h
	$(CC) -I../include $(FLAGS) -shared -o libinspircdio.so inspircd_io.cpp

libconnection.so: connection.cpp ../include/base.h ../include/connection.h ../include/inspircd.h
	$(CC) -I../include $(FLAGS) -shared -o libconnection.so connection.cpp

libmessage.so: message.cpp ../include/base.h ../include/message.h ../include/inspircd.h
	$(CC) -I../include $(FLAGS) -shared -o libmessage.so message.cpp

libcommands.so: commands.cpp ../include/base.h ../include/commands.h ../include/inspircd.h
	$(CC) -I../include $(FLAGS) -shared -o libcommands.so commands.cpp

libdnsqueue.so: dnsqueue.cpp ../include/base.h ../include/dnsqueue.h ../include/inspircd.h
	$(CC) -I../include $(FLAGS) -shared -o libdnsqueue.so dnsqueue.cpp

libdynamic.so: dynamic.cpp ../include/base.h ../include/dynamic.h ../include/inspircd.h
	$(CC) -I../include $(FLAGS) -shared -o libdynamic.so dynamic.cpp

libusers.so: users.cpp ../include/base.h ../include/users.h ../include/inspircd.h
	$(CC) -I../include $(FLAGS) -shared -o libusers.so users.cpp

libmodules.so: modules.cpp ../include/base.h ../include/modules.h ../include/inspircd.h
	$(CC) -I../include $(FLAGS) -shared -o libmodules.so modules.cpp

libwildcard.so: wildcard.cpp ../include/base.h ../include/wildcard.h ../include/inspircd.h
	$(CC) -I../include $(FLAGS) -shared -o libwildcard.so wildcard.cpp

libservers.so: servers.cpp ../include/base.h ../include/servers.h ../include/inspircd.h
	$(CC) -I../include $(FLAGS) -shared -o libservers.so servers.cpp

