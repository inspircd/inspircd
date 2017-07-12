/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Craig Edwards <craigedwards@brainbox.cc>
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
#include "modules/cap.h"
#include "modules/account.h"
#include "modules/sasl.h"
#include "modules/ssl.h"
#include "modules/spanningtree.h"

static std::string sasl_target;

class ServerTracker : public SpanningTreeEventListener
{
	bool online;

	void Update(const Server* server, bool linked)
	{
		if (sasl_target == "*")
			return;

		if (InspIRCd::Match(server->GetName(), sasl_target))
		{
			ServerInstance->Logs->Log(MODNAME, LOG_VERBOSE, "SASL target server \"%s\" %s", sasl_target.c_str(), (linked ? "came online" : "went offline"));
			online = linked;
		}
	}

	void OnServerLink(const Server* server) CXX11_OVERRIDE
	{
		Update(server, true);
	}

	void OnServerSplit(const Server* server) CXX11_OVERRIDE
	{
		Update(server, false);
	}

 public:
	ServerTracker(Module* mod)
		: SpanningTreeEventListener(mod)
	{
		Reset();
	}

	void Reset()
	{
		if (sasl_target == "*")
		{
			online = true;
			return;
		}

		online = false;

		ProtocolInterface::ServerList servers;
		ServerInstance->PI->GetServerList(servers);
		for (ProtocolInterface::ServerList::const_iterator i = servers.begin(); i != servers.end(); ++i)
		{
			const ProtocolInterface::ServerInfo& server = *i;
			if (InspIRCd::Match(server.servername, sasl_target))
			{
				online = true;
				break;
			}
		}
	}

	bool IsOnline() const { return online; }
};

class SASLCap : public Cap::Capability
{
	std::string mechlist;
	const ServerTracker& servertracker;

	bool OnRequest(LocalUser* user, bool adding) CXX11_OVERRIDE
	{
		// Requesting this cap is allowed anytime
		if (adding)
			return true;

		// But removing it can only be done when unregistered
		return (user->registered != REG_ALL);
	}

	bool OnList(LocalUser* user) CXX11_OVERRIDE
	{
		return servertracker.IsOnline();
	}

	const std::string* GetValue(LocalUser* user) const CXX11_OVERRIDE
	{
		return &mechlist;
	}

 public:
	SASLCap(Module* mod, const ServerTracker& tracker)
		: Cap::Capability(mod, "sasl")
		, servertracker(tracker)
	{
	}

	void SetMechlist(const std::string& newmechlist)
	{
		if (mechlist == newmechlist)
			return;

		mechlist = newmechlist;
		NotifyValueChange();
	}
};

enum SaslState { SASL_INIT, SASL_COMM, SASL_DONE };
enum SaslResult { SASL_OK, SASL_FAIL, SASL_ABORT };

static Events::ModuleEventProvider* saslevprov;

static void SendSASL(const parameterlist& params)
{
	if (!ServerInstance->PI->SendEncapsulatedData(sasl_target, "SASL", params))
	{
		FOREACH_MOD_CUSTOM(*saslevprov, SASLEventListener, OnSASLAuth, (params));
	}
}

/**
 * Tracks SASL authentication state like charybdis does. --nenolod
 */
class SaslAuthenticator
{
 private:
	std::string agent;
	User *user;
	SaslState state;
	SaslResult result;
	bool state_announced;

	/* taken from m_services_account */
	static bool ReadCGIIRCExt(const char* extname, User* user, std::string& out)
	{
		ExtensionItem* wiext = ServerInstance->Extensions.GetItem(extname);
		if (!wiext)
			return false;

		if (wiext->creator->ModuleSourceFile != "m_cgiirc.so")
			return false;

		StringExtItem* stringext = static_cast<StringExtItem*>(wiext);
		std::string* addr = stringext->get(user);
		if (!addr)
			return false;

		out = *addr;
		return true;
	}


	void SendHostIP()
	{
		std::string host, ip;

		if (!ReadCGIIRCExt("cgiirc_webirc_hostname", user, host))
		{
			host = user->host;
		}
		if (!ReadCGIIRCExt("cgiirc_webirc_ip", user, ip))
		{
			ip = user->GetIPString();
		}
		else
		{
			/* IP addresses starting with a : on irc are a Bad Thing (tm) */
			if (ip.c_str()[0] == ':')
				ip.insert(ip.begin(),1,'0');
		}

		parameterlist params;
		params.push_back(sasl_target);
		params.push_back("SASL");
		params.push_back(user->uuid);
		params.push_back("*");
		params.push_back("H");
		params.push_back(host);
		params.push_back(ip);

		SendSASL(params);
	}

 public:
	SaslAuthenticator(User* user_, const std::string& method)
		: user(user_), state(SASL_INIT), state_announced(false)
	{
		SendHostIP();

		parameterlist params;
		params.push_back(user->uuid);
		params.push_back("*");
		params.push_back("S");
		params.push_back(method);

		LocalUser* localuser = IS_LOCAL(user);
		if (localuser)
		{
			std::string fp = SSLClientCert::GetFingerprint(&localuser->eh);

			if (fp.size())
				params.push_back(fp);
		}

		SendSASL(params);
	}

	SaslResult GetSaslResult(const std::string &result_)
	{
		if (result_ == "F")
			return SASL_FAIL;

		if (result_ == "A")
			return SASL_ABORT;

		return SASL_OK;
	}

