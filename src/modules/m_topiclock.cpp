/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2012 Attila Molnar <attilamolnar@hush.com>
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

/* $ModDesc: Implements server-side topic locks and the server-to-server command SVSTOPIC */

#include "inspircd.h"

class CommandSVSTOPIC : public Command
{
 public:
	CommandSVSTOPIC(Module* Creator)
		: Command(Creator, "SVSTOPIC", 1, 4)
	{
		flags_needed = FLAG_SERVERONLY;
	}

	CmdResult Handle(const std::vector<std::string> &parameters, User *user)
	{
		if (!ServerInstance->ULine(user->server))
		{
			// Ulines only
			return CMD_FAILURE;
		}

		Channel* chan = ServerInstance->FindChan(parameters[0]);
		if (!chan)
			return CMD_FAILURE;

		if (parameters.size() == 4)
		{
			// 4 parameter version, set all topic data on the channel to the ones given in the parameters
			time_t topicts = ConvToInt(parameters[1]);
			if (!topicts)
			{
				ServerInstance->Logs->Log("m_topiclock", DEFAULT, "Received SVSTOPIC with a 0 topicts, dropped.");
				return CMD_INVALID;
			}

			std::string newtopic;
			newtopic.assign(parameters[3], 0, ServerInstance->Config->Limits.MaxTopic);
			bool topics_differ = (chan->topic != newtopic);
			if ((topics_differ) || (chan->topicset != topicts) || (chan->setby != parameters[2]))
			{
				// Update when any parameter differs
				chan->topicset = topicts;
				chan->setby.assign(parameters[2], 0, 127);
				chan->topic = newtopic;
				// Send TOPIC to clients only if the actual topic has changed, be silent otherwise
				if (topics_differ)
					chan->WriteChannel(user, "TOPIC %s :%s", chan->name.c_str(), chan->topic.c_str());
			}
		}
		else
		{
			// 1 parameter version, nuke the topic
			bool topic_empty = chan->topic.empty();
			if (!topic_empty || !chan->setby.empty())
			{
				chan->topicset = 0;
				chan->setby.clear();
				chan->topic.clear();
				if (!topic_empty)
					chan->WriteChannel(user, "TOPIC %s :", chan->name.c_str());
			}
		}

		return CMD_SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters)
	{
		return ROUTE_BROADCAST;
	}
};

class FlagExtItem : public ExtensionItem
{
 public:
	FlagExtItem(const std::string& key, Module* owner)
		: ExtensionItem(key, owner)
	{
	}

	virtual ~FlagExtItem()
	{
	}

	bool get(const Extensible* container) const
	{
		return (get_raw(container) != NULL);
	}

	std::string serialize(SerializeFormat format, const Extensible* container, void* item) const
	{
		if (format == FORMAT_USER)
			return "true";

		return "1";
	}

	void unserialize(SerializeFormat format, Extensible* container, const std::string& value)
	{
		if (value == "1")
			set_raw(container, this);
		else
			unset_raw(container);
	}

	void set(Extensible* container, bool value)
	{
		if (value)
			set_raw(container, this);
		else
			unset_raw(container);
	}

	void unset(Extensible* container)
	{
		unset_raw(container);
	}

	void free(void* item)
	{
		// nothing to free
	}
};

class ModuleTopicLock : public Module
{
	CommandSVSTOPIC cmd;
	FlagExtItem topiclock;

 public:
	ModuleTopicLock()
		: cmd(this), topiclock("topiclock", this)
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(cmd);
		ServerInstance->Modules->AddService(topiclock);
		ServerInstance->Modules->Attach(I_OnPreTopicChange, this);
	}

	ModResult OnPreTopicChange(User* user, Channel* chan, const std::string &topic)
	{
		// Only fired for local users currently, but added a check anyway
		if ((IS_LOCAL(user)) && (topiclock.get(chan)))
		{
			user->WriteNumeric(744, "%s :TOPIC cannot be changed due to topic lock being active on the channel", chan->name.c_str());
			return MOD_RES_DENY;
		}

		return MOD_RES_PASSTHRU;
	}

	Version GetVersion()
	{
		return Version("Implements server-side topic locks and the server-to-server command SVSTOPIC", VF_COMMON | VF_VENDOR);
	}
};

MODULE_INIT(ModuleTopicLock)
