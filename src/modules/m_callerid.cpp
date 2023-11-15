/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2017-2019, 2021 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013 Adam <Adam@anope.org>
 *   Copyright (C) 2012-2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2009 John Brooks <special@inspircd.org>
 *   Copyright (C) 2008-2009 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2008 Thomas Stagner <aquanight@inspircd.org>
 *   Copyright (C) 2008 Craig Edwards <brain@inspircd.org>
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
#include "modules/callerid.h"
#include "modules/ctctags.h"
#include "modules/isupport.h"
#include "numerichelper.h"

enum
{
	RPL_ACCEPTLIST = 281,
	RPL_ENDOFACCEPT = 282,
	ERR_ACCEPTFULL = 456,
	ERR_ACCEPTEXIST = 457,
	ERR_ACCEPTNOT = 458,
	ERR_TARGUMODEG = 716,
	RPL_TARGNOTIFY = 717,
	RPL_UMODEGMSG = 718
};

class callerid_data final
{
public:
	typedef insp::flat_set<User*> UserSet;
	typedef std::vector<callerid_data*> CallerIdDataSet;

	time_t lastnotify = 0;

	/** Users I accept messages from
	 */
	UserSet accepting;

	/** Users who list me as accepted
	 */
	CallerIdDataSet wholistsme;

	std::string ToString(bool human) const
	{
		std::ostringstream oss;
		oss << lastnotify;
		for (const auto* u : accepting)
		{
			if (human)
				oss << ' ' << u->nick;
			else
				oss << ',' << u->uuid;
		}
		return oss.str();
	}
};

struct CallerIDExtInfo final
	: public ExtensionItem
{
	CallerIDExtInfo(Module* parent)
		: ExtensionItem(parent, "callerid_data", ExtensionType::USER)
	{
	}

	std::string ToHuman(const Extensible* container, void* item) const noexcept override
	{
		callerid_data* dat = static_cast<callerid_data*>(item);
		return dat->ToString(true);
	}

	std::string ToInternal(const Extensible* container, void* item) const noexcept override
	{
		callerid_data* dat = static_cast<callerid_data*>(item);
		return dat->ToString(false);
	}

	void FromInternal(Extensible* container, const std::string& value) noexcept override
	{
		if (container->extype != this->extype)
			return;

		void* old = GetRaw(container);
		if (old)
			this->Delete(nullptr, old);
		auto* dat = new callerid_data();
		SetRaw(container, dat);

		irc::commasepstream s(value);
		std::string tok;
		if (s.GetToken(tok))
			dat->lastnotify = ConvToNum<time_t>(tok);

		while (s.GetToken(tok))
		{
			auto* u = ServerInstance->Users.Find(tok, true);
			if (u && !u->quitting)
			{
				if (dat->accepting.insert(u).second)
				{
					callerid_data* other = this->Get(u, true);
					other->wholistsme.push_back(dat);
				}
			}
		}
	}

	callerid_data* Get(User* user, bool create)
	{
		callerid_data* dat = static_cast<callerid_data*>(GetRaw(user));
		if (create && !dat)
		{
			dat = new callerid_data;
			SetRaw(user, dat);
		}
		return dat;
	}

	void Delete(Extensible* container, void* item) override
	{
		callerid_data* dat = static_cast<callerid_data*>(item);

		// We need to walk the list of users on our accept list, and remove ourselves from their wholistsme.
		for (auto* user : dat->accepting)
		{
			callerid_data* target = this->Get(user, false);
			if (!target)
			{
				ServerInstance->Logs.Debug(MODNAME, "BUG: Inconsistency detected in callerid state, please report (1)");
				continue; // shouldn't happen, but oh well.
			}

			if (!stdalgo::vector::swaperase(target->wholistsme, dat))
				ServerInstance->Logs.Debug(MODNAME, "BUG: Inconsistency detected in callerid state, please report (2)");
		}
		delete dat;
	}
};

