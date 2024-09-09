/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2018 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2018 Dylan Frank <b00mx0r@aureus.pw>
 *   Copyright (C) 2017-2024 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2014-2015, 2018 Attila Molnar <attilamolnar@hush.com>
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
#include "clientprotocolevent.h"
#include "listmode.h"
#include "modules/isupport.h"
#include "utility/string.h"

#include "core_channel.h"
#include "invite.h"

enum
{
	// From RFC 1459.
	ERR_TOOMANYCHANNELS = 405,
	ERR_CHANNELISFULL = 471,
	ERR_INVITEONLYCHAN = 473,
	ERR_BADCHANNELKEY = 475,
};

namespace
{
/** Hook that sends a MODE after a JOIN if the user in the JOIN has some modes prefix set.
 * This happens e.g. when modules such as operprefix explicitly set prefix modes on the joining
 * user, or when a member with prefix modes does a host cycle.
 */
class JoinHook final
	: public ClientProtocol::EventHook
{
	ClientProtocol::Messages::Mode modemsg;
	Modes::ChangeList modechangelist;
	const User* joininguser;

public:
	/** If true, MODE changes after JOIN will be sourced from the user, rather than the server
	 */
	bool modefromuser;

	JoinHook(Module* mod)
		: ClientProtocol::EventHook(mod, "JOIN")
	{
	}

	void OnEventInit(const ClientProtocol::Event& ev) override
	{
		const ClientProtocol::Events::Join& join = static_cast<const ClientProtocol::Events::Join&>(ev);
		const Membership& memb = *join.GetMember();

		modechangelist.clear();
		for (const auto& mode : memb.modes)
			modechangelist.push_add(mode, memb.user->nick);

		if (modechangelist.empty())
		{
			// Member got no modes on join
			joininguser = nullptr;
			return;
		}

		joininguser = memb.user;

		// Prepare a mode protocol event that we can append to the message list in OnPreEventSend()
		modemsg.SetParams(memb.chan, nullptr, modechangelist);
		if (modefromuser)
			modemsg.SetSource(join);
		else
			modemsg.SetSourceUser(ServerInstance->FakeClient);
	}

	ModResult OnPreEventSend(LocalUser* user, const ClientProtocol::Event& ev, ClientProtocol::MessageList& messagelist) override
	{
		// If joininguser is NULL then they didn't get any modes on join, skip.
		// Also don't show their own modes to them, they get that in the NAMES list not via MODE.
		if ((joininguser) && (user != joininguser))
			messagelist.push_back(&modemsg);
		return MOD_RES_PASSTHRU;
	}
};

}

