/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "inspircd.h"
#include "main.h"
#include "treesocket.h"

static const char* const forge_common_1201[] = {
	"m_allowinvite.so",
	"m_alltime.so",
	"m_auditorium.so",
	"m_banexception.so",
	"m_blockcaps.so",
	"m_blockcolor.so",
	"m_botmode.so",
	"m_censor.so",
	"m_chanfilter.so",
	"m_chanhistory.so",
	"m_channelban.so",
	"m_chanprotect.so",
	"m_chghost.so",
	"m_chgname.so",
	"m_commonchans.so",
	"m_customtitle.so",
	"m_deaf.so",
	"m_delayjoin.so",
	"m_delaymsg.so",
	"m_exemptchanops.so",
	"m_gecosban.so",
	"m_globops.so",
	"m_helpop.so",
	"m_hidechans.so",
	"m_hideoper.so",
	"m_invisible.so",
	"m_inviteexception.so",
	"m_joinflood.so",
	"m_kicknorejoin.so",
	"m_knock.so",
	"m_messageflood.so",
	"m_muteban.so",
	"m_nickflood.so",
	"m_nicklock.so",
	"m_noctcp.so",
	"m_nokicks.so",
	"m_nonicks.so",
	"m_nonotice.so",
	"m_nopartmsg.so",
	"m_ojoin.so",
	"m_operprefix.so",
	"m_permchannels.so",
	"m_redirect.so",
	"m_regex_glob.so",
	"m_regex_pcre.so",
	"m_regex_posix.so",
	"m_regex_tre.so",
	"m_remove.so",
	"m_sajoin.so",
	"m_sakick.so",
	"m_sanick.so",
	"m_sapart.so",
	"m_saquit.so",
	"m_serverban.so",
	"m_services_account.so",
	"m_servprotect.so",
	"m_setident.so",
	"m_showwhois.so",
	"m_silence.so",
	"m_sslmodes.so",
	"m_stripcolor.so",
	"m_swhois.so",
	"m_uninvite.so",
	"m_watch.so"
};

static std::string wide_newline("\r\n");
static std::string newline("\n");

void TreeSocket::CompatAddModules(std::vector<std::string>& modlist)
{
	if (proto_version < 1202)
	{
		// you MUST have chgident loaded in order to be able to translate FIDENT
		modlist.push_back("m_chgident.so");
		for(int i=0; i * sizeof(char*) < sizeof(forge_common_1201); i++)
		{
			if (ServerInstance->Modules->Find(forge_common_1201[i]))
				modlist.push_back(forge_common_1201[i]);
		}
		// module was merged
		if (ServerInstance->Modules->Find("m_operchans.so"))
		{
			modlist.push_back("m_operchans.so");
			modlist.push_back("m_operinvex.so");
		}
	}
}

void TreeSocket::WriteLine(std::string line)
{
	if (LinkState == CONNECTED)
	{
		if (line[0] != ':')
		{
			ServerInstance->Logs->Log("m_spanningtree", DEFAULT, "Sending line without server prefix!");
			line = ":" + ServerInstance->Config->GetSID() + " " + line;
		}
		if (proto_version != ProtocolVersion)
		{
			std::string::size_type a = line.find(' ');
			std::string::size_type b = line.find(' ', a + 1);
			std::string command = line.substr(a + 1, b-a-1);
			// now try to find a translation entry
			// TODO a more efficient lookup method will be needed later
			if (proto_version < 1202 && command == "FIDENT")
			{
				ServerInstance->Logs->Log("m_spanningtree",DEBUG,"Rewriting FIDENT for 1201-protocol server");
				line = ":" + ServerInstance->Config->GetSID() + " CHGIDENT " +  line.substr(1,a-1) + line.substr(b);
			}
			else if (proto_version < 1202 && command == "SAVE")
			{
				ServerInstance->Logs->Log("m_spanningtree",DEBUG,"Rewriting SAVE for 1201-protocol server");
				std::string::size_type c = line.find(' ', b + 1);
				std::string uid = line.substr(b, c - b);
				line = ":" + ServerInstance->Config->GetSID() + " SVSNICK" + uid + line.substr(b);
			}
			else if (proto_version < 1202 && command == "AWAY")
			{
				if (b != std::string::npos)
				{
					ServerInstance->Logs->Log("m_spanningtree",DEBUG,"Stripping AWAY timestamp for 1201-protocol server");
					std::string::size_type c = line.find(' ', b + 1);
					line.erase(b,c-b);
				}
			}
			else if (proto_version < 1202 && command == "ENCAP")
			{
				// :src ENCAP target command [args...]
				//     A     B      C       D
				// Therefore B and C cannot be npos in a valid command
				if (b == std::string::npos)
					return;
				std::string::size_type c = line.find(' ', b + 1);
				if (c == std::string::npos)
					return;
				std::string::size_type d = line.find(' ', c + 1);
				std::string subcmd = line.substr(c + 1, d - c - 1);

				if (subcmd == "CHGIDENT" && d != std::string::npos)
				{
					std::string::size_type e = line.find(' ', d + 1);
					if (e == std::string::npos)
						return; // not valid
					std::string target = line.substr(d + 1, e - d - 1);

					ServerInstance->Logs->Log("m_spanningtree",DEBUG,"Forging acceptance of CHGIDENT from 1201-protocol server");
					recvq.insert(0, ":" + target + " FIDENT " + line.substr(e) + "\n");
				}

				Command* thiscmd = ServerInstance->Parser->GetHandler(subcmd);
				if (thiscmd && subcmd != "WHOISNOTICE")
				{
					Version ver = thiscmd->creator->GetVersion();
					if (ver.Flags & VF_OPTCOMMON)
					{
						ServerInstance->Logs->Log("m_spanningtree",DEBUG,"Removing ENCAP on '%s' for 1201-protocol server",
							subcmd.c_str());
						line.erase(a, c-a);
					}
				}
			}
		}
	}

	ServerInstance->Logs->Log("m_spanningtree", RAWIO, "S[%d] O %s", this->GetFd(), line.c_str());
	this->WriteData(line);
	if (proto_version < 1202)
		this->WriteData(wide_newline);
	else
		this->WriteData(newline);
}
