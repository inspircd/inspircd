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

/* $ModDesc: Provides support for ircu style usermode +d (deaf to channel messages and channel notices) */

/** User mode +d - filter out channel messages and channel notices
 */
class User_d : public ModeHandler
{
 public:
	User_d(InspIRCd* Instance, Module* Creator) : ModeHandler(Instance, Creator, 'd', 0, 0, false, MODETYPE_USER, false) { }

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
	{
		if (adding)
		{
			if (!dest->IsModeSet('d'))
			{
				dest->WriteServ("NOTICE %s :*** You have enabled usermode +d, deaf mode. This mode means you WILL NOT receive any messages from any channels you are in. If you did NOT mean to do this, use /mode %s -d.", dest->nick.c_str(), dest->nick.c_str());
				dest->SetMode('d',true);
				return MODEACTION_ALLOW;
			}
		}
		else
		{
			if (dest->IsModeSet('d'))
			{
				dest->SetMode('d',false);
				return MODEACTION_ALLOW;
			}
		}
		return MODEACTION_DENY;
	}
};

class ModuleDeaf : public Module
{
	User_d m1;

	std::string deaf_bypasschars;
	std::string deaf_bypasschars_uline;

 public:
	ModuleDeaf(InspIRCd* Me)
		: Module(Me), m1(Me, this)
	{
		if (!ServerInstance->Modes->AddMode(&m1))
			throw ModuleException("Could not add new modes!");

		OnRehash(NULL);
		Implementation eventlist[] = { I_OnUserPreMessage, I_OnUserPreNotice, I_OnRehash, I_OnBuildExemptList };
		ServerInstance->Modules->Attach(eventlist, this, 4);
	}


	virtual void OnRehash(User* user)
	{
		ConfigReader* conf = new ConfigReader(ServerInstance);
		deaf_bypasschars = conf->ReadValue("deaf", "bypasschars", 0);
		deaf_bypasschars_uline = conf->ReadValue("deaf", "bypasscharsuline", 0);

		delete conf;
	}

	virtual ModResult OnUserPreNotice(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		if (target_type == TYPE_CHANNEL)
		{
			Channel* chan = (Channel*)dest;
			if (chan)
				this->BuildDeafList(MSG_NOTICE, chan, user, status, text, exempt_list);
		}

		return MOD_RES_PASSTHRU;
	}

	virtual ModResult OnUserPreMessage(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		if (target_type == TYPE_CHANNEL)
		{
			Channel* chan = (Channel*)dest;
			if (chan)
				this->BuildDeafList(MSG_PRIVMSG, chan, user, status, text, exempt_list);
		}

		return MOD_RES_PASSTHRU;
	}

	virtual void OnBuildExemptList(MessageType message_type, Channel* chan, User* sender, char status, CUList &exempt_list, const std::string &text)
	{
		BuildDeafList(message_type, chan, sender, status, text, exempt_list);
	}

	virtual void BuildDeafList(MessageType message_type, Channel* chan, User* sender, char status, const std::string &text, CUList &exempt_list)
	{
		CUList *ulist = chan->GetUsers();
		bool is_a_uline;
		bool is_bypasschar, is_bypasschar_avail;
		bool is_bypasschar_uline, is_bypasschar_uline_avail;

		is_bypasschar = is_bypasschar_avail = is_bypasschar_uline = is_bypasschar_uline_avail = 0;
		if (!deaf_bypasschars.empty())
		{
			is_bypasschar_avail = 1;
			if (deaf_bypasschars.find(text[0], 0) != std::string::npos)
				is_bypasschar = 1;
		}
		if (!deaf_bypasschars_uline.empty())
		{
			is_bypasschar_uline_avail = 1;
			if (deaf_bypasschars_uline.find(text[0], 0) != std::string::npos)
				is_bypasschar_uline = 1;
		}

		/*
		 * If we have no bypasschars_uline in config, and this is a bypasschar (regular)
		 * Than it is obviously going to get through +d, no build required
		 */
		if (!is_bypasschar_uline_avail && is_bypasschar)
			return;

		for (CUList::iterator i = ulist->begin(); i != ulist->end(); i++)
		{
			/* not +d ? */
			if (!i->first->IsModeSet('d'))
				continue; /* deliver message */
			/* matched both U-line only and regular bypasses */
			if (is_bypasschar && is_bypasschar_uline)
				continue; /* deliver message */

			is_a_uline = ServerInstance->ULine(i->first->server);
			/* matched a U-line only bypass */
			if (is_bypasschar_uline && is_a_uline)
				continue; /* deliver message */
			/* matched a regular bypass */
			if (is_bypasschar && !is_a_uline)
				continue; /* deliver message */

			if (status && !strchr(chan->GetAllPrefixChars(i->first), status))
				continue;

			/* don't deliver message! */
			exempt_list[i->first] = i->first->nick;
		}
	}

	virtual ~ModuleDeaf()
	{
		ServerInstance->Modes->DelMode(&m1);
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_COMMON|VF_VENDOR,API_VERSION);
	}

};

MODULE_INIT(ModuleDeaf)
