/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2008 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
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

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding, bool)
	{
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
				channel->SetMode('P',false);

				if (channel->GetUserCounter() == 0)
					delete channel;
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

		OnRehash(NULL, "");
	}

	virtual ~ModulePermanentChannels()
	{
		ServerInstance->Modes->DelMode(p);
		delete p;
	}

	virtual void OnRehash(User *user, const std::string &parameter)
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

			if (channel.empty() || topic.empty())
			{
				ServerInstance->Logs->Log("blah", DEBUG, "Malformed permchannels tag with empty topic or channel name.");
				continue;
			}

			Channel *c = ServerInstance->FindChan(channel);

			if (!c)
			{
				c = new Channel(ServerInstance, channel, ServerInstance->Time());
				c->SetTopic(NULL, topic, true);
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
		return Version(1,2,0,0,VF_COMMON|VF_VENDOR,API_VERSION);
	}

	virtual int OnChannelPreDelete(Channel *c)
	{
		if (c->IsModeSet('P'))
			return 1;

		return 0;
	}
};

MODULE_INIT(ModulePermanentChannels)
