/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2018 Adam <Adam@anope.org>
 *   Copyright (C) 2017-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013-2014, 2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2009 John Brooks <special@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007-2008 Craig Edwards <brain@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
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
#include "modules/account.h"
#include "modules/isupport.h"
#include "modules/who.h"
#include "timeutils.h"

enum
{
	// From RFC 1459.
	RPL_ENDOFWHO = 315,
	RPL_WHOREPLY = 352,

	// From ircu.
	RPL_WHOSPCRPL = 354
};

static constexpr char whox_field_order[] = "tcuihsnfdlaor";
static constexpr char who_field_order[] = "cuhsnf";

struct WhoData final
	: public Who::Request
{
	bool GetFieldIndex(char flag, size_t& out) const override
	{
		if (!whox)
		{
			const char* pos = strchr(who_field_order, flag);
			if (!pos)
				return false;

			out = pos - who_field_order;
			return true;
		}

		if (!whox_fields[flag])
			return false;

		out = 0;
		for (const char* c = whox_field_order; *c && *c != flag; ++c)
		{
			if (whox_fields[*c])
				++out;
		}

		return whox_field_order[out];
	}

	WhoData(const CommandBase::Params& parameters)
	{
		// Find the matchtext and swap the 0 for a * so we can use InspIRCd::Match on it.
		matchtext = parameters.size() > 2 ? parameters[2] : parameters[0];
		if (matchtext == "0")
			matchtext = "*";

		// If flags have been specified by the source.
		if (parameters.size() > 1)
		{
			CharState* current_bitset = &flags;
			for (std::string::const_iterator iter = parameters[1].begin(); iter != parameters[1].end(); ++iter)
			{
				unsigned char chr = static_cast<unsigned char>(*iter);

				// If the source specifies a percentage the rest of the flags are WHOX fields.
				if (chr == '%')
				{
					whox = true;
					current_bitset = &whox_fields;
					continue;
				}

				// If we are in WHOX mode and the source specifies a comma
				// the rest of the parameter is the query type.
				if (whox && chr == ',')
				{
					whox_querytype.assign(++iter, parameters[1].end());
					break;
				}

				// The source specified a matching flag.
				current_bitset->set(chr);
			}
		}

		// Fuzzy matches are when the source has not specified a specific user.
		fuzzy_match = flags.any() || (matchtext.find_first_of("*?.") != std::string::npos);
	}
};