class CommandAccept final
	: public Command
{
	/** Pair: first is the target, second is true to add, false to remove
	 */
	typedef std::pair<User*, bool> ACCEPTAction;

	static ACCEPTAction GetTargetAndAction(std::string& tok, User* cmdfrom = nullptr)
	{
		bool remove = (tok[0] == '-');
		if ((remove) || (tok[0] == '+'))
			tok.erase(tok.begin());

		User* target;
		if (!cmdfrom || !IS_LOCAL(cmdfrom))
			target = ServerInstance->Users.Find(tok, true);
		else
			target = ServerInstance->Users.FindNick(tok, true);

		if (target && target->quitting)
			target = nullptr;

		return std::make_pair(target, !remove);
	}

public:
	CallerIDExtInfo extInfo;
	unsigned long maxaccepts;
	CommandAccept(Module* Creator)
		: Command(Creator, "ACCEPT", 1)
		, extInfo(Creator)
	{
		syntax = { "*|(+|-)<nick>[,(+|-)<nick>]+" };
		translation = { TR_CUSTOM };
	}

	void EncodeParameter(std::string& parameter, unsigned int index) override
	{
		// Send lists as-is (part of 2.0 compat)
		if (parameter.find(',') != std::string::npos)
			return;

		// Convert a (+|-)<nick> into a [-]<uuid>
		ACCEPTAction action = GetTargetAndAction(parameter);
		if (!action.first)
			return;

		parameter = (action.second ? "" : "-") + action.first->uuid;
	}

	/** Will take any number of nicks (up to MaxTargets), which can be separated by commas.
	 * - in front of any nick removes, and an * lists. This effectively means you can do:
	 * /accept nick1,nick2,nick3,*
	 * to add 3 nicks and then show your list
	 */
	CmdResult Handle(User* user, const Params& parameters) override
	{
		if (CommandParser::LoopCall(user, this, parameters, 0))
			return CmdResult::SUCCESS;

		/* Even if callerid mode is not set, we let them manage their ACCEPT list so that if they go +g they can
		 * have a list already setup. */

		if (parameters[0] == "*")
		{
			ListAccept(user);
			return CmdResult::SUCCESS;
		}

		std::string tok = parameters[0];
		ACCEPTAction action = GetTargetAndAction(tok, user);
		if (!action.first)
		{
			user->WriteNumeric(Numerics::NoSuchNick(tok));
			return CmdResult::FAILURE;
		}

		if ((!IS_LOCAL(user)) && (!IS_LOCAL(action.first)))
			// Neither source nor target is local, forward the command to the server of target
			return CmdResult::SUCCESS;

		// The second item in the pair is true if the first char is a '+' (or nothing), false if it's a '-'
		if (action.second)
			return (AddAccept(user, action.first) ? CmdResult::SUCCESS : CmdResult::FAILURE);
		else
			return (RemoveAccept(user, action.first) ? CmdResult::SUCCESS : CmdResult::FAILURE);
	}

	RouteDescriptor GetRouting(User* user, const Params& parameters) override
	{
		// There is a list in parameters[0] in two cases:
		// Either when the source is remote, this happens because 2.0 servers send comma separated uuid lists,
		// we don't split those but broadcast them, as before.
		//
		// Or if the source is local then LoopCall() runs OnPostCommand() after each entry in the list,
		// meaning the linking module has sent an ACCEPT already for each entry in the list to the
		// appropriate server and the ACCEPT with the list of nicks (this) doesn't need to be sent anywhere.
		if ((!IS_LOCAL(user)) && (parameters[0].find(',') != std::string::npos))
			return ROUTE_BROADCAST;

		// Find the target
		std::string targetstring = parameters[0];
		ACCEPTAction action = GetTargetAndAction(targetstring, user);
		if (!action.first)
			// Target is a "*" or source is local and the target is a list of nicks
			return ROUTE_LOCALONLY;

		// Route to the server of the target
		return ROUTE_UNICAST(action.first->server);
	}

	void ListAccept(User* user)
	{
		callerid_data* dat = extInfo.Get(user, false);
		if (dat)
		{
			for (const auto* accepted : dat->accepting)
				user->WriteNumeric(RPL_ACCEPTLIST, accepted->nick);
		}
		user->WriteNumeric(RPL_ENDOFACCEPT, "End of ACCEPT list");
	}

	bool AddAccept(User* user, User* whotoadd)
	{
		// Add this user to my accept list first, so look me up..
		callerid_data* dat = extInfo.Get(user, true);
		if (dat->accepting.size() >= maxaccepts)
		{
			user->WriteNumeric(ERR_ACCEPTFULL, INSP_FORMAT("Accept list is full (limit is {})", maxaccepts));
			return false;
		}
		if (!dat->accepting.insert(whotoadd).second)
		{
			user->WriteNumeric(ERR_ACCEPTEXIST, whotoadd->nick, "is already on your accept list");
			return false;
		}

		// Now, look them up, and add me to their list
		callerid_data* target = extInfo.Get(whotoadd, true);
		target->wholistsme.push_back(dat);

		user->WriteNotice(whotoadd->nick + " is now on your accept list");
		return true;
	}

	bool RemoveAccept(User* user, User* whotoremove)
	{
		// Remove them from my list, so look up my list..
		callerid_data* dat = extInfo.Get(user, false);
		if (!dat)
		{
			user->WriteNumeric(ERR_ACCEPTNOT, whotoremove->nick, "is not on your accept list");
			return false;
		}
		if (!dat->accepting.erase(whotoremove))
		{
			user->WriteNumeric(ERR_ACCEPTNOT, whotoremove->nick, "is not on your accept list");
			return false;
		}

		// Look up their list to remove me.
		callerid_data* dat2 = extInfo.Get(whotoremove, false);
		if (!dat2)
		{
			// How the fuck is this possible.
			ServerInstance->Logs.Debug(MODNAME, "BUG: Inconsistency detected in callerid state, please report (3)");
			return false;
		}

		if (!stdalgo::vector::swaperase(dat2->wholistsme, dat))
			ServerInstance->Logs.Debug(MODNAME, "BUG: Inconsistency detected in callerid state, please report (4)");

		user->WriteNotice(whotoremove->nick + " is no longer on your accept list");
		return true;
	}
};

