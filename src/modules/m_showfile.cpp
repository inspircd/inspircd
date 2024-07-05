/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013-2014, 2016, 2018 Attila Molnar <attilamolnar@hush.com>
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
#include "clientprotocolmsg.h"

enum
{
	// From UnrealIRCd.
	RPL_RULES = 232,
	RPL_RULESTART = 308,
	RPL_RULESEND = 309,
	ERR_NORULES = 434
};

class CommandShowFile final
	: public Command
{
	enum Method
	{
		SF_MSG,
		SF_NOTICE,
		SF_NUMERIC
	};

	std::string introtext;
	std::string endtext;
	unsigned int intronumeric;
	unsigned int textnumeric;
	unsigned int endnumeric;
	std::vector<std::string> contents;
	Method method;

public:
	CommandShowFile(Module* parent, const std::string& cmdname)
		: Command(parent, cmdname)
	{
	}

	CmdResult Handle(User* user, const Params& parameters) override
	{
		if (method == SF_NUMERIC)
		{
			if (!introtext.empty() && intronumeric)
				user->WriteRemoteNumeric(intronumeric, introtext);

			for (const auto& line : contents)
				user->WriteRemoteNumeric(textnumeric, line);

			if (!endtext.empty() && endnumeric)
				user->WriteRemoteNumeric(endnumeric, endtext);
		}
		else if (IS_LOCAL(user))
		{
			LocalUser* const localuser = IS_LOCAL(user);
			for (const auto& line : contents)
			{
				ClientProtocol::Messages::Privmsg msg(ClientProtocol::Messages::Privmsg::nocopy, ServerInstance->FakeClient, localuser, line, ((method == SF_MSG) ? MessageType::PRIVMSG : MessageType::NOTICE));
				localuser->Send(ServerInstance->GetRFCEvents().privmsg, msg);
			}
		}
		return CmdResult::SUCCESS;
	}

	void UpdateSettings(const std::shared_ptr<ConfigTag>& tag, const std::string& filecontents)
	{
		introtext = tag->getString("introtext", "Showing " + name);
		endtext = tag->getString("endtext", "End of " + name);
		intronumeric = tag->getNum<unsigned int>("intronumeric", RPL_RULESTART, 0, 999);
		textnumeric = tag->getNum<unsigned int>("numeric", RPL_RULES, 0, 999);
		endnumeric = tag->getNum<unsigned int>("endnumeric", RPL_RULESEND, 0, 999);
		std::string smethod = tag->getString("method");

		method = SF_NUMERIC;
		if (smethod == "msg")
			method = SF_MSG;
		else if (smethod == "notice")
			method = SF_NOTICE;

		// Process the entry.
		contents.clear();

		irc::sepstream linestream(filecontents, '\n', true);
		for (std::string line; linestream.GetToken(line); )
		{
			// Some clients can not handle receiving NOTICE/PRIVMSG/RPL_RULES
			// with an empty trailing parameter so if a line is empty we
			// replace it with a single space.
			contents.push_back(line.empty() ? " " : line);
		}
		InspIRCd::ProcessColors(contents);
		contents.shrink_to_fit();
	}
};

class ModuleShowFile final
	: public Module
{
private:
	std::vector<CommandShowFile*> cmds;

	void ReadTag(const std::shared_ptr<ConfigTag>& tag, std::vector<CommandShowFile*>& newcmds)
	{
		std::string cmdname = tag->getString("name");
		if (cmdname.empty())
			throw ModuleException(this, "Empty value for 'name'");

		std::transform(cmdname.begin(), cmdname.end(), cmdname.begin(), ::toupper);

		const std::string file = tag->getString("file", cmdname);
		if (file.empty())
			throw ModuleException(this, "Empty value for 'file'");

		auto reader = ServerInstance->Config->ReadFile(file);
		if (!reader)
			throw ModuleException(this, "Unable to read " + file + ": " + reader.error);

		CommandShowFile* sfcmd;
		Command* handler = ServerInstance->Parser.GetHandler(cmdname);
		if (handler)
		{
			// Command exists, check if it is ours
			if (handler->creator != this)
				throw ModuleException(this, "Command " + cmdname + " already exists");

			// This is our command, make sure we don't have the same entry twice
			sfcmd = static_cast<CommandShowFile*>(handler);
			if (stdalgo::isin(newcmds, sfcmd))
				throw ModuleException(this, "Command " + cmdname + " is already used in a <showfile> tag");
		}
		else
		{
			// Command doesn't exist, create it
			sfcmd = new CommandShowFile(this, cmdname);
			ServerInstance->Modules.AddService(*sfcmd);
		}

		sfcmd->UpdateSettings(tag, reader.contents);
		newcmds.push_back(sfcmd);
	}

public:
	ModuleShowFile()
		: Module(VF_VENDOR, "Adds support for showing the contents of files to users when they execute a command.")
	{
	}

	void ReadConfig(ConfigStatus& status) override
	{
		std::vector<CommandShowFile*> newcmds;

		for (const auto& [_, tag] : ServerInstance->Config->ConfTags("showfile"))
		{
			try
			{
				ReadTag(tag, newcmds);
			}
			catch (const CoreException& ex)
			{
				ServerInstance->Logs.Warning(MODNAME, "Error: " + ex.GetReason() + " at " + tag->source.str());
			}
		}

		// Remove all commands that were removed from the config
		std::vector<CommandShowFile*> removed(cmds.size());
		std::sort(newcmds.begin(), newcmds.end());
		std::set_difference(cmds.begin(), cmds.end(), newcmds.begin(), newcmds.end(), removed.begin());

		stdalgo::delete_all(removed);
		cmds.swap(newcmds);
	}

	~ModuleShowFile() override
	{
		stdalgo::delete_all(cmds);
	}
};

MODULE_INIT(ModuleShowFile)
