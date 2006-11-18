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
INSTMODE = 0700
MAKEARGS = 'PROGS=${PROGS}' 'FLAGS=${FLAGS}' 'CC=${CC}' 'LDLIBS=${LDLIBS}' 'MODULES=${MODULES}' 'MODPATH=${MODPATH}' 'LIBPATH=${LIBPATH}' 'STATICLIBS=${STATICLIBS}'

all : @MAKEORDER@ finishmessage

mods:
	@echo "Making modules..."
	${MAKE} -C src/modules DIRNAME="src/modules" $(MAKEARGS)

# Make this depend on 'ircd' and 'mods', so its always output last even with -j
finishmessage: ircd mods
	@echo ""
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
	-install -m $(INSTMODE) src/libIRCDaes.so $(LIBPATH)
	-install -m $(INSTMODE) src/libIRCDasyncdns.so $(LIBPATH)
	-install -m $(INSTMODE) src/libIRCDbase.so $(LIBPATH)
	-install -m $(INSTMODE) src/libIRCDchannels.so $(LIBPATH)
	-install -m $(INSTMODE) src/libIRCDcommand_parse.so $(LIBPATH)
	-install -m $(INSTMODE) src/libIRCDcommands.so $(LIBPATH)
	-install -m $(INSTMODE) src/libIRCDconfigreader.so $(LIBPATH)
	-install -m $(INSTMODE) src/libIRCDcull_list.so $(LIBPATH)
	-install -m $(INSTMODE) src/libIRCDdynamic.so $(LIBPATH)
	-install -m $(INSTMODE) src/libIRCDhash.so $(LIBPATH)
	-install -m $(INSTMODE) src/libIRCDhelper.so $(LIBPATH)
	-install -m $(INSTMODE) src/libIRCDinspsocket.so $(LIBPATH)
	-install -m $(INSTMODE) src/libIRCDmode.so $(LIBPATH)
	-install -m $(INSTMODE) src/libIRCDmodules.so $(LIBPATH)
	-install -m $(INSTMODE) src/libIRCDsocket.so $(LIBPATH)
	-install -m $(INSTMODE) src/libIRCDsocketengine.so $(LIBPATH)
	-install -m $(INSTMODE) src/libIRCDstring.so $(LIBPATH)
	-install -m $(INSTMODE) src/libIRCDuserprocess.so $(LIBPATH)
	-install -m $(INSTMODE) src/libIRCDusers.so $(LIBPATH)
	-install -m $(INSTMODE) src/libIRCDwildcard.so $(LIBPATH)
	-install -m $(INSTMODE) src/libIRCDxline.so $(LIBPATH)
	-install -m $(INSTMODE) src/libIRCDtimer.so $(LIBPATH)
	-install -m $(INSTMODE) src/libIRCDsnomasks.so $(LIBPATH)
	-install -m $(INSTMODE) src/inspircd $(BINPATH)
	-install -m $(INSTMODE) inspircd $(BASE) 2>/dev/null
	-install -m $(INSTMODE) src/cmd_admin.so $(LIBPATH)
	-install -m $(INSTMODE) src/cmd_away.so $(LIBPATH)
	-install -m $(INSTMODE) src/cmd_commands.so $(LIBPATH)
	-install -m $(INSTMODE) src/cmd_connect.so $(LIBPATH)
	-install -m $(INSTMODE) src/cmd_die.so $(LIBPATH)
	-install -m $(INSTMODE) src/cmd_eline.so $(LIBPATH)
	-install -m $(INSTMODE) src/cmd_gline.so $(LIBPATH)
	-install -m $(INSTMODE) src/cmd_info.so $(LIBPATH)
	-install -m $(INSTMODE) src/cmd_invite.so $(LIBPATH)
	-install -m $(INSTMODE) src/cmd_ison.so $(LIBPATH)
	-install -m $(INSTMODE) src/cmd_join.so $(LIBPATH)
	-install -m $(INSTMODE) src/cmd_kick.so $(LIBPATH)
	-install -m $(INSTMODE) src/cmd_kill.so $(LIBPATH)
	-install -m $(INSTMODE) src/cmd_kline.so $(LIBPATH)
	-install -m $(INSTMODE) src/cmd_links.so $(LIBPATH)
	-install -m $(INSTMODE) src/cmd_list.so $(LIBPATH)
	-install -m $(INSTMODE) src/cmd_loadmodule.so $(LIBPATH)
	-install -m $(INSTMODE) src/cmd_lusers.so $(LIBPATH)
	-install -m $(INSTMODE) src/cmd_map.so $(LIBPATH)
	-install -m $(INSTMODE) src/cmd_modules.so $(LIBPATH)
	-install -m $(INSTMODE) src/cmd_motd.so $(LIBPATH)
	-install -m $(INSTMODE) src/cmd_names.so $(LIBPATH)
	-install -m $(INSTMODE) src/cmd_nick.so $(LIBPATH)
	-install -m $(INSTMODE) src/cmd_notice.so $(LIBPATH)
	-install -m $(INSTMODE) src/cmd_oper.so $(LIBPATH)
	-install -m $(INSTMODE) src/cmd_part.so $(LIBPATH)
	-install -m $(INSTMODE) src/cmd_pass.so $(LIBPATH)
	-install -m $(INSTMODE) src/cmd_ping.so $(LIBPATH)
	-install -m $(INSTMODE) src/cmd_pong.so $(LIBPATH)
	-install -m $(INSTMODE) src/cmd_privmsg.so $(LIBPATH)
	-install -m $(INSTMODE) src/cmd_qline.so $(LIBPATH)
	-install -m $(INSTMODE) src/cmd_quit.so $(LIBPATH)
	-install -m $(INSTMODE) src/cmd_rehash.so $(LIBPATH)
	-install -m $(INSTMODE) src/cmd_restart.so $(LIBPATH)
	-install -m $(INSTMODE) src/cmd_rules.so $(LIBPATH)
	-install -m $(INSTMODE) src/cmd_server.so $(LIBPATH)
	-install -m $(INSTMODE) src/cmd_squit.so $(LIBPATH)
	-install -m $(INSTMODE) src/cmd_stats.so $(LIBPATH)
	-install -m $(INSTMODE) src/cmd_summon.so $(LIBPATH)
	-install -m $(INSTMODE) src/cmd_time.so $(LIBPATH)
	-install -m $(INSTMODE) src/cmd_topic.so $(LIBPATH)
	-install -m $(INSTMODE) src/cmd_trace.so $(LIBPATH)
	-install -m $(INSTMODE) src/cmd_unloadmodule.so $(LIBPATH)
	-install -m $(INSTMODE) src/cmd_user.so $(LIBPATH)
	-install -m $(INSTMODE) src/cmd_userhost.so $(LIBPATH)
	-install -m $(INSTMODE) src/cmd_users.so $(LIBPATH)
	-install -m $(INSTMODE) src/cmd_version.so $(LIBPATH)
	-install -m $(INSTMODE) src/cmd_wallops.so $(LIBPATH)
	-install -m $(INSTMODE) src/cmd_who.so $(LIBPATH)
	-install -m $(INSTMODE) src/cmd_whois.so $(LIBPATH)
	-install -m $(INSTMODE) src/cmd_whowas.so $(LIBPATH)
	-install -m $(INSTMODE) src/cmd_zline.so $(LIBPATH)
	-install -m $(INSTMODE) src/cmd_mode.so $(LIBPATH)
	@-cp .gdbargs $(BASE)
	@-cp conf/inspircd.quotes.example $(CONPATH)
	@-cp conf/inspircd.rules.example $(CONPATH)
	@-cp conf/key.pem $(CONPATH)
	@-cp conf/cert.pem $(CONPATH)
	@-cp docs/inspircd.conf.example $(CONPATH)
	${MAKE} -C src/modules DIRNAME="src/modules" $(MAKEARGS) modinst
	@echo ""
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