class CallerIDAPIImpl final
	: public CallerID::APIBase
{
private:
	CallerIDExtInfo& ext;

public:
	CallerIDAPIImpl(Module* Creator, CallerIDExtInfo& Ext)
		: CallerID::APIBase(Creator)
		, ext(Ext)
	{
	}

	bool IsOnAcceptList(User* source, User* target) override
	{
		callerid_data* dat = ext.Get(target, true);
		return dat->accepting.count(source);
	}
};

class ModuleCallerID final
	: public Module
	, public CTCTags::EventListener
	, public ISupport::EventListener
{
private:
	CommandAccept cmd;
	CallerIDAPIImpl api;
	SimpleUserMode myumode;

	// Configuration variables:
	bool tracknick; // Allow ACCEPT entries to update with nick changes.
	unsigned long notify_cooldown; // Seconds between notifications.

	/** Removes a user from all accept lists
	 * @param who The user to remove from accepts
	 */
	void RemoveFromAllAccepts(User* who)
	{
		// First, find the list of people who have me on accept
		callerid_data* userdata = cmd.extInfo.Get(who, false);
		if (!userdata)
			return;

		// Iterate over the list of people who accept me, and remove all entries
		for (auto* dat : userdata->wholistsme)
		{
			// Find me on their callerid list
			if (!dat->accepting.erase(who))
				ServerInstance->Logs.Debug(MODNAME, "BUG: Inconsistency detected in callerid state, please report (5)");
		}

		userdata->wholistsme.clear();
	}

public:
	ModuleCallerID()
		: Module(VF_VENDOR | VF_COMMON, "Provides user mode g (callerid) which allows users to require that other users are on their whitelist before messaging them.")
		, CTCTags::EventListener(this)
		, ISupport::EventListener(this)
		, cmd(this)
		, api(this, cmd.extInfo)
		, myumode(this, "callerid", 'g')
	{
	}

	void OnBuildISupport(ISupport::TokenMap& tokens) override
	{
		tokens["ACCEPT"] = ConvToStr(cmd.maxaccepts);
		tokens["CALLERID"] = ConvToStr(myumode.GetModeChar());
	}

	ModResult HandleMessage(User* user, const MessageTarget& target)
	{
		if (!IS_LOCAL(user) || target.type != MessageTarget::TYPE_USER)
			return MOD_RES_PASSTHRU;

		User* dest = target.Get<User>();
		if (!dest->IsModeSet(myumode) || (user == dest))
			return MOD_RES_PASSTHRU;

		if (user->HasPrivPermission("users/ignore-callerid"))
			return MOD_RES_PASSTHRU;

		callerid_data* dat = cmd.extInfo.Get(dest, true);
		if (!dat->accepting.count(user))
		{
			time_t now = ServerInstance->Time();
			/* +g and *not* accepted */
			user->WriteNumeric(ERR_TARGUMODEG, dest->nick, INSP_FORMAT("is in +{} mode (server-side ignore).", myumode.GetModeChar()));
			if (now > (dat->lastnotify + long(notify_cooldown)))
			{
				user->WriteNumeric(RPL_TARGNOTIFY, dest->nick, "has been informed that you messaged them.");
				dest->WriteRemoteNumeric(RPL_UMODEGMSG, user->nick, user->GetUserHost(),
					INSP_FORMAT("is messaging you, and you have user mode +{} set. Use /ACCEPT +{} to allow.", myumode.GetModeChar(), user->nick)
				);
				dat->lastnotify = now;
			}
			return MOD_RES_DENY;
		}
		return MOD_RES_PASSTHRU;
	}

	ModResult OnUserPreMessage(User* user, MessageTarget& target, MessageDetails& details) override
	{
		return HandleMessage(user, target);
	}

	ModResult OnUserPreTagMessage(User* user, MessageTarget& target, CTCTags::TagMessageDetails& details) override
	{
		return HandleMessage(user, target);
	}

	void OnUserPostNick(User* user, const std::string& oldnick) override
	{
		if (!tracknick)
			RemoveFromAllAccepts(user);
	}

	void OnUserQuit(User* user, const std::string& message, const std::string& oper_message) override
	{
		RemoveFromAllAccepts(user);
	}

	void ReadConfig(ConfigStatus& status) override
	{
		const auto& tag = ServerInstance->Config->ConfValue("callerid");
		cmd.maxaccepts = tag->getNum<unsigned long>("maxaccepts", 30, 1);
		tracknick = tag->getBool("tracknick");
		notify_cooldown = tag->getDuration("cooldown", 60);
	}

	void Prioritize() override
	{
		// Want to be after modules like account or silence
		ServerInstance->Modules.SetPriority(this, I_OnUserPreMessage, PRIORITY_LAST);
	}
};

MODULE_INIT(ModuleCallerID)
