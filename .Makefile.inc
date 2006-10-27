# Inspircd Main Makefile
#
# (C) ChatSpike development team
# DO NOT MODIFY UNLESS YOU KNOW WHAT YOU ARE DOING!
#####
# Original Makefile & ./configure by <Craig@ChatSpike.net>
# Small modifications by <brain@ChatSpike.net>
#

CC = @CC@
MAKE = @MAKEPROG@
PROGS = inspircd
FLAGS = @FLAGS@
LDLIBS = @LDLIBS@
BASE = @BASE_DIR@
CONPATH = @CONFIG_DIR@
MODPATH = @MODULE_DIR@
BINPATH = @BINARY_DIR@
LIBPATH = @LIBRARY_DIR@
MODULES = @MODULES@
STATICLIBS = @STATICLIBS@
MAKEARGS = 'PROGS=${PROGS}' 'FLAGS=${FLAGS}' 'CC=${CC}' 'LDLIBS=${LDLIBS}' 'MODULES=${MODULES}' 'MODPATH=${MODPATH}' 'LIBPATH=${LIBPATH}' 'STATICLIBS=${STATICLIBS}'

all : @MAKEORDER@

mods:
	@echo "Making modules..."
	${MAKE} -C src/modules DIRNAME="src/modules" $(MAKEARGS)
	@echo "*************************************"
	@echo "*        BUILD COMPLETE!            *"
	@echo "*   To install InspIRCd, type:      *"
	@echo "*         make install              *"
	@echo "*************************************"

ircd:
	@echo "Making ircd..."
	${MAKE} -C src DIRNAME="src" $(MAKEARGS)

