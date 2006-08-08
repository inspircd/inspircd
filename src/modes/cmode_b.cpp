#include <string>
#include <vector>
#include "inspircd_config.h"
#include "configreader.h"
#include "hash_map.h"
#include "inspircd.h"
#include "mode.h"
#include "channels.h"
#include "users.h"
#include "helperfuncs.h"
#include "message.h"
#include "modules.h"
#include "inspstring.h"
#include "hashcomp.h"
#include "modes/cmode_b.h"

extern InspIRCd* ServerInstance;
extern ServerConfig* Config;
extern std::vector<Module*> modules;
extern std::vector<ircd_module*> factory;
extern int MODCOUNT;
extern time_t TIME;

ModeChannelBan::ModeChannelBan() : ModeHandler('b', 1, 1, true, MODETYPE_CHANNEL, false)
{
}

ModeAction ModeChannelBan::OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding)
{
	int status = cstatus(source, channel);
	/* Call the correct method depending on wether we're adding or removing the mode */
	if (adding)
	{
		parameter = this->AddBan(source, parameter, channel, status);
	}
	else
	{
		parameter = this->DelBan(source, parameter, channel, status);
	}
	/* If the method above 'ate' the parameter by reducing it to an empty string, then
	 * it won't matter wether we return ALLOW or DENY here, as an empty string overrides
	 * the return value and is always MODEACTION_DENY if the mode is supposed to have
	 * a parameter.
	 */
	return MODEACTION_ALLOW;
}

void ModeChannelBan::DisplayList(userrec* user, chanrec* channel)
{
	/* Display the channel banlist */
	for (BanList::iterator i = channel->bans.begin(); i != channel->bans.end(); i++)
	{
		user->WriteServ("367 %s %s %s %s %d",user->nick, channel->name, i->data, i->set_by, i->set_time);
	}
	user->WriteServ("368 %s %s :End of channel ban list",user->nick, channel->name);
	return;
}

std::string& ModeChannelBan::AddBan(userrec *user,std::string &dest,chanrec *chan,int status)
{
	if ((!user) || (!chan))
	{
		log(DEFAULT,"*** BUG *** AddBan was given an invalid parameter");
		dest = "";
		return dest;
	}

	/* Attempt to tidy the mask */
	ModeParser::CleanMask(dest);
	/* If the mask was invalid, we exit */
	if (dest == "")
		return dest;

	long maxbans = GetMaxBans(chan->name);
	if ((unsigned)chan->bans.size() > (unsigned)maxbans)
	{
		user->WriteServ("478 %s %s :Channel ban list for %s is full (maximum entries for this channel is %d)",user->nick, chan->name,chan->name,maxbans);
		dest = "";
		return dest;
	}

	int MOD_RESULT = 0;
	FOREACH_RESULT(I_OnAddBan,OnAddBan(user,chan,dest));
	if (MOD_RESULT)
	{
		dest = "";
		return dest;
	}

	for (BanList::iterator i = chan->bans.begin(); i != chan->bans.end(); i++)
	{
		if (!strcasecmp(i->data,dest.c_str()))
		{
			/* dont allow a user to set the same ban twice */
			dest = "";
			return dest;
		}
	}

	b.set_time = TIME;
	strlcpy(b.data,dest.c_str(),MAXBUF);
	if (*user->nick)
	{
		strlcpy(b.set_by,user->nick,NICKMAX-1);
	}
	else
	{
		strlcpy(b.set_by,Config->ServerName,NICKMAX-1);
	}
	chan->bans.push_back(b);
	return dest;
}

ModePair ModeChannelBan::ModeSet(userrec* source, userrec* dest, chanrec* channel, const std::string &parameter)
{
	for (BanList::iterator i = channel->bans.begin(); i != channel->bans.end(); i++)
	{
		if (!strcasecmp(i->data,parameter.c_str()))
		{
			return std::make_pair(true, i->data);
		}
	}
        return std::make_pair(false, parameter);
}

std::string& ModeChannelBan::DelBan(userrec *user,std::string& dest,chanrec *chan,int status)
{
	if ((!user) || (!chan))
	{
		log(DEFAULT,"*** BUG *** TakeBan was given an invalid parameter");
		dest = "";
		return dest;
	}

	/* 'Clean' the mask, e.g. nick -> nick!*@* */
	ModeParser::CleanMask(dest);

	for (BanList::iterator i = chan->bans.begin(); i != chan->bans.end(); i++)
	{
		if (!strcasecmp(i->data,dest.c_str()))
		{
			int MOD_RESULT = 0;
			FOREACH_RESULT(I_OnDelBan,OnDelBan(user,chan,dest));
			if (MOD_RESULT)
			{
				dest = "";
				return dest;
			}
			chan->bans.erase(i);
			return dest;
		}
	}
	dest = "";
	return dest;
}