	/* checks for and deals with a state change. */
	SaslState ProcessInboundMessage(const std::vector<std::string> &msg)
	{
		switch (this->state)
		{
		 case SASL_INIT:
			this->agent = msg[0];
			this->state = SASL_COMM;
			/* fall through */
		 case SASL_COMM:
			if (msg[0] != this->agent)
				return this->state;

			if (msg.size() < 4)
				return this->state;

			if (msg[2] == "C")
				this->user->Write("AUTHENTICATE %s", msg[3].c_str());
			else if (msg[2] == "D")
			{
				this->state = SASL_DONE;
				this->result = this->GetSaslResult(msg[3]);
			}
			else if (msg[2] == "M")
				this->user->WriteNumeric(908, msg[3], "are available SASL mechanisms");
			else
				ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, "Services sent an unknown SASL message \"%s\" \"%s\"", msg[2].c_str(), msg[3].c_str());

			break;
		 case SASL_DONE:
			break;
		 default:
			ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, "WTF: SaslState is not a known state (%d)", this->state);
			break;
		}

		return this->state;
	}

	void Abort(void)
	{
		this->state = SASL_DONE;
		this->result = SASL_ABORT;
	}

	bool SendClientMessage(const std::vector<std::string>& parameters)
	{
		if (this->state != SASL_COMM)
			return true;

		parameterlist params;
		params.push_back(this->user->uuid);
		params.push_back(this->agent);
		params.push_back("C");

		params.insert(params.end(), parameters.begin(), parameters.end());

		SendSASL(params);

		if (parameters[0].c_str()[0] == '*')
		{
			this->Abort();
			return false;
		}

		return true;
	}

	void AnnounceState(void)
	{
		if (this->state_announced)
			return;

		switch (this->result)
		{
		 case SASL_OK:
			this->user->WriteNumeric(903, "SASL authentication successful");
			break;
	 	 case SASL_ABORT:
			this->user->WriteNumeric(906, "SASL authentication aborted");
			break;
		 case SASL_FAIL:
			this->user->WriteNumeric(904, "SASL authentication failed");
			break;
		 default:
			break;
		}

		this->state_announced = true;
	}
};

class CommandAuthenticate : public Command
{
 public:
	SimpleExtItem<SaslAuthenticator>& authExt;
	Cap::Capability& cap;
	CommandAuthenticate(Module* Creator, SimpleExtItem<SaslAuthenticator>& ext, Cap::Capability& Cap)
		: Command(Creator, "AUTHENTICATE", 1), authExt(ext), cap(Cap)
	{
		works_before_reg = true;
		allow_empty_last_param = false;
	}

	CmdResult Handle (const std::vector<std::string>& parameters, User *user)
	{
		{
			if (!cap.get(user))
				return CMD_FAILURE;

			if (parameters[0].find(' ') != std::string::npos || parameters[0][0] == ':')
				return CMD_FAILURE;

			SaslAuthenticator *sasl = authExt.get(user);
			if (!sasl)
				authExt.set(user, new SaslAuthenticator(user, parameters[0]));
			else if (sasl->SendClientMessage(parameters) == false)	// IAL abort extension --nenolod
			{
				sasl->AnnounceState();
				authExt.unset(user);
			}
		}
		return CMD_FAILURE;
	}
};

class CommandSASL : public Command
{
 public:
	SimpleExtItem<SaslAuthenticator>& authExt;
	CommandSASL(Module* Creator, SimpleExtItem<SaslAuthenticator>& ext) : Command(Creator, "SASL", 2), authExt(ext)
	{
		this->flags_needed = FLAG_SERVERONLY; // should not be called by users
	}

	CmdResult Handle(const std::vector<std::string>& parameters, User *user)
	{
		User* target = ServerInstance->FindUUID(parameters[1]);
		if (!target)
		{
			ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "User not found in sasl ENCAP event: %s", parameters[1].c_str());
			return CMD_FAILURE;
		}

		SaslAuthenticator *sasl = authExt.get(target);
		if (!sasl)
			return CMD_FAILURE;

		SaslState state = sasl->ProcessInboundMessage(parameters);
		if (state == SASL_DONE)
		{
			sasl->AnnounceState();
			authExt.unset(target);
		}
		return CMD_SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters)
	{
		return ROUTE_BROADCAST;
	}
};

class ModuleSASL : public Module
{
	SimpleExtItem<SaslAuthenticator> authExt;
	ServerTracker servertracker;
	SASLCap cap;
	CommandAuthenticate auth;
	CommandSASL sasl;
	Events::ModuleEventProvider sasleventprov;

 public:
	ModuleSASL()
		: authExt("sasl_auth", ExtensionItem::EXT_USER, this)
		, servertracker(this)
		, cap(this, servertracker)
		, auth(this, authExt, cap)
		, sasl(this, authExt)
		, sasleventprov(this, "event/sasl")
	{
		saslevprov = &sasleventprov;
	}

	void init() CXX11_OVERRIDE
	{
		if (!ServerInstance->Modules->Find("m_services_account.so") || !ServerInstance->Modules->Find("m_cap.so"))
			ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, "WARNING: m_services_account.so and m_cap.so are not loaded! m_sasl.so will NOT function correctly until these two modules are loaded!");
	}

	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		sasl_target = ServerInstance->Config->ConfValue("sasl")->getString("target", "*");
		servertracker.Reset();
	}

	void OnUserConnect(LocalUser *user) CXX11_OVERRIDE
	{
		SaslAuthenticator *sasl_ = authExt.get(user);
		if (sasl_)
		{
			sasl_->Abort();
			authExt.unset(user);
		}
	}

	void OnDecodeMetaData(Extensible* target, const std::string& extname, const std::string& extdata) CXX11_OVERRIDE
	{
		if ((target == NULL) && (extname == "saslmechlist"))
			cap.SetMechlist(extdata);
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides support for IRC Authentication Layer (aka: SASL) via AUTHENTICATE.", VF_VENDOR);
	}
};

MODULE_INIT(ModuleSASL)
