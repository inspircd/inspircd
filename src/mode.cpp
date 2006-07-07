/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *	              	 E-mail:
 *                <brain@chatspike.net>
 *           	  <Craig@chatspike.net>
 *     
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

using namespace std;

#include "inspircd_config.h"
#include "inspircd.h"
#include "configreader.h"
#include <unistd.h>
#include <sys/errno.h>
#include <time.h>
#include <string>
#include "hash_map.h"
#include <map>
#include <sstream>
#include <vector>
#include <deque>
#include "connection.h"
#include "users.h"
#include "ctables.h"
#include "globals.h"
#include "modules.h"
#include "dynamic.h"
#include "wildcard.h"
#include "message.h"
#include "commands.h"
#include "xline.h"
#include "inspstring.h"
#include "helperfuncs.h"
#include "mode.h"

/* +s (secret) */
#include "modes/cmode_s.h"
/* +p (private) */
#include "modes/cmode_p.h"
/* +b (bans) */
#include "modes/cmode_b.h"
/* +m (moderated) */
#include "modes/cmode_m.h"
/* +t (only (half) ops can change topic) */
#include "modes/cmode_t.h"
/* +n (no external messages) */
#include "modes/cmode_n.h"
/* +i (invite only) */
#include "modes/cmode_i.h"

extern int MODCOUNT;
extern std::vector<Module*> modules;
extern std::vector<ircd_module*> factory;
extern InspIRCd* ServerInstance;
extern ServerConfig* Config;

extern time_t TIME;

ModeHandler::ModeHandler(char modeletter, int parameters_on, int parameters_off, bool listmode, ModeType type, bool operonly)
	: mode(modeletter), n_params_on(parameters_on), n_params_off(parameters_off), list(listmode), m_type(type), oper(operonly)
{
}

ModeHandler::~ModeHandler()
{
}

bool ModeHandler::IsListMode()
{
	return list;
}

ModeType ModeHandler::GetModeType()
{
	return m_type;
}

bool ModeHandler::NeedsOper()
{
	return oper;
}

int ModeHandler::GetNumParams(bool adding)
{
	return adding ? n_params_on : n_params_off;
}

char ModeHandler::GetModeChar()
{
	return mode;
}

ModeAction ModeHandler::OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding)
{
	return MODEACTION_DENY;
}

void ModeHandler::DisplayList(userrec* user, chanrec* channel)
{
}

bool ModeHandler::CheckTimeStamp(time_t theirs, time_t ours, const std::string &their_param, const std::string &our_param, chanrec* channel)
{
	return (ours < theirs);
}

ModeWatcher::ModeWatcher(char modeletter, ModeType type) : mode(modeletter), m_type(type)
{
}

ModeWatcher::~ModeWatcher()
{
}

char ModeWatcher::GetModeChar()
{
	return mode;
}

ModeType ModeWatcher::GetModeType()
{
	return m_type;
}

bool ModeWatcher::BeforeMode(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding, ModeType type)
{
	return true;
}

void ModeWatcher::AfterMode(userrec* source, userrec* dest, chanrec* channel, const std::string &parameter, bool adding, ModeType type)
{
}

userrec* ModeParser::SanityChecks(userrec *user,char *dest,chanrec *chan,int status)
{
	userrec *d;
	if ((!user) || (!dest) || (!chan) || (!*dest))
	{
		return NULL;
	}
	d = Find(dest);
	if (!d)
	{
		WriteServ(user->fd,"401 %s %s :No such nick/channel",user->nick, dest);
		return NULL;
	}
	return d;
}

