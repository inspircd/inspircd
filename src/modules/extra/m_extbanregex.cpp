/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018-2020 Matt Schatz <genius3000@g3k.solutions>
 *
 * This file is a module for InspIRCd.  InspIRCd is free software: you can
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

/// $ModAuthor: genius3000
/// $ModAuthorMail: genius3000@g3k.solutions
/// $ModConfig: <extbanregex engine="pcre" opersonly="yes">
/// $ModDepends: core 3
/// $ModDesc: Provides extban 'x' - Regex matching to n!u@h\sr

/* Since regex matching can be very CPU intensive, a config option is available
 * to limit the use of this extban to opers only and with an oper privilege
 * for control (channels/regex-extban). Default is true.
 * A SNOTICE is sent to the 'a' SNOMASK if a match from this extban takes
 * more than half a second.
 */

/* Helpop Lines for the EXTBANS section
 * Find: '<helpop key="extbans" title="Extended Bans" value="'
 * Place after the 's:<server>' line:
 x:<pattern>   Matches users to a regex pattern (requires a regex
               module and the extbanregex contrib module).
 */


#include "inspircd.h"
#include "listmode.h"
#include "modules/regex.h"

namespace
{
enum
{
	ERR_NOENGINE = 543,
	ERR_INVALIDMASK = 544
};

bool IsExtBanRegex(const std::string& mask)
{
	return ((mask.length() > 2) && (mask[0] == 'x') && (mask[1] == ':'));
}

bool IsNestedExtBanRegex(const std::string &mask)
{
	return ((mask.length() > 3) && (mask.find(":x:") != std::string::npos));
}

void RemoveAll(const std::string& engine, ChanModeReference& ban, ChanModeReference& exc, ChanModeReference& inv)
{
	std::vector<ListModeBase*> listmodes;
	listmodes.push_back(ban->IsListModeBase());
	if (exc)
		listmodes.push_back(exc->IsListModeBase());
	if (inv)
		listmodes.push_back(inv->IsListModeBase());

	// Loop each channel checking for any regex extbans
	// Batch removals with a Modes::ChangeList and Process()
	// Send a notice to hop/op if anything was removed
	const chan_hash& chans = ServerInstance->GetChans();
	for (chan_hash::const_iterator c = chans.begin(); c != chans.end(); ++c)
	{
		Channel* chan = c->second;
		Modes::ChangeList changelist;

		for (std::vector<ListModeBase*>::const_iterator i = listmodes.begin(); i != listmodes.end(); ++i)
		{
			ListModeBase* lm = *i;
			ListModeBase::ModeList* list = lm ? lm->GetList(chan) : NULL;
			if (!list)
				continue;

			for (ListModeBase::ModeList::const_iterator iter = list->begin(); iter != list->end(); ++iter)
			{
				if (IsExtBanRegex(iter->mask) || IsNestedExtBanRegex(iter->mask))
					changelist.push_remove(lm, iter->mask);
			}
		}

		if (changelist.empty())
			continue;

		ServerInstance->Modes.Process(ServerInstance->FakeClient, chan, NULL, changelist);

		const std::string msg = "Regex engine has changed to '" + engine + "'. All regex extbans have been removed";
		PrefixMode* hop = ServerInstance->Modes->FindPrefixMode('h');
		char pfxchar = (hop && hop->name == "halfop") ? hop->GetPrefix() : '@';

#if defined INSPIRCD_VERSION_BEFORE && INSPIRCD_VERSION_BEFORE(3, 5)
		ClientProtocol::Messages::Privmsg notice(ServerInstance->FakeClient, chan, msg, MSG_NOTICE);
		chan->Write(ServerInstance->GetRFCEvents().privmsg, notice, pfxchar);
		ServerInstance->PI->SendMessage(chan, pfxchar, msg, MSG_NOTICE);
#else
		chan->WriteNotice(msg, pfxchar);
#endif
	}
}
} // namespace

class WatchedMode : public ModeWatcher
{
	bool& opersonly;
	dynamic_reference<RegexFactory>& rxfactory;

 public:
	WatchedMode(Module *mod, bool& oo, dynamic_reference<RegexFactory>& rf, const std::string modename)
		: ModeWatcher(mod, modename, MODETYPE_CHANNEL)
		, opersonly(oo)
		, rxfactory(rf)
	{
	}

