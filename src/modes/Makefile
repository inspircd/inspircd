CC = i am cornholio
CXXFLAGS = -I../../include ${FLAGS}

all: modeclasses.a

umode_w.o: umode_w.cpp ../../include/base.h ../../include/modules.h ../../include/inspircd.h ../../include/channels.h ../../include/users.h ../../include/inspircd_config.h ../../include/mode.h
	$(CC) -pipe -export-dynamic -c umode_w.cpp

umode_s.o: umode_s.cpp ../../include/base.h ../../include/modules.h ../../include/inspircd.h ../../include/channels.h ../../include/users.h ../../include/inspircd_config.h ../../include/mode.h
	$(CC) -pipe -export-dynamic -c umode_s.cpp
umode_o.o: umode_o.cpp ../../include/base.h ../../include/modules.h ../../include/inspircd.h ../../include/channels.h ../../include/users.h ../../include/inspircd_config.h ../../include/mode.h
	$(CC) -pipe -export-dynamic -c umode_o.cpp

umode_n.o: umode_n.cpp ../../include/base.h ../../include/modules.h ../../include/inspircd.h ../../include/channels.h ../../include/users.h ../../include/inspircd_config.h ../../include/mode.h
	$(CC) -pipe -export-dynamic -c umode_n.cpp

umode_i.o: umode_i.cpp ../../include/base.h ../../include/modules.h ../../include/inspircd.h ../../include/channels.h ../../include/users.h ../../include/inspircd_config.h ../../include/mode.h
	$(CC) -pipe -export-dynamic -c umode_i.cpp

cmode_v.o: cmode_v.cpp ../../include/base.h ../../include/modules.h ../../include/inspircd.h ../../include/channels.h ../../include/users.h ../../include/inspircd_config.h ../../include/mode.h
	$(CC) -pipe -export-dynamic -c cmode_v.cpp

cmode_t.o: cmode_t.cpp ../../include/base.h ../../include/modules.h ../../include/inspircd.h ../../include/channels.h ../../include/users.h ../../include/inspircd_config.h ../../include/mode.h
	$(CC) -pipe -export-dynamic -c cmode_t.cpp

cmode_s.o: cmode_s.cpp ../../include/base.h ../../include/modules.h ../../include/inspircd.h ../../include/channels.h ../../include/users.h ../../include/inspircd_config.h ../../include/mode.h
	$(CC) -pipe -export-dynamic -c cmode_s.cpp

cmode_p.o: cmode_p.cpp ../../include/base.h ../../include/modules.h ../../include/inspircd.h ../../include/channels.h ../../include/users.h ../../include/inspircd_config.h ../../include/mode.h
	$(CC) -pipe -export-dynamic -c cmode_p.cpp

cmode_o.o: cmode_o.cpp ../../include/base.h ../../include/modules.h ../../include/inspircd.h ../../include/channels.h ../../include/users.h ../../include/inspircd_config.h ../../include/mode.h
	$(CC) -pipe -export-dynamic -c cmode_o.cpp

cmode_n.o: cmode_n.cpp ../../include/base.h ../../include/modules.h ../../include/inspircd.h ../../include/channels.h ../../include/users.h ../../include/inspircd_config.h ../../include/mode.h
	$(CC) -pipe -export-dynamic -c cmode_n.cpp

cmode_m.o: cmode_m.cpp ../../include/base.h ../../include/modules.h ../../include/inspircd.h ../../include/channels.h ../../include/users.h ../../include/inspircd_config.h ../../include/mode.h
	$(CC) -pipe -export-dynamic -c cmode_m.cpp

cmode_l.o: cmode_l.cpp ../../include/base.h ../../include/modules.h ../../include/inspircd.h ../../include/channels.h ../../include/users.h ../../include/inspircd_config.h ../../include/mode.h
	$(CC) -pipe -export-dynamic -c cmode_l.cpp

cmode_k.o: cmode_k.cpp ../../include/base.h ../../include/modules.h ../../include/inspircd.h ../../include/channels.h ../../include/users.h ../../include/inspircd_config.h ../../include/mode.h
	$(CC) -pipe -export-dynamic -c cmode_k.cpp

cmode_i.o: cmode_i.cpp ../../include/base.h ../../include/modules.h ../../include/inspircd.h ../../include/channels.h ../../include/users.h ../../include/inspircd_config.h ../../include/mode.h
	$(CC) -pipe -export-dynamic -c cmode_i.cpp

cmode_h.o: cmode_h.cpp ../../include/base.h ../../include/modules.h ../../include/inspircd.h ../../include/channels.h ../../include/users.h ../../include/inspircd_config.h ../../include/mode.h
	$(CC) -pipe -export-dynamic -c cmode_h.cpp

cmode_b.o: cmode_b.cpp ../../include/base.h ../../include/modules.h ../../include/inspircd.h ../../include/channels.h ../../include/users.h ../../include/inspircd_config.h ../../include/mode.h
	$(CC) -pipe -export-dynamic -c cmode_b.cpp

modeclasses.a: umode_w.o umode_s.o umode_o.o umode_n.o umode_i.o cmode_v.o cmode_t.o cmode_s.o cmode_p.o cmode_o.o cmode_n.o cmode_m.o cmode_l.o cmode_k.o cmode_i.o cmode_h.o cmode_b.o
	ar r modeclasses.a *.o
	ranlib modeclasses.a