class CommandWho final
	: public SplitCommand
{
private:
	Account::API accountapi;
	ChanModeReference secretmode;
	ChanModeReference privatemode;
	UserModeReference invisiblemode;
	Events::ModuleEventProvider whoevprov;
	Events::ModuleEventProvider whomatchevprov;
	Events::ModuleEventProvider whovisibleevprov;

	void BuildOpLevels()
	{
		// Build a map of prefixes ordered descending by their rank.
		std::multimap<ModeHandler::Rank, const PrefixMode*, std::greater<>> ranks;
		for (const auto* pm : ServerInstance->Modes.GetPrefixModes())
			ranks.insert(std::make_pair(pm->GetPrefixRank(), pm));

		// Now we have the ranks ordered we can assign them levels.
		ModeHandler::Rank lastrank = 0;
		unsigned int oplevel = 0;
		for (const auto& [_, pm] : ranks)
		{
			if (lastrank && pm->GetPrefixRank() != lastrank)
				oplevel++; // Keep the same op level for modes with the same prefix rank.

			lastrank = pm->GetPrefixRank();
			oplevels[pm->GetModeChar()] = ConvToStr(oplevel);
			ServerInstance->Logs.Debug(MODNAME, "Assigned oplevel {} to the {} ({}) prefix mode.",
				oplevel, pm->GetModeChar(), pm->name);
		}
	}

	/** Determines whether a user can view the users of a channel. */
	bool CanView(Channel* chan, User* user)
	{
		// If we are in a channel we can view all users in it.
		if (chan->HasUser(user))
			return true;

		// Opers with the users/auspex priv can see everything.
		if (user->HasPrivPermission("users/auspex"))
			return true;

		// You can see inside a channel from outside unless it is secret or private.
		return !chan->IsModeSet(secretmode) && !chan->IsModeSet(privatemode);
	}

	/** Gets the first channel which is visible between the source and the target users. */
	Membership* GetFirstVisibleChannel(const WhoData& data, LocalUser* source, User* user)
	{
		for (auto* memb : user->chans)
		{
			// Let a module handle this first if it wants to.
			ModResult res = whovisibleevprov.FirstResult(&Who::VisibleEventListener::OnWhoVisible, data, source, memb);
			if (res == MOD_RES_ALLOW)
				return memb; // Module explicitly picked this chan.

			if (res == MOD_RES_DENY)
				continue; // Module explicitly rejected this chan.

			// A module didn't specify either way so use the default behaviour:
			// 1. The requesting user is getting a WHO response for themself.
			// 2. The +s (secret) and +p (private) channel modes are not set.
			// 3. The requesting user is a member of the channel.
			if (source == user || (!memb->chan->IsModeSet(secretmode) && !memb->chan->IsModeSet(privatemode)) || memb->chan->HasUser(source))
				return memb;
		}
		return nullptr;
	}

	/** Determines whether WHO flags match a specific channel user. */
	static bool MatchChannel(LocalUser* source, Membership* memb, WhoData& data);

	/** Determines whether WHO flags match a specific user. */
	bool MatchUser(LocalUser* source, User* target, WhoData& data);

	/** Performs a WHO request on a channel. */
	void WhoChannel(LocalUser* source, const std::vector<std::string>& parameters, WhoData& data);

	/** Template for getting a user from various types of collection. */
	template<typename T>
	static User* GetUser(T& t);

	/** Performs a WHO request on a list of users. */
	template<typename T>
	void WhoUsers(LocalUser* source, const std::vector<std::string>& parameters, const T& users, WhoData& data);

public:
	insp::flat_map<char, std::string> oplevels;

	CommandWho(Module* parent)
		: SplitCommand(parent, "WHO", 1, 3)
		, accountapi(parent)
		, secretmode(parent, "secret")
		, privatemode(parent, "private")
		, invisiblemode(parent, "invisible")
		, whoevprov(parent, "event/who")
		, whomatchevprov(parent, "event/who-match")
		, whovisibleevprov(parent, "event/who-visible")
	{
		syntax = {
			"<server>|<nick>|<channel>|<realname>|<host>|0 [Aafhilmnoprstux]%[acdfhilnorstu]",
			"<unused> [Aafhilmnoprstux]%[acdfhilnorstu] :<server>|<nick>|<channel>|<realname>|<host>|0]"
		};
	}

	/** Sends a WHO reply to a user. */
	void SendWhoLine(LocalUser* user, const std::vector<std::string>& parameters, Membership* memb, User* u, WhoData& data);

	CmdResult HandleLocal(LocalUser* user, const Params& parameters) override;
};

template<> User* CommandWho::GetUser(UserManager::OperList::const_iterator& t) { return *t; }
template<> User* CommandWho::GetUser(UserMap::const_iterator& t) { return t->second; }

bool CommandWho::MatchChannel(LocalUser* source, Membership* memb, WhoData& data)
{
	bool source_has_users_auspex = source->HasPrivPermission("users/auspex");
	bool source_can_see_server = ServerInstance->Config->HideServer.empty() || source_has_users_auspex;

	// The source only wants remote users. This user is eligible if:
	//   (1) The source can't see server information.
	//   (2) The source is not local to the current server.
	LocalUser* lu = IS_LOCAL(memb->user);
	if (data.flags['f'] && source_can_see_server && lu)
		return false;

	// The source only wants local users. This user is eligible if:
	//   (1) The source can't see server information.
	//   (2) The source is local to the current server.
	if (data.flags['l'] && source_can_see_server && !lu)
		return false;

	// Only show operators if the oper flag has been specified.
	if (data.flags['o'] && !memb->user->IsOper())
		return false;

	// All other flags are ignored for channels.
	return true;
}