char* ModeParser::Grant(userrec *d,chanrec *chan,int MASK)
{
	if (!chan)
		return NULL;

	for (std::vector<ucrec*>::const_iterator i = d->chans.begin(); i != d->chans.end(); i++)
	{
		if (((ucrec*)(*i))->channel == chan)
		{
			if (((ucrec*)(*i))->uc_modes & MASK)
			{
				return NULL;
			}
			((ucrec*)(*i))->uc_modes = ((ucrec*)(*i))->uc_modes | MASK;
			switch (MASK)
			{
				case UCMODE_OP:
					((ucrec*)(*i))->channel->AddOppedUser(d);
				break;
				case UCMODE_HOP:
					((ucrec*)(*i))->channel->AddHalfoppedUser(d);
				break;
				case UCMODE_VOICE:
					((ucrec*)(*i))->channel->AddVoicedUser(d);
				break;
			}
			log(DEBUG,"grant: %s %s",((ucrec*)(*i))->channel->name,d->nick);
			return d->nick;
		}
	}
	return NULL;
}

char* ModeParser::Revoke(userrec *d,chanrec *chan,int MASK)
{
	if (!chan)
		return NULL;

	for (std::vector<ucrec*>::const_iterator i = d->chans.begin(); i != d->chans.end(); i++)
	{
		if (((ucrec*)(*i))->channel == chan)
		{
			if ((((ucrec*)(*i))->uc_modes & MASK) == 0)
			{
				return NULL;
			}
			((ucrec*)(*i))->uc_modes ^= MASK;
			switch (MASK)
			{
				case UCMODE_OP:
					((ucrec*)(*i))->channel->DelOppedUser(d);
				break;
				case UCMODE_HOP:
					((ucrec*)(*i))->channel->DelHalfoppedUser(d);
				break;
				case UCMODE_VOICE:
					((ucrec*)(*i))->channel->DelVoicedUser(d);
				break;
			}
			log(DEBUG,"revoke: %s %s",((ucrec*)(*i))->channel->name,d->nick);
			return d->nick;
		}
	}
	return NULL;
}

char* ModeParser::GiveOps(userrec *user,char *dest,chanrec *chan,int status)
{
	userrec *d = this->SanityChecks(user,dest,chan,status);
	
	if (d)
	{
		if (IS_LOCAL(user))
		{
			int MOD_RESULT = 0;
			FOREACH_RESULT(I_OnAccessCheck,OnAccessCheck(user,d,chan,AC_OP));
			
			if (MOD_RESULT == ACR_DENY)
				return NULL;
			if (MOD_RESULT == ACR_DEFAULT)
			{
				if ((status < STATUS_OP) && (!is_uline(user->server)))
				{
					WriteServ(user->fd,"482 %s %s :You're not a channel operator",user->nick, chan->name);
					return NULL;
				}
			}
		}

		return this->Grant(d,chan,UCMODE_OP);
	}
	return NULL;
}

char* ModeParser::GiveHops(userrec *user,char *dest,chanrec *chan,int status)
{
	userrec *d = this->SanityChecks(user,dest,chan,status);
	
	if (d)
	{
		if (IS_LOCAL(user))
		{
			int MOD_RESULT = 0;
			FOREACH_RESULT(I_OnAccessCheck,OnAccessCheck(user,d,chan,AC_HALFOP));
		
			if (MOD_RESULT == ACR_DENY)
				return NULL;
			if (MOD_RESULT == ACR_DEFAULT)
			{
				if ((status < STATUS_OP) && (!is_uline(user->server)))
				{
					WriteServ(user->fd,"482 %s %s :You're not a channel operator",user->nick, chan->name);
					return NULL;
				}
			}
		}

		return this->Grant(d,chan,UCMODE_HOP);
	}
	return NULL;
}

char* ModeParser::GiveVoice(userrec *user,char *dest,chanrec *chan,int status)
{
	userrec *d = this->SanityChecks(user,dest,chan,status);
	
	if (d)
	{
		if (IS_LOCAL(user))
		{
			int MOD_RESULT = 0;
			FOREACH_RESULT(I_OnAccessCheck,OnAccessCheck(user,d,chan,AC_VOICE));
			
			if (MOD_RESULT == ACR_DENY)
				return NULL;
			if (MOD_RESULT == ACR_DEFAULT)
			{
				if ((status < STATUS_HOP) && (!is_uline(user->server)))
				{
					WriteServ(user->fd,"482 %s %s :You must be at least a half-operator to change modes on this channel",user->nick, chan->name);
					return NULL;
				}
			}
		}

		return this->Grant(d,chan,UCMODE_VOICE);
	}
	return NULL;
}