class CoreModChannel final
	: public Module
	, public CheckExemption::EventListener
	, public ISupport::EventListener
{
	Invite::APIImpl invapi;
	CommandInvite cmdinvite;
	CommandJoin cmdjoin;
	CommandKick cmdkick;
	CommandNames cmdnames;
	CommandTopic cmdtopic;
	Events::ModuleEventProvider evprov;
	JoinHook joinhook;

	ModeChannelBan banmode;
	SimpleChannelMode inviteonlymode;
	ModeChannelKey keymode;
	ModeChannelLimit limitmode;
	SimpleChannelMode moderatedmode;
	SimpleChannelMode noextmsgmode;
	ModeChannelOp opmode;
	SimpleChannelMode privatemode;
	SimpleChannelMode secretmode;
	SimpleChannelMode topiclockmode;
	ModeChannelVoice voicemode;

	insp::flat_map<std::string, char> exemptions;
	ExtBanManager extbanmgr;

	ModResult IsInvited(User* user, Channel* chan)
	{
		LocalUser* localuser = IS_LOCAL(user);
		if ((localuser) && (invapi.IsInvited(localuser, chan)))
			return MOD_RES_ALLOW;
		return MOD_RES_PASSTHRU;
	}

public:
	CoreModChannel()
		: Module(VF_CORE | VF_VENDOR, "Provides the INVITE, JOIN, KICK, NAMES, and TOPIC commands")
		, CheckExemption::EventListener(this, UINT_MAX)
		, ISupport::EventListener(this)
		, invapi(this)
		, cmdinvite(this, invapi)
		, cmdjoin(this)
		, cmdkick(this)
		, cmdnames(this)
		, cmdtopic(this)
		, evprov(this, "event/channel")
		, joinhook(this)
		, banmode(this)
		, inviteonlymode(this, "inviteonly", 'i')
		, keymode(this)
		, limitmode(this)
		, moderatedmode(this, "moderated", 'm')
		, noextmsgmode(this, "noextmsg", 'n')
		, opmode(this)
		, privatemode(this, "private", 'p')
		, secretmode(this, "secret", 's')
		, topiclockmode(this, "topiclock", 't')
		, voicemode(this)
		, extbanmgr(this, banmode)
	{
	}

	void ReadConfig(ConfigStatus& status) override
	{
		const auto& optionstag = ServerInstance->Config->ConfValue("options");

		std::string current;
		irc::spacesepstream defaultstream(optionstag->getString("exemptchanops"));
		insp::flat_map<std::string, char> exempts;
		while (defaultstream.GetToken(current))
		{
			std::string::size_type pos = current.find(':');
			if (pos == std::string::npos || (pos + 2) > current.size())
				throw ModuleException(this, "Invalid exemptchanops value '" + current + "' at " + optionstag->source.str());

			const std::string restriction = current.substr(0, pos);
			const char prefix = current[pos + 1];

			ServerInstance->Logs.Debug(MODNAME, "Exempting prefix {} from {}", prefix, restriction);
			exempts[restriction] = prefix;
		}

		ExtBan::Format newformat = optionstag->getEnum("extbanformat", ExtBan::Format::ANY, {
			{ "any",    ExtBan::Format::ANY },
			{ "name",   ExtBan::Format::NAME },
			{ "letter", ExtBan::Format::LETTER },
		});

		const auto& securitytag = ServerInstance->Config->ConfValue("security");
		Invite::AnnounceState newannouncestate = securitytag->getEnum("announceinvites", Invite::ANNOUNCE_DYNAMIC, {
			{ "all",     Invite::ANNOUNCE_ALL },
			{ "dynamic", Invite::ANNOUNCE_DYNAMIC },
			{ "none",    Invite::ANNOUNCE_NONE },
			{ "ops",     Invite::ANNOUNCE_OPS },
		});

		// Config is valid, apply it

		// Validates and applies <maxlist> tags, so do it first
		banmode.DoRehash();

		exemptions.swap(exempts);
		extbanmgr.format = newformat;
		invapi.announceinvites = newannouncestate;
		joinhook.modefromuser = optionstag->getBool("cyclehostsfromuser");

		Implementation events[] = { I_OnCheckKey, I_OnCheckLimit, I_OnCheckChannelBan };
		if (optionstag->getBool("invitebypassmodes", true))
			ServerInstance->Modules.Attach(events, this, sizeof(events)/sizeof(Implementation));
		else
			ServerInstance->Modules.Detach(events, this, sizeof(events)/sizeof(Implementation));

		const auto& limitstag = ServerInstance->Config->ConfValue("limits");
		keymode.maxkeylen = limitstag->getNum<size_t>("maxkey", 32, 1, ModeParser::MODE_PARAM_MAX);
	}

	void OnBuildISupport(ISupport::TokenMap& tokens) override
	{
		tokens["KEYLEN"] = ConvToStr(keymode.maxkeylen);
		extbanmgr.BuildISupport(tokens["EXTBAN"]);

		std::vector<std::string> limits;
		std::string vlist;
		for (auto* lm : ServerInstance->Modes.GetListModes())
		{
			limits.push_back(FMT::format("{}:{}", lm->GetModeChar(), lm->GetLowerLimit()));
			if (lm->HasVariableLength())
				vlist.push_back(lm->GetModeChar());
		}
		if (!vlist.empty())
			tokens["VLIST"] = vlist;

		std::sort(limits.begin(), limits.end());
		tokens["MAXLIST"] = insp::join(limits, ',');
	}

	void OnBuildClassISupport(const std::shared_ptr<ConnectClass>& klass, ISupport::TokenMap& tokens) override
	{
		tokens["CHANLIMIT"] = FMT::format("#:{}", klass->maxchans);
	}

	ModResult OnUserPreJoin(LocalUser* user, Channel* chan, const std::string& cname, std::string& privs, const std::string& keygiven, bool override) override
	{
		if (override)
		{
			// Don't enforce limits on overrides.
			return MOD_RES_PASSTHRU;
		}
		else
		{
			/* We don't restrict the number of channels that remote users or users that are override-joining may be in.
			 * We restrict local users to <connect:maxchans> channels.
			 * We restrict local operators to <oper:maxchans> channels.
			 */
			unsigned long maxchans = user->GetClass()->maxchans;
			if (user->IsOper())
				maxchans = user->oper->GetConfig()->getNum<unsigned long>("maxchans", maxchans, maxchans);

			if (user->chans.size() >= maxchans)
			{
				user->WriteNumeric(ERR_TOOMANYCHANNELS, cname, "You are on too many channels");
				return MOD_RES_DENY;
			}
		}

		if (!chan)
			return MOD_RES_PASSTHRU; // Can't have limits on a new channel.

		// Check whether the channel key is correct.
		const std::string ckey = chan->GetModeParameter(&keymode);
		if (!ckey.empty())
		{
			ModResult modres;
			FIRST_MOD_RESULT(OnCheckKey, modres, (user, chan, keygiven));
			if (!modres.check(InspIRCd::TimingSafeCompare(ckey, keygiven)))
			{
				// If no key provided, or key is not the right one, and can't bypass +k (not invited or option not enabled)
				user->WriteNumeric(ERR_BADCHANNELKEY, chan->name, "Cannot join channel (incorrect channel key)");
				return MOD_RES_DENY;
			}
		}

		// Check whether the invite only mode is set.
		if (chan->IsModeSet(inviteonlymode))
		{
			ModResult modres;
			FIRST_MOD_RESULT(OnCheckInvite, modres, (user, chan));
			if (modres != MOD_RES_ALLOW)
			{
				user->WriteNumeric(ERR_INVITEONLYCHAN, chan->name, "Cannot join channel (invite only)");
				return MOD_RES_DENY;
			}
		}

		// Check whether the limit would be exceeded by this user joining.
		if (chan->IsModeSet(limitmode))
		{
			ModResult modres;
			FIRST_MOD_RESULT(OnCheckLimit, modres, (user, chan));
			if (!modres.check(chan->GetUsers().size() < static_cast<size_t>(limitmode.ext.Get(chan))))
			{
				user->WriteNumeric(ERR_CHANNELISFULL, chan->name, "Cannot join channel (channel is full)");
				return MOD_RES_DENY;
			}
		}

		// Check whether the user is banned from joining the channel.
		if (chan->IsBanned(user))
		{
			user->WriteNumeric(ERR_BANNEDFROMCHAN, chan->name, "Cannot join channel (you're banned)");
			return MOD_RES_DENY;
		}

		// Everything looks okay.
		return MOD_RES_PASSTHRU;
	}

	void OnPostJoin(Membership* memb) override
	{
		Channel* const chan = memb->chan;
		LocalUser* const localuser = IS_LOCAL(memb->user);
		if (localuser)
		{
			// Remove existing invite, if any
			invapi.Remove(localuser, chan);

			if (chan->topic.length())
				Topic::ShowTopic(localuser, chan);

			// Show all members of the channel, including invisible (+i) users
			cmdnames.SendNames(localuser, chan, true);
		}
	}

	ModResult OnCheckKey(User* user, Channel* chan, const std::string& keygiven) override
	{
		// Hook only runs when being invited bypasses +bkl
		return IsInvited(user, chan);
	}

	ModResult OnCheckChannelBan(User* user, Channel* chan) override
	{
		// Hook only runs when being invited bypasses +bkl
		return IsInvited(user, chan);
	}

	ModResult OnCheckLimit(User* user, Channel* chan) override
	{
		// Hook only runs when being invited bypasses +bkl
		return IsInvited(user, chan);
	}

	ModResult OnCheckInvite(User* user, Channel* chan) override
	{
		// Hook always runs
		return IsInvited(user, chan);
	}

	void OnUserDisconnect(LocalUser* user) override
	{
		invapi.RemoveAll(user);
	}

	void OnChannelDelete(Channel* chan) override
	{
		// Make sure the channel won't appear in invite lists from now on, don't wait for cull to unset the ext
		invapi.RemoveAll(chan);
	}

	ModResult OnCheckBan(User* user, Channel* chan, const std::string& mask) override
	{
		bool inverted;
		std::string name;
		std::string value;
		if (!ExtBan::Parse(mask, name, value, inverted))
			return MOD_RES_PASSTHRU;

		ExtBan::Base* extban = nullptr;
		if (name.size() == 1)
			extban = extbanmgr.FindLetter(name[0]);
		else
			extban = extbanmgr.FindName(name);

		// It is formatted like an extban but isn't a matching extban.
		if (!extban || extban->GetType() != ExtBan::Type::MATCHING)
			return MOD_RES_PASSTHRU;

		return extban->IsMatch(user, chan, value) != inverted ? MOD_RES_DENY : MOD_RES_PASSTHRU;
	}

	ModResult OnCheckExemption(User* user, Channel* chan, const std::string& restriction) override
	{
		if (!exemptions.count(restriction))
			return MOD_RES_PASSTHRU;

		ModeHandler::Rank mypfx = chan->GetPrefixValue(user);
		char minmode = exemptions[restriction];

		PrefixMode* mh = ServerInstance->Modes.FindPrefixMode(minmode);
		if (mh && mypfx >= mh->GetPrefixRank())
			return MOD_RES_ALLOW;
		if (mh || minmode == '*')
			return MOD_RES_DENY;
		return MOD_RES_PASSTHRU;
	}

	void Prioritize() override
	{
		ServerInstance->Modules.SetPriority(this, I_OnPostJoin, PRIORITY_FIRST);
		ServerInstance->Modules.SetPriority(this, I_OnUserPreJoin, PRIORITY_LAST);
	}
};

MODULE_INIT(CoreModChannel)