bool CommandWho::MatchUser(LocalUser* source, User* user, WhoData& data)
{
	// Users who are not fully connected can never match.
	if (!user->IsFullyConnected())
		return false;

	bool source_has_users_auspex = source->HasPrivPermission("users/auspex");
	bool source_can_see_target = source == user || source_has_users_auspex;
	bool source_can_see_server = ServerInstance->Config->HideServer.empty() || source_has_users_auspex;

	// The source only wants remote users. This user is eligible if:
	//   (1) The source can't see server information.
	//   (2) The source is not local to the current server.
	LocalUser* lu = IS_LOCAL(user);
	if (data.flags['f'] && source_can_see_server && lu)
		return false;

	// The source only wants local users. This user is eligible if:
	//   (1) The source can't see server information.
	//   (2) The source is local to the current server.
	if (data.flags['l'] && source_can_see_server && !lu)
		return false;

	// Let a module handle this first if it wants to.
	ModResult res = whomatchevprov.FirstResult(&Who::MatchEventListener::OnWhoMatch, data, source, user);
	if (res == MOD_RES_ALLOW)
		return true; // Module explicitly matched.

	else if (res == MOD_RES_DENY)
		return false; // Module explicitly rejected.

	// The source wants to match against users' away messages.
	bool match = false;
	if (data.flags['A'])
		match = user->IsAway() && InspIRCd::Match(user->away->message, data.matchtext, ascii_case_insensitive_map);

	// The source wants to match against users' account names.
	else if (data.flags['a'])
	{
		const std::string* account = accountapi ? accountapi->GetAccountName(user) : nullptr;
		match = account && InspIRCd::Match(*account, data.matchtext);
	}

	// The source wants to match against users' hostnames.
	else if (data.flags['h'])
	{
		const std::string host = user->GetHost(source_can_see_target && data.flags['x']);
		match = InspIRCd::Match(host, data.matchtext, ascii_case_insensitive_map);
	}

	// The source wants to match against users' IP addresses.
	else if (data.flags['i'])
		match = source_can_see_target && InspIRCd::MatchCIDR(user->GetAddress(), data.matchtext, ascii_case_insensitive_map);

	// The source wants to match against users' modes.
	else if (data.flags['m'])
	{
		if (source_can_see_target)
		{
			bool set = true;
			for (const auto chr : data.matchtext)
			{
				switch (chr)
				{
					// The following user modes should be set.
					case '+':
						set = true;
						break;

					// The following user modes should be unset.
					case '-':
						set = false;
						break;

					default:
						if (user->IsModeSet(chr) != set)
							return false;
						break;
				}
			}

			// All of the modes matched.
			return true;
		}
	}

	// The source wants to match against users' nicks.
	else if (data.flags['n'])
		match = InspIRCd::Match(user->nick, data.matchtext);

	// The source wants to match against users' connection ports.
	else if (data.flags['p'])
	{
		if (source_can_see_target && lu)
		{
			irc::portparser portrange(data.matchtext, false);
			while (long port = portrange.GetToken())
			{
				if (port >= std::numeric_limits<in_port_t>::min()
					&& port <= std::numeric_limits<in_port_t>::max()
					&& static_cast<in_port_t>(port) == lu->server_sa.port())
				{
					match = true;
					break;
				}
			}
		}
	}

	// The source wants to match against users' real names.
	else if (data.flags['r'])
		match = InspIRCd::Match(user->GetRealName(), data.matchtext, ascii_case_insensitive_map);

	else if (data.flags['s'])
	{
		bool show_real_server_name = ServerInstance->Config->HideServer.empty() || (source->HasPrivPermission("servers/auspex") && data.flags['x']);
		const std::string server = show_real_server_name ? user->server->GetName() : ServerInstance->Config->HideServer;
		match = InspIRCd::Match(server, data.matchtext, ascii_case_insensitive_map);
	}

	// The source wants to match against users' connection times.
	else if (data.flags['t'])
	{
		time_t seconds = ServerInstance->Time() - Duration::From(data.matchtext);
		if (user->signon >= seconds)
			match = true;
	}

	// The source wants to match against users' usernames.
	else if (data.flags['u'])
	{
		const std::string username = user->GetUser(source_can_see_target && data.flags['x']);
		match = InspIRCd::Match(username, data.matchtext, ascii_case_insensitive_map);
	}

	// The <name> passed to WHO is matched against users' host, server,
	// real name and nickname if the channel <name> cannot be found.
	else
	{
		const std::string hostname = user->GetHost(source_can_see_target && data.flags['x']);
		match = InspIRCd::Match(hostname, data.matchtext, ascii_case_insensitive_map);

		if (!match)
		{
			bool show_real_server_name = ServerInstance->Config->HideServer.empty() || (source->HasPrivPermission("servers/auspex") && data.flags['x']);
			const std::string server = show_real_server_name ? user->server->GetName() : ServerInstance->Config->HideServer;
			match = InspIRCd::Match(server, data.matchtext, ascii_case_insensitive_map);
		}

		if (!match)
			match = InspIRCd::Match(user->GetRealName(), data.matchtext, ascii_case_insensitive_map);

		if (!match)
			match = InspIRCd::Match(user->nick, data.matchtext);
	}

	return match;
}

