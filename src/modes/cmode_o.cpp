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
#include "modes/cmode_o.h"

extern InspIRCd* ServerInstance;
extern ServerConfig* Config;
extern std::vector<Module*> modules;
extern std::vector<ircd_module*> factory;
extern int MODCOUNT;
extern time_t TIME;

ModeChannelOp::ModeChannelOp() : ModeHandler('o', 1, 1, true, MODETYPE_CHANNEL, false)
{
}

ModePair ModeChannelOp::ModeSet(userrec* source, userrec* dest, chanrec* channel, const std::string &parameter)
{
        userrec* x = Find(parameter);
        if (x)
        {
                if (cstatus(x, channel) == STATUS_OP)
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

ModeAction ModeChannelOp::OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding)
{
	int status = cstatus(source, channel);

	/* Call the correct method depending on wether we're adding or removing the mode */
	if (adding)
	{
		parameter = this->AddOp(source, parameter.c_str(), channel, status);
	}
	else
	{
		parameter = this->DelOp(source, parameter.c_str(), channel, status);
	}
	/* If the method above 'ate' the parameter by reducing it to an empty string, then
	 * it won't matter wether we return ALLOW or DENY here, as an empty string overrides
	 * the return value and is always MODEACTION_DENY if the mode is supposed to have
	 * a parameter.
	 */
	return MODEACTION_ALLOW;
}

std::string ModeChannelOp::AddOp(userrec *user,const char* dest,chanrec *chan,int status)
{
	userrec *d = ModeParser::SanityChecks(user,dest,chan,status);

	if (d)
	{
		if (IS_LOCAL(user))
		{
			int MOD_RESULT = 0;
			FOREACH_RESULT(I_OnAccessCheck,OnAccessCheck(user,d,chan,AC_OP));

			if (MOD_RESULT == ACR_DENY)
				return "";
			if (MOD_RESULT == ACR_DEFAULT)
			{
				if ((status < STATUS_OP) && (!is_uline(user->server)))
				{
					WriteServ(user->fd,"482 %s %s :You're not a channel operator",user->nick, chan->name);
					return "";
				}
			}
		}

		return ModeParser::Grant(d,chan,UCMODE_OP);
	}
	return "";
}

std::string ModeChannelOp::DelOp(userrec *user,const char *dest,chanrec *chan,int status)
{
	userrec *d = ModeParser::SanityChecks(user,dest,chan,status);

	if (d)
	{
		if (IS_LOCAL(user))
		{
			int MOD_RESULT = 0;
			log(DEBUG,"Call OnAccessCheck for AC_DEOP");
			FOREACH_RESULT(I_OnAccessCheck,OnAccessCheck(user,d,chan,AC_DEOP));

			log(DEBUG,"Returns %d",MOD_RESULT);

			if (MOD_RESULT == ACR_DENY)
				return "";
			if (MOD_RESULT == ACR_DEFAULT)
			{
				if ((status < STATUS_OP) && (!is_uline(user->server)) && (IS_LOCAL(user)))
				{
					WriteServ(user->fd,"482 %s %s :You are not a channel operator",user->nick, chan->name);
					return "";
				}
			}
		}

		return ModeParser::Revoke(d,chan,UCMODE_OP);
	}
	return "";
}
