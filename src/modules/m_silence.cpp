/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2006-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2005-2007 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2006 John Brooks <john.brooks@dereferenced.net>
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

/* $ModDesc: Provides support for the /SILENCE command */

/* Improved drop-in replacement for the /SILENCE command
 * syntax: /SILENCE [+|-]<mask> <p|c|i|n|t|a|x> as in <privatemessage|channelmessage|invites|privatenotice|channelnotice|all|exclude>
 *
 * example that blocks all except private messages
 *  /SILENCE +*!*@* a
 *  /SILENCE +*!*@* px
 *
 * example that blocks all invites except from channel services
 *  /SILENCE +*!*@* i
 *  /SILENCE +chanserv!services@chatters.net ix
 *
 * example that blocks some bad dude from private, notice and inviting you
 *  /SILENCE +*!kiddie@lamerz.net pin
 *
 * TODO: possibly have add and remove check for existing host and only modify flags according to
 *       what's been changed instead of having to remove first, then add if you want to change
 *       an entry.
 */

// pair of hostmask and flags
typedef std::pair<std::string, int> silenceset;

// deque list of pairs
typedef std::deque<silenceset> silencelist;

// intmasks for flags
static int SILENCE_PRIVATE	= 0x0001; /* p  private messages      */
static int SILENCE_CHANNEL	= 0x0002; /* c  channel messages      */
static int SILENCE_INVITE	= 0x0004; /* i  invites               */
static int SILENCE_NOTICE	= 0x0008; /* n  notices               */
static int SILENCE_CNOTICE	= 0x0010; /* t  channel notices       */
static int SILENCE_ALL		= 0x0020; /* a  all, (pcint)          */
static int SILENCE_EXCLUDE	= 0x0040; /* x  exclude this pattern  */


class CommandSVSSilence : public Command
{
 public:
	CommandSVSSilence(Module* Creator) : Command(Creator,"SVSSILENCE", 2)
	{
		syntax = "<target> {[+|-]<mask> <p|c|i|n|t|a|x>}";
		TRANSLATE4(TR_NICK, TR_TEXT, TR_TEXT, TR_END); /* we watch for a nick. not a UID. */
	}

	CmdResult Handle (const std::vector<std::string>& parameters, User *user)
	{
		/*
		 * XXX: thought occurs to me
		 * We may want to change the syntax of this command to
		 * SVSSILENCE <flagsora+> +<nick> -<nick> +<nick>
		 * style command so services can modify lots of entries at once.
		 * leaving it backwards compatible for now as it's late. -- w
		 */
		if (!ServerInstance->ULine(user->server))
			return CMD_FAILURE;

		User *u = ServerInstance->FindNick(parameters[0]);
		if (!u)
			return CMD_FAILURE;

		if (IS_LOCAL(u))
		{
			ServerInstance->Parser->CallHandler("SILENCE", std::vector<std::string>(parameters.begin() + 1, parameters.end()), u);
		}

		return CMD_SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters)
	{
		User* target = ServerInstance->FindNick(parameters[0]);
		if (target)
			return ROUTE_OPT_UCAST(target->server);
		return ROUTE_LOCALONLY;
	}
};

class CommandSilence : public Command
{
	unsigned int& maxsilence;
 public:
	SimpleExtItem<silencelist> ext;
	CommandSilence(Module* Creator, unsigned int &max) : Command(Creator, "SILENCE", 0),
		maxsilence(max), ext("silence_list", Creator)
	{
		allow_empty_last_param = false;
		syntax = "{[+|-]<mask> <p|c|i|n|t|a|x>}";
		TRANSLATE3(TR_TEXT, TR_TEXT, TR_END);
	}

