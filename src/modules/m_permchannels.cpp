/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
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
	PermChannel(InspIRCd* Instance) : ModeHandler(Instance, 'P', 0, 0, false, MODETYPE_CHANNEL, false) { }

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding, bool sm)
	{
		if (!source->HasPrivPermission("channels/set-permanent"))
		{
			source->WriteNumeric(ERR_NOPRIVILEGES, "%s :Permission Denied - You do not have the required operator privileges", source->nick.c_str());
			return MODEACTION_DENY;
		}

		if (adding)
		{
			if (!channel->IsModeSet('P'))
			{
				channel->SetMode('P',true);
				return MODEACTION_ALLOW;
			}
		}
		else
		{
			if (channel->IsModeSet('P'))
			{
				if (channel->GetUserCounter() == 0 && !sm)
				{
					/*
					 * ugh, ugh, UGH!
					 *
					 * We can't delete this channel the way things work at the moment,
					 * because of the following scenario:
					 * s1:#c <-> s2:#c
					 *
					 * s1 has a user in #c, s2 does not. s2 has +P set. s2 has a losing TS.
					 *
					 * On netmerge, s2 loses, so s2 removes all modes (including +P) which
					 * would subsequently delete the channel here causing big fucking problems.
					 *
					 * I don't think there's really a way around this, so just deny -P on a 0 user chan.
					 * -- w00t
					 *
					 * delete channel;
					 */
					return MODEACTION_DENY;
				}

				/* for servers, remove +P (to avoid desyncs) but don't bother trying to delete. */
				channel->SetMode('P',false);
				return MODEACTION_ALLOW;
			}
		}

		return MODEACTION_DENY;
	}
};

class ModulePermanentChannels : public Module
{
	PermChannel *p;
public:

	ModulePermanentChannels(InspIRCd* Me) : Module(Me)
	{
		p = new PermChannel(ServerInstance);
		if (!ServerInstance->Modes->AddMode(p))
		{
			delete p;
			throw ModuleException("Could not add new modes!");
		}
		Implementation eventlist[] = { I_OnChannelPreDelete };
		ServerInstance->Modules->Attach(eventlist, this, 1);

		OnRehash(NULL);
	}

	virtual ~ModulePermanentChannels()
	{
		ServerInstance->Modes->DelMode(p);
		delete p;
	}

	virtual void OnRehash(User *user)
	{
		/*
		 * Process config-defined list of permanent channels.
		 * -- w00t
		 */
		ConfigReader MyConf(ServerInstance);
		for (int i = 0; i < MyConf.Enumerate("permchannels"); i++)
		{
			std::string channel = MyConf.ReadValue("permchannels", "channel", i);
			std::string topic = MyConf.ReadValue("permchannels", "topic", i);
			std::string modes = MyConf.ReadValue("permchannels", "modes", i);

			if (channel.empty())
			{
				ServerInstance->Logs->Log("blah", DEBUG, "Malformed permchannels tag with empty channel name.");
				continue;
			}

			Channel *c = ServerInstance->FindChan(channel);

			if (!c)
			{
				c = new Channel(ServerInstance, channel, ServerInstance->Time());
				if (!topic.empty())
				{
					c->SetTopic(NULL, topic, true);

					/*
					 * Due to the way protocol works in 1.2, we need to hack the topic TS in such a way that this
					 * topic will always win over others.
					 *
					 * This is scheduled for (proper) fixing in a later release, and can be removed at a later date.
					 */
					c->topicset = 42;
				}
				ServerInstance->Logs->Log("blah", DEBUG, "Added %s with topic %s", channel.c_str(), topic.c_str());

				if (modes.empty())
					continue;

				irc::spacesepstream list(modes);
				std::string modeseq;
				std::string par;

				list.GetToken(modeseq);

				// XXX bleh, should we pass this to the mode parser instead? ugly. --w00t
				for (std::string::iterator n = modeseq.begin(); n != modeseq.end(); ++n)
				{
					ModeHandler* mode = ServerInstance->Modes->FindMode(*n, MODETYPE_CHANNEL);
					if (mode)
					{
						if (mode->GetNumParams(true))
							list.GetToken(par);
						else
							par.clear();

						mode->OnModeChange(ServerInstance->FakeClient, ServerInstance->FakeClient, c, par, true);
					}
				}
			}
		}
	}

	virtual Version GetVersion()
	{
		return Version("$Id$",VF_COMMON|VF_VENDOR,API_VERSION);
	}

	virtual int OnChannelPreDelete(Channel *c)
	{
		if (c->IsModeSet('P'))
			return 1;

		return 0;
	}
};

MODULE_INIT(ModulePermanentChannels)