char* ModeParser::TakeOps(userrec *user,char *dest,chanrec *chan,int status)
{
	userrec *d = this->SanityChecks(user,dest,chan,status);
	
	if (d)
	{
		if (IS_LOCAL(user))
		{
			int MOD_RESULT = 0;
			FOREACH_RESULT(I_OnAccessCheck,OnAccessCheck(user,d,chan,AC_DEOP));
			
			if (MOD_RESULT == ACR_DENY)
				return NULL;
			if (MOD_RESULT == ACR_DEFAULT)
			{
				if ((status < STATUS_OP) && (!is_uline(user->server)) && (IS_LOCAL(user)))
				{
					WriteServ(user->fd,"482 %s %s :You are not a channel operator",user->nick, chan->name);
					return NULL;
				}
			}
		}

		return this->Revoke(d,chan,UCMODE_OP);
	}
	return NULL;
}

char* ModeParser::TakeHops(userrec *user,char *dest,chanrec *chan,int status)
{
	userrec *d = this->SanityChecks(user,dest,chan,status);
	
	if (d)
	{
		if (IS_LOCAL(user))
		{
			int MOD_RESULT = 0;
			FOREACH_RESULT(I_OnAccessCheck,OnAccessCheck(user,d,chan,AC_DEHALFOP));
			
			if (MOD_RESULT == ACR_DENY)
				return NULL;
			if (MOD_RESULT == ACR_DEFAULT)
			{
				/* Tweak by Brain suggested by w00t, allow a halfop to dehalfop themselves */
				if ((user != d) && ((status < STATUS_OP) && (!is_uline(user->server))))
				{
					WriteServ(user->fd,"482 %s %s :You are not a channel operator",user->nick, chan->name);
					return NULL;
				}
			}
		}

		return this->Revoke(d,chan,UCMODE_HOP);
	}
	return NULL;
}

char* ModeParser::TakeVoice(userrec *user,char *dest,chanrec *chan,int status)
{
	userrec *d = this->SanityChecks(user,dest,chan,status);

	if (d)	
	{
		if (IS_LOCAL(user))
		{
			int MOD_RESULT = 0;
			FOREACH_RESULT(I_OnAccessCheck,OnAccessCheck(user,d,chan,AC_DEVOICE));
			
			if (MOD_RESULT == ACR_DENY)
				return NULL;
			if (MOD_RESULT == ACR_DEFAULT)
			{
				if ((status < STATUS_HOP) && (!is_uline(user->server)))
				{
					WriteServ(user->fd,"482 %s %s :You must be at least a half-operator to change modes on this channel",user->nick, chan->name);
					return NULL;
				}
			}
		}

		return this->Revoke(d,chan,UCMODE_VOICE);
	}
	return NULL;
}

