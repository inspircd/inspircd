/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017, 2019-2024 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2014-2015 Attila Molnar <attilamolnar@hush.com>
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


#pragma once

#include "inspircd.h"
#include "listmode.h"
#include "modules/exemption.h"
#include "modules/extban.h"

namespace Topic
{
	void ShowTopic(LocalUser* user, Channel* chan);
}

namespace Invite
{
	class APIImpl;
}

class CommandInvite final
	: public Command
{
private:
	Invite::APIImpl& invapi;

public:
	CommandInvite(Module* parent, Invite::APIImpl& invapiimpl);
	CmdResult Handle(User* user, const Params& parameters) override;
	RouteDescriptor GetRouting(User* user, const Params& parameters) override;
};

class CommandJoin final
	: public SplitCommand
{
public:
	CommandJoin(Module* parent);
	CmdResult HandleLocal(LocalUser* user, const Params& parameters) override;
};

class CommandTopic final
	: public SplitCommand
{
private:
	CheckExemption::EventProvider exemptionprov;
	ChanModeReference secretmode;
	ChanModeReference topiclockmode;

public:
	CommandTopic(Module* parent);
	CmdResult HandleLocal(LocalUser* user, const Params& parameters) override;
};

class CommandNames final
	: public SplitCommand
{
private:
	ChanModeReference secretmode;
	ChanModeReference privatemode;
	UserModeReference invisiblemode;
	Events::ModuleEventProvider namesevprov;

public:
	CommandNames(Module* parent);

	CmdResult HandleLocal(LocalUser* user, const Params& parameters) override;

	/** Spool the NAMES list for a given channel to the given user
	 * @param user User to spool the NAMES list to
	 * @param chan Channel whose nicklist to send
	 * @param show_invisible True to show invisible (+i) members to the user, false to omit them from the list
	 */
	void SendNames(LocalUser* user, Channel* chan, bool show_invisible);
};

class CommandKick final
	: public Command
{
public:
	CommandKick(Module* parent);
	CmdResult Handle(User* user, const Params& parameters) override;
	RouteDescriptor GetRouting(User* user, const Params& parameters) override;
};

class ModeChannelBan final
	: public ListModeBase
{
private:
	ExtBan::ManagerRef extbanmgr;
public:
	ModeChannelBan(Module* Creator);
	bool CompareEntry(const std::string& entry, const std::string& value) const override;
	bool ValidateParam(LocalUser* user, Channel* channel, std::string& parameter) override;
};

class ModeChannelKey final
	: public ParamMode<ModeChannelKey, StringExtItem>
{
public:
	std::string::size_type maxkeylen;
	ModeChannelKey(Module* Creator);
	bool OnModeChange(User* source, User* dest, Channel* channel, Modes::Change& change) override;
	void SerializeParam(Channel* chan, const std::string* key, std::string& out);
	bool OnSet(User* source, Channel* chan, std::string& param) override;
	bool IsParameterSecret() override;
};

class ModeChannelLimit final
	: public ParamMode<ModeChannelLimit, IntExtItem>
{
public:
	ModeChannelLimit(Module* Creator);
	bool ResolveModeConflict(const std::string& their_param, const std::string& our_param, Channel* channel) override;
	void SerializeParam(Channel* chan, intptr_t n, std::string& out);
	bool OnSet(User* source, Channel* channel, std::string& parameter) override;
};

class ModeChannelOp final
	: public PrefixMode
{
public:
	ModeChannelOp(Module* Creator);
};

class ModeChannelVoice final
	: public PrefixMode
{
public:
	ModeChannelVoice(Module* Creator);
};

class ExtBanManager final
	: public ExtBan::Manager
{
private:
	ModeChannelBan& banmode;
	Events::ModuleEventProvider evprov;
	LetterMap byletter;
	NameMap byname;

public:
	ExtBan::Format format;

	ExtBanManager(Module* Creator, ModeChannelBan& bm)
		: ExtBan::Manager(Creator)
		, banmode(bm)
		, evprov(Creator, "event/extban")
	{
	}

	void AddExtBan(ExtBan::Base* extban) override;
	bool Canonicalize(std::string& text) const override;
	ExtBan::Comparison CompareEntry(const ListModeBase* lm, const std::string& entry, const std::string& value) const override;
	void DelExtBan(ExtBan::Base* extban) override;
	ExtBan::Format GetFormat() const override { return format; }
	const LetterMap& GetLetterMap() const override { return byletter; }
	const NameMap& GetNameMap() const override { return byname; }
	ModResult GetStatus(ExtBan::ActingBase* extban, User* user, Channel* channel, const std::optional<ExtBan::MatchConfig>& config) const override;
	ExtBan::Base* FindName(const std::string& name) const override;
	ExtBan::Base* FindLetter(ExtBan::Letter letter) const override;
	void BuildISupport(std::string& out);
};
