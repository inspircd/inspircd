/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
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

static void fputesc(FILE* f, const std::string& text)
{
	for(std::string::size_type i=0; i < text.length(); i++)
	{
		if (text[i] == '\\' || text[i] == '"')
			fputc('\\', f);
		fputc(text[i], f);
	}
}

static bool WriteDatabase(ModeHandler* p)
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
		if (!chan->IsModeSet(p))
			continue;

		fputs("<permchannels channel=\"", f);
		fputesc(f, chan->name);
		fputs("\" topic=\"", f);
		fputesc(f, chan->topic);
		fputs("\" modes=\"", f);
		irc::modestacker cmodes;
		chan->ChanModes(cmodes, MODELIST_FULL);
		fputesc(f, cmodes.popModeLine(true, INT_MAX, INT_MAX));
		fputs("\">\n", f);
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
	bool dirty;
public:

	ModulePermanentChannels() : p(this), dirty(false)
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(p);
		Implementation eventlist[] = { I_OnChannelPreDelete, I_OnPostTopicChange, I_OnMode, I_OnRehash, I_OnBackgroundTimer };
		ServerInstance->Modules->Attach(eventlist, this, 5);

		OnRehash(NULL);
	}

	CullResult cull()
	{
		ServerInstance->Modes->DelMode(&p);
		return Module::cull();
	}

	virtual void OnRehash(User *user)
	{
		/*
		 * Process config-defined list of permanent channels.
		 * -- w00t
		 */

		permchannelsconf = ServerInstance->Config->ConfValue("permchanneldb")->getString("filename");

		ConfigTagList permchannels = ServerInstance->Config->ConfTags("permchannels");
		for (ConfigIter i = permchannels.first; i != permchannels.second; ++i)
		{
			ConfigTag* tag = i->second;
			std::string channel = tag->getString("channel");
			std::string topic = tag->getString("topic");
			std::string modes = tag->getString("modes");

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
				std::vector<std::string> seq;
				seq.push_back(c->name);
				std::string token;
				while (list.GetToken(token))
					seq.push_back(token);

				ServerInstance->SendMode(seq, ServerInstance->FakeClient);
			}
		}
	}

	void OnMode(User*, Extensible* dest, const irc::modestacker&)
	{
		Channel* chan = dynamic_cast<Channel*>(dest);
		if (chan && chan->IsModeSet(&p))
			dirty = true;
	}

	void OnPostTopicChange(User*, Channel *c, const std::string&)
	{
		if (c->IsModeSet(&p))
			dirty = true;
	}

	void OnBackgroundTimer(time_t)
	{
		if (dirty)
			WriteDatabase(&p);
		dirty = false;
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