void ModeParser::Process(char **parameters, int pcnt, userrec *user, bool servermode)
{
	std::string target = parameters[0];
	ModeType type = MODETYPE_USER;
	unsigned char mask = 0;
	chanrec* targetchannel = FindChan(parameters[0]);
	userrec* targetuser  = Find(parameters[0]);

	log(DEBUG,"ModeParser::Process start");

	if (pcnt > 1)
	{
		if (targetchannel)
		{
			log(DEBUG,"Target type is CHANNEL");
			type = MODETYPE_CHANNEL;
			mask = MASK_CHANNEL;
		}
		else if (targetuser)
		{
			log(DEBUG,"Target type is USER");
			type = MODETYPE_USER;
			mask = MASK_USER;
		}
		else
		{
			/* No such nick/channel */
			log(DEBUG,"Target type is UNKNOWN, bailing");
			return;
		}
		std::string mode_sequence = parameters[1];
		std::string parameter = "";
		std::ostringstream parameter_list;
		std::string output_sequence = "";
		bool adding = true, state_change = false;
		int handler_id = 0;
		int parameter_counter = 2; /* Index of first parameter */

		for (std::string::const_iterator letter = mode_sequence.begin(); letter != mode_sequence.end(); letter++)
		{
			unsigned char modechar = *letter;

			switch (modechar)
			{

				log(DEBUG,"Iterate mode letter %c",modechar);

				/* NB:
				 * For + and - mode characters, we don't just stick the character into the output sequence.
				 * This is because the user may do something dumb, like: +-+ooo or +oo-+. To prevent this
				 * appearing in the output sequence, we store a flag which says there was a state change,
				 * which is set on any + or -, however, the + or - that we finish on is only appended to
				 * the output stream in the event it is followed by a non "+ or -" character, such as o or v.
				 */
				case '+':
					/* The following expression prevents: +o+o nick nick, compressing it to +oo nick nick,
					 * however, will allow the + if it is the first item in the sequence, regardless.
					 */
					if ((!adding) || (!output_sequence.length()))
						state_change = true;
					adding = true;
					continue;
				break;
				case '-':
					if ((adding) || (!output_sequence.length()))
						state_change = true;
					adding = false;
					continue;
				break;
				default:

					/**
					 * Watch carefully for the sleight of hand trick.
					 * 65 is the ascii value of 'A'. We take this from
					 * the char we're looking at to get a number between
					 * 1 and 127. We then logic-or it to get the hashed
					 * position, dependent on wether its a channel or
					 * a user mode. This is a little stranger, but a lot
					 * faster, than using a map of pairs.
					 */
					handler_id = (modechar - 65) | mask;

					if (modehandlers[handler_id])
					{
						bool abort = false;

						log(DEBUG,"Found a ModeHandler* for mode %c",modechar);

						for (ModeWatchIter watchers = modewatchers[handler_id].begin(); watchers != modewatchers[handler_id].end(); watchers++)
						{
							log(DEBUG,"Call a ModeWatcher*");
							if ((*watchers)->BeforeMode(user, targetuser, targetchannel, parameter, adding, type) == MODEACTION_DENY)
								abort = true;
						}
						if ((modehandlers[handler_id]->GetModeType() == type) && (!abort))
						{
							log(DEBUG,"Modetype match, calling handler");

							if (modehandlers[handler_id]->GetNumParams(adding))
							{
								log(DEBUG,"ModeHandler* for this mode says it has parameters. pcnt=%d parameter_counter=%d",pcnt,parameter_counter);

								if (parameter_counter < pcnt)
								{
									parameter = parameters[parameter_counter++];
								}
								else
								{
									/* No parameter, continue to the next mode */
									continue;
								}
							}
							ModeAction ma = modehandlers[handler_id]->OnModeChange(user, targetuser, targetchannel, parameter, adding);

							if ((modehandlers[handler_id]->GetNumParams(adding)) && (parameter == ""))
							{
								/* The handler nuked the parameter and they are supposed to have one.
								 * We CANT continue now, even if they actually returned MODEACTION_ALLOW,
								 * so we bail to the next mode character.
								 */
								continue;
							}

							if (ma == MODEACTION_ALLOW)
							{
								log(DEBUG,"ModeAction was allow");

								/* We're about to output a valid mode letter - was there previously a pending state-change? */
								if (state_change)
								{
									log(DEBUG,"Appending state change");
									output_sequence.append(adding ? "+" : "-");
								}
								
								/* Add the mode letter */
								output_sequence.push_back(modechar);
								log(DEBUG,"Added mode letter to output sequence, sequence now: '%s'",output_sequence.c_str());

								/* Is there a valid parameter for this mode? If so add it to the parameter list */
								if ((modehandlers[handler_id]->GetNumParams(adding)) && (parameter != ""))
								{
									log(DEBUG,"Added parameter to parameter_list, list now: '%s'",parameter_list.str().c_str());
									parameter_list << " " << parameter;
								}

								/* Call all the AfterMode events in the mode watchers for this mode */
								for (ModeWatchIter watchers = modewatchers[handler_id].begin(); watchers != modewatchers[handler_id].end(); watchers++)
								{
									log(DEBUG,"Called a ModeWatcher* after event");
									(*watchers)->AfterMode(user, targetuser, targetchannel, parameter, adding, type);
								}

								/* Reset the state change flag */
								state_change = false;
							}
						}
					}
				break;
			}
		}
		/* Was there at least one valid mode in the sequence? */
		if (output_sequence != "")
		{
			if (servermode)
			{
				if (type == MODETYPE_CHANNEL)
				{
					WriteChannelWithServ(Config->ServerName,targetchannel,"MODE %s %s%s",targetchannel->name,output_sequence.c_str(),parameter_list.str().c_str());
				}
			}
			else
			{
				if (type == MODETYPE_CHANNEL)
				{
					log(DEBUG,"Write output sequence and parameters to channel: %s %s%s",targetchannel->name,output_sequence.c_str(),parameter_list.str().c_str());
					WriteChannel(targetchannel,user,"MODE %s %s%s",targetchannel->name,output_sequence.c_str(),parameter_list.str().c_str());
					FOREACH_MOD(I_OnMode,OnMode(user, targetchannel, TYPE_CHANNEL, output_sequence + parameter_list.str()));
				}
			}
		}
	}
}