	bool BeforeMode(User* user, User*, Channel* chan, std::string& param, bool adding) CXX11_OVERRIDE
	{
		if (!adding || !IS_LOCAL(user))
			return true;

		if (!IsExtBanRegex(param) && !IsNestedExtBanRegex(param))
			return true;

		if (opersonly && !user->HasPrivPermission("channels/regex-extban"))
			return false;

		if (!rxfactory)
		{
			user->WriteNumeric(ERR_NOENGINE, "Regex engine is missing, cannot set a regex extban.");
			return false;
		}

		// Ensure mask is at least "!@", beyond that is up to the user
		const std::string mask = param.substr(param.find("x:") + 2);
		std::string::size_type plink = mask.find('!');
		if (plink == std::string::npos || mask.find('@', plink) == std::string::npos)
		{
			user->WriteNumeric(ERR_INVALIDMASK, mask, "Regex extban mask must be n!u@h\\sr");
			return false;
		}

		Regex* regex;
		try
		{
			regex = rxfactory->Create(mask);
			delete regex;
		}
		catch (ModuleException& e)
		{
			user->WriteNumeric(ERR_INVALIDMASK, mask, InspIRCd::Format("Regex extban mask is invalid (%s)",
				e.GetReason().c_str()));
			return false;
		}

		return true;
	}
};

class ModuleExtBanRegex : public Module
{
	bool initing;
	bool opersonly;
	ChanModeReference banmode;
	ChanModeReference banexceptionmode;
	ChanModeReference inviteexceptionmode;
	WatchedMode banwatcher;
	WatchedMode exceptionwatcher;
	WatchedMode inviteexceptionwatcher;

	dynamic_reference<RegexFactory> rxfactory;
	RegexFactory* factory;

 public:
	ModuleExtBanRegex()
		: initing(true)
		, opersonly(true)
		, banmode(this, "ban")
		, banexceptionmode(this, "banexception")
		, inviteexceptionmode(this, "invex")
		, banwatcher(this, opersonly, rxfactory, "ban")
		, exceptionwatcher(this, opersonly, rxfactory, "banexception")
		, inviteexceptionwatcher(this, opersonly, rxfactory, "invex")
		, rxfactory(this, "regex")
	{
	}

	void ReadConfig(ConfigStatus&) CXX11_OVERRIDE
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("extbanregex");
		opersonly = tag->getBool("opersonly", true);
		std::string newrxengine = tag->getString("engine");
		factory = rxfactory ? rxfactory.operator->() : NULL;

		if (newrxengine.empty())
			rxfactory.SetProvider("regex");
		else
			rxfactory.SetProvider("regex/" + newrxengine);

		if (!rxfactory)
		{
			if (newrxengine.empty())
				ServerInstance->SNO->WriteToSnoMask('a', "WARNING: No regex engine loaded - regex extban functionality disabled until this is corrected.");
			else
				ServerInstance->SNO->WriteToSnoMask('a', "WARNING: Regex engine '%s' is not loaded - regex extban functionality disabled until this is corrected.", newrxengine.c_str());

			RemoveAll("none", banmode, banexceptionmode, inviteexceptionmode);
		}
		else if (!initing && rxfactory.operator->() != factory)
		{
			ServerInstance->SNO->WriteToSnoMask('a', "Regex engine has changed to '%s', removing all regex extbans.", rxfactory->name.c_str());
			RemoveAll(rxfactory->name, banmode, banexceptionmode, inviteexceptionmode);
		}

		initing = false;
	}

	ModResult OnCheckBan(User* user, Channel* chan, const std::string& mask) CXX11_OVERRIDE
	{
		if (!factory)
			return MOD_RES_PASSTHRU;

		if (!IsExtBanRegex(mask))
			return MOD_RES_PASSTHRU;

		std::string dhost = user->GetFullHost() + " " + user->GetRealName();
		std::string host = user->GetFullRealHost() + " " + user->GetRealName();
		std::string ip = user->nick + "!" + user->MakeHostIP() + " " + user->GetRealName();

		struct timeval pretv, posttv;
		gettimeofday(&pretv, NULL);

		Regex* regex = factory->Create(mask.substr(2));
		bool matched = (regex->Matches(dhost) || regex->Matches(host) || regex->Matches(ip));
		delete regex;

		gettimeofday(&posttv, NULL);
		float timediff = ((double)(posttv.tv_usec - pretv.tv_usec) / 1000000) + (double)(posttv.tv_sec - pretv.tv_sec);
		if (timediff > 0.5)
		{
			ServerInstance->SNO->WriteGlobalSno('a', "*** extbanregex match took %f seconds on %s %s",
				timediff, chan->name.c_str(), mask.substr(2).c_str());
		}

		return (matched ? MOD_RES_DENY : MOD_RES_PASSTHRU);
	}

	void On005Numeric(std::map<std::string, std::string>& tokens) CXX11_OVERRIDE
	{
		tokens["EXTBAN"].push_back('x');
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Extban 'x' - regex matching to n!u@h\\sr", VF_OPTCOMMON, rxfactory ? rxfactory->name : "");
	}
};

MODULE_INIT(ModuleExtBanRegex)