void CommandWho::WhoChannel(LocalUser* source, const std::vector<std::string>& parameters, WhoData& data)
{
	if (!CanView(data.matchchan, source))
		return;

	bool inside = data.matchchan->HasUser(source);
	for (const auto& [user, memb] : data.matchchan->GetUsers())
	{
		// Only show invisible users if the source is in the channel or has the users/auspex priv.
		if (!inside && user->IsModeSet(invisiblemode) && !source->HasPrivPermission("users/auspex"))
			continue;

		// Skip the user if it doesn't match the query.
		if (!MatchChannel(source, memb, data))
			continue;

		SendWhoLine(source, parameters, memb, user, data);
	}
}

template<typename T>
void CommandWho::WhoUsers(LocalUser* source, const std::vector<std::string>& parameters, const T& users, WhoData& data)
{
	bool source_has_users_auspex = source->HasPrivPermission("users/auspex");
	for (typename T::const_iterator iter = users.begin(); iter != users.end(); ++iter)
	{
		User* user = GetUser(iter);

		// Only show users in response to a fuzzy WHO if we can see them normally.
		bool can_see_normally = user == source || source->SharesChannelWith(user) || !user->IsModeSet(invisiblemode);
		if (data.fuzzy_match && !can_see_normally && !source_has_users_auspex)
			continue;

		// Skip the user if it doesn't match the query.
		if (!MatchUser(source, user, data))
			continue;

		SendWhoLine(source, parameters, nullptr, user, data);
	}
}