	CmdResult Handle (const std::vector<std::string>& parameters, User *user)
	{
		if (!parameters.size())
		{
			// no parameters, show the current silence list.
			silencelist* sl = ext.get(user);
			// if the user has a silence list associated with their user record, show it
			if (sl)
			{
				for (silencelist::const_iterator c = sl->begin(); c != sl->end(); c++)
				{
					std::string decomppattern = DecompPattern(c->second);
					user->WriteNumeric(271, "%s %s %s %s",user->nick.c_str(), user->nick.c_str(),c->first.c_str(), decomppattern.c_str());
				}
			}
			user->WriteNumeric(272, "%s :End of Silence List",user->nick.c_str());

			return CMD_SUCCESS;
		}
		else if (parameters.size() > 0)
		{
			// one or more parameters, add or delete entry from the list (only the first parameter is used)
			std::string mask = parameters[0].substr(1);
			char action = parameters[0][0];
			// Default is private and notice so clients do not break
			int pattern = CompilePattern("pn");

			// if pattern supplied, use it
			if (parameters.size() > 1) {
				pattern = CompilePattern(parameters[1].c_str());
			}

			if (pattern == 0)
			{
				user->WriteServ("NOTICE %s :Bad SILENCE pattern",user->nick.c_str());
				return CMD_INVALID;
			}

			if (!mask.length())
			{
				// 'SILENCE +' or 'SILENCE -', assume *!*@*
				mask = "*!*@*";
			}

			ModeParser::CleanMask(mask);

			if (action == '-')
			{
				std::string decomppattern = DecompPattern(pattern);
				// fetch their silence list
				silencelist* sl = ext.get(user);
				// does it contain any entries and does it exist?
				if (sl)
				{
					for (silencelist::iterator i = sl->begin(); i != sl->end(); i++)
					{
						// search through for the item
						irc::string listitem = i->first.c_str();
						if (listitem == mask && i->second == pattern)
						{
							sl->erase(i);
							user->WriteNumeric(950, "%s %s :Removed %s %s from silence list",user->nick.c_str(), user->nick.c_str(), mask.c_str(), decomppattern.c_str());
							if (!sl->size())
							{
								ext.unset(user);
							}
							return CMD_SUCCESS;
						}
					}
				}
				user->WriteNumeric(952, "%s %s :%s %s does not exist on your silence list",user->nick.c_str(), user->nick.c_str(), mask.c_str(), decomppattern.c_str());
			}
			else if (action == '+')
			{
				// fetch the user's current silence list
				silencelist* sl = ext.get(user);
				if (!sl)
				{
					sl = new silencelist;
					ext.set(user, sl);
				}
				if (sl->size() > maxsilence)
				{
					user->WriteNumeric(952, "%s %s :Your silence list is full",user->nick.c_str(), user->nick.c_str());
					return CMD_FAILURE;
				}

				std::string decomppattern = DecompPattern(pattern);
				for (silencelist::iterator n = sl->begin(); n != sl->end();  n++)
				{
					irc::string listitem = n->first.c_str();
					if (listitem == mask && n->second == pattern)
					{
						user->WriteNumeric(952, "%s %s :%s %s is already on your silence list",user->nick.c_str(), user->nick.c_str(), mask.c_str(), decomppattern.c_str());
						return CMD_FAILURE;
					}
				}
				if (((pattern & SILENCE_EXCLUDE) > 0))
				{
					sl->push_front(silenceset(mask,pattern));
				}
				else
				{
					sl->push_back(silenceset(mask,pattern));
				}
				user->WriteNumeric(951, "%s %s :Added %s %s to silence list",user->nick.c_str(), user->nick.c_str(), mask.c_str(), decomppattern.c_str());
				return CMD_SUCCESS;
			}
		}
		return CMD_SUCCESS;
	}

	/* turn the nice human readable pattern into a mask */
	int CompilePattern(const char* pattern)
	{
		int p = 0;
		for (const char* n = pattern; *n; n++)
		{
			switch (*n)
			{
				case 'p':
					p |= SILENCE_PRIVATE;
					break;
				case 'c':
					p |= SILENCE_CHANNEL;
					break;
				case 'i':
					p |= SILENCE_INVITE;
					break;
				case 'n':
					p |= SILENCE_NOTICE;
					break;
				case 't':
					p |= SILENCE_CNOTICE;
					break;
				case 'a':
				case '*':
					p |= SILENCE_ALL;
					break;
				case 'x':
					p |= SILENCE_EXCLUDE;
					break;
				default:
					break;
			}
		}
		return p;
	}

