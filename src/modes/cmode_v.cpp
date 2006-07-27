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
#include "commands.h"
#include "modules.h"
#include "inspstring.h"
#include "hashcomp.h"
#include "modes/cmode_v.h"

extern InspIRCd* ServerInstance;
extern ServerConfig* Config;
extern std::vector<Module*> modules;
extern std::vector<ircd_module*> factory;
extern int MODCOUNT;
extern time_t TIME;

ModeChannelVoice::ModeChannelVoice() : ModeHandler('v', 1, 1, true, MODETYPE_CHANNEL, false)
{
}

ModePair ModeChannelVoice::ModeSet(userrec* source, userrec* dest, chanrec* channel, const std::string &parameter)
{
        userrec* x = Find(parameter);
        if (x)
        {
                if (cstatus(x, channel) == STATUS_VOICE)
                {
                        return std::make_pair(true, x->nick);
                }
                else
                {
                        return std::make_pair(false, parameter);
                }
        }
	return std::make_pair(false, parameter);
}

ModeAction ModeChannelVoice::OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding)
{
	int status = cstatus(source, channel);

	/* Call the correct method depending on wether we're adding or removing the mode */
	if (adding)
	{
		parameter = this->AddVoice(source, parameter.c_str(), channel, status);
	}
	else
	{
		parameter = this->DelVoice(source, parameter.c_str(), channel, status);
	}
	/* If the method above 'ate' the parameter by reducing it to an empty string, then
	 * it won't matter wether we return ALLOW or DENY here, as an empty string overrides
	 * the return value and is always MODEACTION_DENY if the mode is supposed to have
	 * a parameter.
	 */
	return MODEACTION_ALLOW;
}

std::string ModeChannelVoice::AddVoice(userrec *user,const char* dest,chanrec *chan,int status)
{
	userrec *d = ModeParser::SanityChecks(user,dest,chan,status);

	if (d)
	{
		if (IS_LOCAL(user))
		{
			int MOD_RESULT = 0;
			FOREACH_RESULT(I_OnAccessCheck,OnAccessCheck(user,d,chan,AC_VOICE));

			if (MOD_RESULT == ACR_DENY)
				return "";
			if (MOD_RESULT == ACR_DEFAULT)
			{
				if ((status < STATUS_HOP) && (!is_uline(user->server)))
				{
					WriteServ(user->fd,"482 %s %s :You're not a channel (half)operator",user->nick, chan->name);
					return "";
				}
			}
		}

		return ModeParser::Grant(d,chan,UCMODE_VOICE);
	}
	return "";
}

std::string ModeChannelVoice::DelVoice(userrec *user,const char *dest,chanrec *chan,int status)
{
	userrec *d = ModeParser::SanityChecks(user,dest,chan,status);

	if (d)
	{
		if (IS_LOCAL(user))
		{
			int MOD_RESULT = 0;
			FOREACH_RESULT(I_OnAccessCheck,OnAccessCheck(user,d,chan,AC_DEVOICE));

			if (MOD_RESULT == ACR_DENY)
				return "";
			if (MOD_RESULT == ACR_DEFAULT)
			{
				if ((status < STATUS_HOP) && (!is_uline(user->server)))
				{
					WriteServ(user->fd,"482 %s %s :You are not a channel (half)operator",user->nick, chan->name);
					return "";
				}
			}
		}

		return ModeParser::Revoke(d,chan,UCMODE_VOICE);
	}
	return "";
}