void CommandWho::SendWhoLine(LocalUser* source, const std::vector<std::string>& parameters, Membership* memb, User* user, WhoData& data)
{
	if (!memb)
		memb = GetFirstVisibleChannel(data, source, user);

	bool source_can_see_target = source == user || source->HasPrivPermission("users/auspex");
	Numeric::Numeric wholine(data.whox ? RPL_WHOSPCRPL : RPL_WHOREPLY);
	if (data.whox)
	{
		// The source used WHOX so we send a fancy customised response.

		// Include the query type in the reply.
		if (data.whox_fields['t'])
			wholine.push(data.whox_querytype.empty() || data.whox_querytype.length() > 3 ? "0" : data.whox_querytype);

		// Include the first channel name.
		if (data.whox_fields['c'])
			wholine.push(memb ? memb->chan->name : "*");

		// Include the user's username.
		if (data.whox_fields['u'])
			wholine.push(user->GetUser(source_can_see_target && data.flags['x']));

		// Include the user's IP address.
		if (data.whox_fields['i'])
			wholine.push(source_can_see_target ? user->GetAddress() : "255.255.255.255");

		// Include the user's hostname.
		if (data.whox_fields['h'])
			wholine.push(user->GetHost(source_can_see_target && data.flags['x']));

		// Include the server name.
		if (data.whox_fields['s'])
		{
			if (ServerInstance->Config->HideServer.empty() || (source->HasPrivPermission("servers/auspex") && data.flags['x']))
				wholine.push(user->server->GetName());
			else
				wholine.push(ServerInstance->Config->HideServer);
		}

		// Include the user's nickname.
		if (data.whox_fields['n'])
			wholine.push(user->nick);

		// Include the user's flags.
		if (data.whox_fields['f'])
		{
			// Away state.
			std::string flags(user->IsAway() ? "G" : "H");

			// Operator status.
			if (user->IsOper())
				flags.push_back('*');

			// Membership prefix.
			if (memb)
			{
				char prefix = memb->GetPrefixChar();
				if (prefix)
					flags.push_back(prefix);
			}

			wholine.push(flags);
		}

		// Include the number of hops between the users.
		if (data.whox_fields['d'])
			wholine.push("0");

		// Include the user's idle time.
		if (data.whox_fields['l'])
		{
			LocalUser* lu = IS_LOCAL(user);
			unsigned long idle = lu ? ServerInstance->Time() - lu->idle_lastmsg : 0;
			wholine.push(ConvToStr(idle));
		}

		// Include the user's account name.
		if (data.whox_fields['a'])
		{
			const std::string* account = accountapi ? accountapi->GetAccountName(user) : nullptr;
			wholine.push(account ? *account : "0");
		}

		// Include the user's operator rank level.
		if (data.whox_fields['o'])
		{
			// If we haven't built a table to convert from InspIRCd member
			// ranks to WHOX oplevels yet we need to do that here.
			if (oplevels.empty())
				BuildOpLevels();

			wholine.push(memb && memb->GetModeChar() ? oplevels[memb->GetModeChar()] : "n/a");
		}

		// Include the user's real name.
		if (data.whox_fields['r'])
			wholine.push(user->GetRealName());
	}
	else
	{
		// We are not using WHOX so we just send a plain RFC response.

		// Include the channel name.
		wholine.push(memb ? memb->chan->name : "*");

		// Include the user's username.
		wholine.push(user->GetUser(source_can_see_target && data.flags['x']));

		// Include the user's hostname.
		wholine.push(user->GetHost(source_can_see_target && data.flags['x']));

		// Include the server name.
		if (ServerInstance->Config->HideServer.empty() || (source->HasPrivPermission("servers/auspex") && data.flags['x']))
			wholine.push(user->server->GetName());
		else
			wholine.push(ServerInstance->Config->HideServer);

		// Include the user's nick.
		wholine.push(user->nick);

		// Include the user's flags.
		{
			// Away state.
			std::string flags(user->IsAway() ? "G" : "H");

			// Operator status.
			if (user->IsOper())
				flags.push_back('*');

			// Membership prefix.
			if (memb)
			{
				char prefix = memb->GetPrefixChar();
				if (prefix)
					flags.push_back(prefix);
			}

			wholine.push(flags);
		}

		// Include the number of hops between the users and the user's real name.
		wholine.push("0 ");
		wholine.GetParams().back().append(user->GetRealName());
	}

	ModResult res = whoevprov.FirstResult(&Who::EventListener::OnWhoLine, data, source, user, memb, wholine);
	if (res != MOD_RES_DENY)
		data.results.push_back(wholine);
}

CmdResult CommandWho::HandleLocal(LocalUser* user, const Params& parameters)
{
	WhoData data(parameters);

	// Is the source running a WHO on a channel?
	data.matchchan = ServerInstance->Channels.Find(data.matchtext);
	if (data.matchchan)
		WhoChannel(user, parameters, data);

	// If we only want to match against opers we only have to iterate the oper list.
	else if (data.flags['o'])
		WhoUsers(user, parameters, ServerInstance->Users.all_opers, data);

	// Otherwise we have to use the global user list.
	else
		WhoUsers(user, parameters, ServerInstance->Users.GetUsers(), data);

	// Send the results to the source.
	for (const auto& numeric : data.results)
		user->WriteNumeric(numeric);
	user->WriteNumeric(RPL_ENDOFWHO, (data.matchtext.empty() ? "*" : data.matchtext.c_str()), "End of /WHO list.");

	// Penalize the source a bit for large queries with one unit of penalty per 200 results.
	user->CommandFloodPenalty += data.results.size() * 5;
	return CmdResult::SUCCESS;
}

class CoreModWho final
	: public Module
	, public ISupport::EventListener
{
private:
	CommandWho cmd;

public:
	CoreModWho()
		: Module(VF_CORE | VF_VENDOR, "Provides the WHO command")
		, ISupport::EventListener(this)
		, cmd(this)
	{
	}

	void OnBuildISupport(ISupport::TokenMap& tokens) override
	{
		tokens["WHOX"];
	}

	void OnServiceAdd(ServiceProvider& provider) override
	{
		// If the service is a prefix mode we need to rebuild the oplevel map.
		if (provider.service == SERVICE_MODE && static_cast<ModeHandler&>(provider).IsPrefixMode())
			cmd.oplevels.clear();
	}

	void OnServiceDel(ServiceProvider& provider) override
	{
		this->OnServiceAdd(provider);
	}
};

MODULE_INIT(CoreModWho)