void cmd_mode::Handle (char **parameters, int pcnt, userrec *user)
{
	if (!user)
		return;

	ServerInstance->ModeGrok->Process(parameters, pcnt, user, false);

	return;
}

void ModeParser::CleanMask(std::string &mask)
{
	std::string::size_type pos_of_pling = mask.find_first_of('!');
	std::string::size_type pos_of_at = mask.find_first_of('@');
	std::string::size_type pos_of_dot = mask.find_first_of('.');
	std::string::size_type pos_of_colon = mask.find_first_of(':'); /* Because ipv6 addresses are colon delimited */

	if ((pos_of_pling == std::string::npos) && (pos_of_at == std::string::npos))
	{
		/* Just a nick, or just a host */
		if ((pos_of_dot == std::string::npos) && (pos_of_colon == std::string::npos))
		{
			/* It has no '.' in it, it must be a nick. */
			mask.append("!*@*");
		}
		else
		{
			/* Got a dot in it? Has to be a host */
			mask = "*!*@" + mask;
		}
	}
	else if ((pos_of_pling == std::string::npos) && (pos_of_at != std::string::npos))
	{
		/* Has an @ but no !, its a user@host */
		 mask = "*!" + mask;
	}
	else if ((pos_of_pling != std::string::npos) && (pos_of_at == std::string::npos))
	{
		/* Has a ! but no @, it must be a nick!ident */
		mask.append("@*");
	}
}

bool ModeParser::AddMode(ModeHandler* mh, unsigned const char modeletter)
{
	unsigned char mask = 0;
	unsigned char pos = 0;

	/* Yes, i know, this might let people declare modes like '_' or '^'.
	 * If they do that, thats their problem, and if i ever EVER see an
	 * official InspIRCd developer do that, i'll beat them with a paddle!
	 */
	if ((modeletter < 'A') || (modeletter > 'z'))
		return false;

	mh->GetModeType() == MODETYPE_USER ? mask = MASK_USER : mask = MASK_CHANNEL;
	pos = (modeletter-65) | mask;

	if (modehandlers[pos])
		return false;

	modehandlers[pos] = mh;
	log(DEBUG,"ModeParser::AddMode: added mode %c",modeletter);
	return true;
}

ModeParser::ModeParser()
{
	/* Clear mode list */
	memset(modehandlers, 0, sizeof(modehandlers));
	memset(modewatchers, 0, sizeof(modewatchers));

	/* Initialise the RFC mode letters */
	this->AddMode(new ModeChannelSecret, 's');
	this->AddMode(new ModeChannelPrivate, 'p');
	this->AddMode(new ModeChannelBan, 'b');
	this->AddMode(new ModeChannelModerated, 'm');
	this->AddMode(new ModeChannelTopicOps, 't');
	this->AddMode(new ModeChannelNoExternal, 'n');
	this->AddMode(new ModeChannelInviteOnly, 'i');
}

