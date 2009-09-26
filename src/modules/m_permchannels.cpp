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

// Not in a class due to circular dependancy hell.
static std::string permchannelsconf;
static bool WriteDatabase()
{
	FILE *f;

	if (permchannelsconf.empty())
	{
		// Fake success.
		return true;
	}

	std::string tempname = permchannelsconf + ".tmp";

	/*
	 * We need to perform an atomic write so as not to fuck things up.
	 * So, let's write to a temporary file, flush and sync the FD, then rename the file..
	 *		-- w00t
	 */
	f = fopen(tempname.c_str(), "w");
	if (!f)
	{
		ServerInstance->Logs->Log("m_permchannels",DEFAULT, "permchannels: Cannot create database! %s (%d)", strerror(errno), errno);
		ServerInstance->SNO->WriteToSnoMask('a', "database: cannot create new db: %s (%d)", strerror(errno), errno);
		return false;
	}

	// Now, let's write.
	for (chan_hash::const_iterator i = ServerInstance->chanlist->begin(); i != ServerInstance->chanlist->end(); i++)
	{
		Channel* chan = i->second;
		if (!chan->IsModeSet('P'))
			continue;

		char line[1024];
		const char* items[] =
		{
			"<permchannels channel=",
			chan->name.c_str(),
			" topic=",
			chan->topic.c_str(),
			" modes=",
			chan->ChanModes(true),
			">\n"
		};

		int lpos = 0, item = 0, ipos = 0;
		while (lpos < 1022 && item < 7)
		{
			char c = items[item][ipos++];
			if (c == 0)
			{
				// end of this string; hop to next string, insert a quote
				item++;
				ipos = 0;
				c = '"';
			}
			else if (c == '\\' || c == '"')
			{
				line[lpos++] = '\\';
			}
			line[lpos++] = c;
		}
		line[--lpos] = 0;
		fputs(line, f);
	}

	int write_error = 0;
	write_error = ferror(f);
	write_error |= fclose(f);
	if (write_error)
	{
		ServerInstance->Logs->Log("m_permchannels",DEFAULT, "permchannels: Cannot write to new database! %s (%d)", strerror(errno), errno);
		ServerInstance->SNO->WriteToSnoMask('a', "database: cannot write to new db: %s (%d)", strerror(errno), errno);
		return false;
	}

	// Use rename to move temporary to new db - this is guarenteed not to fuck up, even in case of a crash.
	if (rename(tempname.c_str(), permchannelsconf.c_str()) < 0)
	{
		ServerInstance->Logs->Log("m_permchannels",DEFAULT, "permchannels: Cannot move new to old database! %s (%d)", strerror(errno), errno);
		ServerInstance->SNO->WriteToSnoMask('a', "database: cannot replace old with new db: %s (%d)", strerror(errno), errno);
		return false;
	}

	return true;
}



/** Handles the +P channel mode
 */
class PermChannel : public ModeHandler
{
 public:
	PermChannel(Module* Creator) : ModeHandler(Creator, 'P', PARAM_NONE, MODETYPE_CHANNEL) { }

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
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

				// Save permchannels db if needed.
				WriteDatabase();
				return MODEACTION_ALLOW;
			}
		}
		else
		{
			if (channel->IsModeSet('P'))
			{
				if (channel->GetUserCounter() == 0 && !IS_FAKE(source))
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

				// Save permchannels db if needed.
				WriteDatabase();
				return MODEACTION_ALLOW;
			}
		}

		return MODEACTION_DENY;
	}
};

class ModulePermanentChannels : public Module
{
	PermChannel p;
public:

	ModulePermanentChannels() : p(this)
	{
		if (!ServerInstance->Modes->AddMode(&p))
			throw ModuleException("Could not add new modes!");
		Implementation eventlist[] = { I_OnChannelPreDelete, I_OnPostTopicChange, I_OnRawMode };
		ServerInstance->Modules->Attach(eventlist, this, 3);

		OnRehash(NULL);
	}

	virtual ~ModulePermanentChannels()
	{
		ServerInstance->Modes->DelMode(&p);
		/*
		 * DelMode can't remove the +P mode on empty channels, or it will break
		 * merging modes with remote servers. Remove the empty channels now as
		 * we know this is not the case.
		 */
		chan_hash::iterator iter = ServerInstance->chanlist->begin();
		while (iter != ServerInstance->chanlist->end())
		{
			Channel* c = iter->second;
			if (c->GetUserCounter() == 0)
			{
				chan_hash::iterator at = iter;
				iter++;
				ServerInstance->chanlist->erase(at);
				delete c;
			}
			else
				iter++;
		}
	}

	virtual void OnRehash(User *user)
	{
		/*
		 * Process config-defined list of permanent channels.
		 * -- w00t
		 */
		ConfigReader MyConf;

		permchannelsconf = MyConf.ReadValue("permchanneldb", "filename", "", 0, false);

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
				c = new Channel(channel, ServerInstance->Time());
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

	virtual ModResult OnRawMode(User* user, Channel* chan, const char mode, const std::string &param, bool adding, int pcnt)
	{
		if (chan && chan->IsModeSet('P'))
			WriteDatabase();

		return MOD_RES_PASSTHRU;
	}

	virtual void OnPostTopicChange(User*, Channel *c, const std::string&)
	{
		if (c->IsModeSet('P'))
			WriteDatabase();
	}

	virtual Version GetVersion()
	{
		return Version("Provides support for channel mode +P to provide permanent channels",VF_COMMON|VF_VENDOR,API_VERSION);
	}

	virtual ModResult OnChannelPreDelete(Channel *c)
	{
		if (c->IsModeSet('P'))
			return MOD_RES_DENY;

		return MOD_RES_PASSTHRU;
	}
};

MODULE_INIT(ModulePermanentChannels)