install: all
	@-mkdir -p ${BASE}
	@-mkdir -p $(LIBPATH)
	@-mkdir -p $(BINPATH)
	@-mkdir -p $(CONPATH)
	@-mkdir -p ${MODPATH}
	-install -m 0700 src/libIRCDaes.so $(LIBPATH)
	-install -m 0700 src/libIRCDasyncdns.so $(LIBPATH)
	-install -m 0700 src/libIRCDbase.so $(LIBPATH)
	-install -m 0700 src/libIRCDchannels.so $(LIBPATH)
	-install -m 0700 src/libIRCDcommand_parse.so $(LIBPATH)
	-install -m 0700 src/libIRCDcommands.so $(LIBPATH)
	-install -m 0700 src/libIRCDconfigreader.so $(LIBPATH)
	-install -m 0700 src/libIRCDcull_list.so $(LIBPATH)
	-install -m 0700 src/libIRCDdynamic.so $(LIBPATH)
	-install -m 0700 src/libIRCDhash.so $(LIBPATH)
	-install -m 0700 src/libIRCDhelper.so $(LIBPATH)
	-install -m 0700 src/libIRCDinspsocket.so $(LIBPATH)
	-install -m 0700 src/libIRCDmode.so $(LIBPATH)
	-install -m 0700 src/libIRCDmodules.so $(LIBPATH)
	-install -m 0700 src/libIRCDsocket.so $(LIBPATH)
	-install -m 0700 src/libIRCDsocketengine.so $(LIBPATH)
	-install -m 0700 src/libIRCDstring.so $(LIBPATH)
	-install -m 0700 src/libIRCDuserprocess.so $(LIBPATH)
	-install -m 0700 src/libIRCDusers.so $(LIBPATH)
	-install -m 0700 src/libIRCDwildcard.so $(LIBPATH)
	-install -m 0700 src/libIRCDxline.so $(LIBPATH)
	-install -m 0700 src/libIRCDtimer.so $(LIBPATH)
	-install -m 0700 src/libIRCDsnomasks.so $(LIBPATH)
	-install -m 0700 src/inspircd $(BINPATH)
	-install -m 0700 inspircd $(BASE) 2>/dev/null
	-install -m 0700 src/cmd_admin.so $(LIBPATH)
	-install -m 0700 src/cmd_away.so $(LIBPATH)
	-install -m 0700 src/cmd_commands.so $(LIBPATH)
	-install -m 0700 src/cmd_connect.so $(LIBPATH)
	-install -m 0700 src/cmd_die.so $(LIBPATH)
	-install -m 0700 src/cmd_eline.so $(LIBPATH)
	-install -m 0700 src/cmd_gline.so $(LIBPATH)
	-install -m 0700 src/cmd_info.so $(LIBPATH)
	-install -m 0700 src/cmd_invite.so $(LIBPATH)
	-install -m 0700 src/cmd_ison.so $(LIBPATH)
	-install -m 0700 src/cmd_join.so $(LIBPATH)
	-install -m 0700 src/cmd_kick.so $(LIBPATH)
	-install -m 0700 src/cmd_kill.so $(LIBPATH)
	-install -m 0700 src/cmd_kline.so $(LIBPATH)
	-install -m 0700 src/cmd_links.so $(LIBPATH)
	-install -m 0700 src/cmd_list.so $(LIBPATH)
	-install -m 0700 src/cmd_loadmodule.so $(LIBPATH)
	-install -m 0700 src/cmd_lusers.so $(LIBPATH)
	-install -m 0700 src/cmd_map.so $(LIBPATH)
	-install -m 0700 src/cmd_modules.so $(LIBPATH)
	-install -m 0700 src/cmd_motd.so $(LIBPATH)
	-install -m 0700 src/cmd_names.so $(LIBPATH)
	-install -m 0700 src/cmd_nick.so $(LIBPATH)
	-install -m 0700 src/cmd_notice.so $(LIBPATH)
	-install -m 0700 src/cmd_oper.so $(LIBPATH)
	-install -m 0700 src/cmd_part.so $(LIBPATH)
	-install -m 0700 src/cmd_pass.so $(LIBPATH)
	-install -m 0700 src/cmd_ping.so $(LIBPATH)
	-install -m 0700 src/cmd_pong.so $(LIBPATH)
	-install -m 0700 src/cmd_privmsg.so $(LIBPATH)
	-install -m 0700 src/cmd_qline.so $(LIBPATH)
	-install -m 0700 src/cmd_quit.so $(LIBPATH)
	-install -m 0700 src/cmd_rehash.so $(LIBPATH)
	-install -m 0700 src/cmd_restart.so $(LIBPATH)
	-install -m 0700 src/cmd_rules.so $(LIBPATH)
	-install -m 0700 src/cmd_server.so $(LIBPATH)
	-install -m 0700 src/cmd_squit.so $(LIBPATH)
	-install -m 0700 src/cmd_stats.so $(LIBPATH)
	-install -m 0700 src/cmd_summon.so $(LIBPATH)
	-install -m 0700 src/cmd_time.so $(LIBPATH)
	-install -m 0700 src/cmd_topic.so $(LIBPATH)
	-install -m 0700 src/cmd_trace.so $(LIBPATH)
	-install -m 0700 src/cmd_unloadmodule.so $(LIBPATH)
	-install -m 0700 src/cmd_user.so $(LIBPATH)
	-install -m 0700 src/cmd_userhost.so $(LIBPATH)
	-install -m 0700 src/cmd_users.so $(LIBPATH)
	-install -m 0700 src/cmd_version.so $(LIBPATH)
	-install -m 0700 src/cmd_wallops.so $(LIBPATH)
	-install -m 0700 src/cmd_who.so $(LIBPATH)
	-install -m 0700 src/cmd_whois.so $(LIBPATH)
	-install -m 0700 src/cmd_whowas.so $(LIBPATH)
	-install -m 0700 src/cmd_zline.so $(LIBPATH)
	-install -m 0700 src/cmd_mode.so $(LIBPATH)
	@-cp .gdbargs $(BASE)
	@-cp conf/inspircd.quotes.example $(CONPATH)
	@-cp conf/inspircd.rules.example $(CONPATH)
	@-cp conf/key.pem $(CONPATH)
	@-cp conf/cert.pem $(CONPATH)
	@-cp docs/inspircd.conf.example $(CONPATH)
	${MAKE} -C src/modules DIRNAME="src/modules" $(MAKEARGS) modinst
	@echo "*************************************"
	@echo "*        INSTALL COMPLETE!          *"
	@echo "* It is safe to ignore any messages *"
	@echo "* related to copying of files.      *"
	@echo "*                                   *"
	@echo "* REMEMBER TO EDIT YOUR CONFIG FILE *"
	@echo "*************************************"

clean:
	@echo Cleaning...
	@rm -rvf src/*.so src/*.o src/inspircd src/modules/*.so src/modules/*.o *~ src/*~ src/modules/*~ src/modules/extra/*~ src/modes/*~ src/modes/*.o src/modes/*.a
	@echo Completed.

modclean:
	rm -rf modules/*.so
	rm -rf src/modules/*.so
	rm -rf src/modules/*.o

squeakyclean: distclean

distclean: modclean clean

