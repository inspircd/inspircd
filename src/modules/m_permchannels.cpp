/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2011 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"

/* $ModDesc: Provides support for channel mode +P to provide permanent channels */

/** Handles the +P channel mode
 */
class PermChannel : public ModeHandler
{
 public:
	PermChannel(Module* Creator) : ModeHandler(Creator, "permanent", 'P', PARAM_NONE, MODETYPE_CHANNEL)
	{
		oper = true;
		fixed_letter = false;
	}

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
	{
		if (adding)
		{
			if (!channel->IsModeSet(this))
			{
				channel->SetMode(this,true);
				return MODEACTION_ALLOW;
			}
		}
		else
		{
			if (channel->IsModeSet(this))
			{
				channel->SetMode(this,false);
				if (channel->GetUserCounter() == 0)
				{
					channel->DelUser(ServerInstance->FakeClient);
				}
				return MODEACTION_ALLOW;
			}
		}

		return MODEACTION_DENY;
	}
};

class ModulePermanentChannels : public Module
{
	PermChannel p;

	void LoadPermChannels()
	{
		ConfigTagList permchannels = ServerInstance->Config->GetTags("permchannels");
		for (ConfigIter i = permchannels.first; i != permchannels.second; ++i)
		{
			ConfigTag* tag = i->second;
			std::string channel = tag->getString("channel");
			std::string topic = tag->getString("topic");
			std::string modes = tag->getString("modes");
			std::string longmodes = tag->getString("modelist");

			if (channel.empty())
			{
				ServerInstance->Logs->Log("m_permchannels", ERROR, "Malformed permchannels tag at " +
					tag->getTagLocation());
				continue;
			}

			Channel *c = ServerInstance->FindChan(channel);

			if (!c)
			{
				c = new Channel(channel, tag->getInt("ts", ServerInstance->Time()));
				if (!topic.empty())
				{
					c->SetTopic(NULL, topic, true);
					// topic was set at channel creation; it'll be overwritten if it was ever
					// changed
					c->topicset = c->age;
				}
				ServerInstance->Logs->Log("m_permchannels", DEBUG, "Added %s with topic %s", channel.c_str(), topic.c_str());

				if (modes.empty() && longmodes.empty())
					continue;

				Extensible* target;
				irc::modestacker modestack;
				irc::spacesepstream list(modes);
				irc::spacesepstream longlist(longmodes);

				std::vector<std::string> seq;
				seq.push_back(c->name);
				std::string token;
				while (list.GetToken(token))
					seq.push_back(token);
				ServerInstance->Modes->Parse(seq, ServerInstance->FakeClient, target, modestack);

				while (longlist.GetToken(token))
				{
					std::string name, value;

					std::string::size_type eq = token.find('=');
					if (eq == std::string::npos)
						name = token;
					else
					{
						name = token.substr(0, eq);
						value = token.substr(eq + 1);
					}
					ModeHandler *mh = ServerInstance->Modes->FindMode(name);
					if (mh && mh->GetModeType() == MODETYPE_CHANNEL)
					{
						modestack.push(irc::modechange(mh->id, value, true));
					}
				}
				ServerInstance->Modes->Process(ServerInstance->FakeClient, target, modestack);
			}
		}
	}

public:

	ModulePermanentChannels() : p(this)
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(p);
		ServerInstance->Modules->Attach(I_OnChannelPreDelete, this);
		LoadPermChannels();
	}

	CullResult cull()
	{
		ServerInstance->Modes->DelMode(&p);
		return Module::cull();
	}

	void ReadConfig(ConfigReadStatus& status)
	{
		if(status.reason == REHASH_NEWCONF)
			LoadPermChannels();
	}

	virtual Version GetVersion()
	{
		return Version("Provides support for channel mode +P to provide permanent channels",VF_VENDOR);
	}

	virtual ModResult OnChannelPreDelete(Channel *c)
	{
		if (c->IsModeSet(&p))
			return MOD_RES_DENY;

		return MOD_RES_PASSTHRU;
	}
};

MODULE_INIT(ModulePermanentChannels)