	/* turn the mask into a nice human readable format */
	std::string DecompPattern (const int pattern)
	{
		std::string out;
		if (pattern & SILENCE_PRIVATE)
			out += ",privatemessages";
		if (pattern & SILENCE_CHANNEL)
			out += ",channelmessages";
		if (pattern & SILENCE_INVITE)
			out += ",invites";
		if (pattern & SILENCE_NOTICE)
			out += ",privatenotices";
		if (pattern & SILENCE_CNOTICE)
			out += ",channelnotices";
		if (pattern & SILENCE_ALL)
			out = ",all";
		if (pattern & SILENCE_EXCLUDE)
			out += ",exclude";
		if (out.length())
			return "<" + out.substr(1) + ">";
		else
			return "<none>";
	}

};

class ModuleSilence : public Module
{
	unsigned int maxsilence;
	CommandSilence cmdsilence;
	CommandSVSSilence cmdsvssilence;
 public:

	ModuleSilence()
		: maxsilence(32), cmdsilence(this, maxsilence), cmdsvssilence(this)
	{
	}

	void init()
	{
		OnRehash(NULL);
		ServerInstance->Modules->AddService(cmdsilence);
		ServerInstance->Modules->AddService(cmdsvssilence);
		ServerInstance->Modules->AddService(cmdsilence.ext);

		Implementation eventlist[] = { I_OnRehash, I_On005Numeric, I_OnUserPreNotice, I_OnUserPreMessage, I_OnUserPreInvite };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	void OnRehash(User* user)
	{
		maxsilence = ServerInstance->Config->ConfValue("silence")->getInt("maxentries", 32);
		if (!maxsilence)
			maxsilence = 32;
	}

	void On005Numeric(std::string &output)
	{
		// we don't really have a limit...
		output = output + " ESILENCE SILENCE=" + ConvToStr(maxsilence);
	}

	void OnBuildExemptList(MessageType message_type, Channel* chan, User* sender, char status, CUList &exempt_list, const std::string &text)
	{
		int public_silence = (message_type == MSG_PRIVMSG ? SILENCE_CHANNEL : SILENCE_CNOTICE);
		const UserMembList *ulist = chan->GetUsers();

		for (UserMembCIter i = ulist->begin(); i != ulist->end(); i++)
		{
			if (IS_LOCAL(i->first))
			{
				if (MatchPattern(i->first, sender, public_silence) == MOD_RES_DENY)
				{
					exempt_list.insert(i->first);
				}
			}
		}
	}

	ModResult PreText(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list, int silence_type)
	{
		if (target_type == TYPE_USER && IS_LOCAL(((User*)dest)))
		{
			return MatchPattern((User*)dest, user, silence_type);
		}
		else if (target_type == TYPE_CHANNEL)
		{
			Channel* chan = (Channel*)dest;
			if (chan)
			{
				this->OnBuildExemptList((silence_type == SILENCE_PRIVATE ? MSG_PRIVMSG : MSG_NOTICE), chan, user, status, exempt_list, "");
			}
		}
		return MOD_RES_PASSTHRU;
	}

	ModResult OnUserPreMessage(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		return PreText(user, dest, target_type, text, status, exempt_list, SILENCE_PRIVATE);
	}

	ModResult OnUserPreNotice(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		return PreText(user, dest, target_type, text, status, exempt_list, SILENCE_NOTICE);
	}

	ModResult OnUserPreInvite(User* source,User* dest,Channel* channel, time_t timeout)
	{
		return MatchPattern(dest, source, SILENCE_INVITE);
	}

	ModResult MatchPattern(User* dest, User* source, int pattern)
	{
		/* Server source */
		if (!source || !dest)
			return MOD_RES_ALLOW;

		silencelist* sl = cmdsilence.ext.get(dest);
		if (sl)
		{
			for (silencelist::const_iterator c = sl->begin(); c != sl->end(); c++)
			{
				if (((((c->second & pattern) > 0)) || ((c->second & SILENCE_ALL) > 0)) && (InspIRCd::Match(source->GetFullHost(), c->first)))
					return (c->second & SILENCE_EXCLUDE) ? MOD_RES_PASSTHRU : MOD_RES_DENY;
			}
		}
		return MOD_RES_PASSTHRU;
	}

	~ModuleSilence()
	{
	}

	Version GetVersion()
	{
		return Version("Provides support for the /SILENCE command", VF_OPTCOMMON | VF_VENDOR);
	}
};

MODULE_INIT(ModuleSilence)
