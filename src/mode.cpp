/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *		      	 E-mail:
 *		<brain@chatspike.net>
 *	   	  <Craig@chatspike.net>
 *     
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

using namespace std;

#include "inspircd_config.h"
#include "inspircd.h"
#include "configreader.h"
#include <unistd.h>
#include "hash_map.h"
#include "connection.h"
#include "users.h"
#include "modules.h"
#include "inspstring.h"
#include "helperfuncs.h"
#include "commands.h"
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
/* +k (keyed channel) */
#include "modes/cmode_k.h"
/* +l (channel user limit) */
#include "modes/cmode_l.h"
/* +o (channel op) */
#include "modes/cmode_o.h"
/* +h (channel halfop) */
#include "modes/cmode_h.h"
/* +v (channel voice) */
#include "modes/cmode_v.h"

/* +s (server notices) */
#include "modes/umode_s.h"
/* +w (see wallops) */
#include "modes/umode_w.h"
/* +i (invisible) */
#include "modes/umode_i.h"
/* +o (operator) */
#include "modes/umode_o.h"
/* +n (notice mask - our implementation of snomasks) */
#include "modes/umode_n.h"

ModeHandler::ModeHandler(InspIRCd* Instance, char modeletter, int parameters_on, int parameters_off, bool listmode, ModeType type, bool operonly)
	: ServerInstance(Instance), mode(modeletter), n_params_on(parameters_on), n_params_off(parameters_off), list(listmode), m_type(type), oper(operonly)
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

ModePair ModeHandler::ModeSet(userrec* source, userrec* dest, chanrec* channel, const std::string &parameter)
{
	if (dest)
	{
		return std::make_pair(dest->IsModeSet(this->mode), "");
	}
	else
	{
		return std::make_pair(channel->IsModeSet(this->mode), "");
	}
}

void ModeHandler::DisplayList(userrec* user, chanrec* channel)
{
}

bool ModeHandler::CheckTimeStamp(time_t theirs, time_t ours, const std::string &their_param, const std::string &our_param, chanrec* channel)
{
	return (ours < theirs);
}

ModeWatcher::ModeWatcher(InspIRCd* Instance, char modeletter, ModeType type) : ServerInstance(Instance), mode(modeletter), m_type(type)
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

userrec* ModeParser::SanityChecks(userrec *user,const char *dest,chanrec *chan,int status)
{
	userrec *d;
	if ((!user) || (!dest) || (!chan) || (!*dest))
	{
		return NULL;
	}
	d = ServerInstance->FindNick(dest);
	if (!d)
	{
		user->WriteServ("401 %s %s :No such nick/channel",user->nick, dest);
		return NULL;
	}
	return d;
}

const char* ModeParser::Grant(userrec *d,chanrec *chan,int MASK)
{
	if (!chan)
		return "";

	for (std::vector<ucrec*>::const_iterator i = d->chans.begin(); i != d->chans.end(); i++)
	{
		ucrec* n = (ucrec*)(*i);
		if (n->channel == chan)
		{
			if (n->uc_modes & MASK)
			{
				return "";
			}
			n->uc_modes = n->uc_modes | MASK;
			switch (MASK)
			{
				case UCMODE_OP:
					n->channel->AddOppedUser(d);
				break;
				case UCMODE_HOP:
					n->channel->AddHalfoppedUser(d);
				break;
				case UCMODE_VOICE:
					n->channel->AddVoicedUser(d);
				break;
			}
			log(DEBUG,"grant: %s %s",n->channel->name,d->nick);
			return d->nick;
		}
	}
	return "";
}

const char* ModeParser::Revoke(userrec *d,chanrec *chan,int MASK)
{
	if (!chan)
		return "";

	for (std::vector<ucrec*>::const_iterator i = d->chans.begin(); i != d->chans.end(); i++)
	{
		ucrec* n = (ucrec*)(*i);
		if (n->channel == chan)
		{
			if ((n->uc_modes & MASK) == 0)
			{
				return "";
			}
			n->uc_modes ^= MASK;
			switch (MASK)
			{
				case UCMODE_OP:
					n->channel->DelOppedUser(d);
				break;
				case UCMODE_HOP:
					n->channel->DelHalfoppedUser(d);
				break;
				case UCMODE_VOICE:
					n->channel->DelVoicedUser(d);
				break;
			}
			log(DEBUG,"revoke: %s %s",n->channel->name,d->nick);
			return d->nick;
		}
	}
	return "";
}

void ModeParser::DisplayCurrentModes(userrec *user, userrec* targetuser, chanrec* targetchannel, const char* text)
{
	if (targetchannel)
	{
		/* Display channel's current mode string */
		user->WriteServ("324 %s %s +%s",user->nick, targetchannel->name, targetchannel->ChanModes(targetchannel->HasUser(user)));
		user->WriteServ("329 %s %s %d", user->nick, targetchannel->name, targetchannel->created);
		return;
	}
	else if (targetuser)
	{
		/* Display user's current mode string */
		user->WriteServ("221 %s :+%s",targetuser->nick,targetuser->FormatModes());
		user->WriteServ("008 %s :+%s", targetuser->nick, targetuser->FormatNoticeMasks());
		return;
	}

	/* No such nick/channel */
	user->WriteServ("401 %s %s :No such nick/channel",user->nick, text);
	return;
}

void ModeParser::Process(const char** parameters, int pcnt, userrec *user, bool servermode)
{
	std::string target = parameters[0];
	ModeType type = MODETYPE_USER;
	unsigned char mask = 0;
	chanrec* targetchannel = ServerInstance->FindChan(parameters[0]);
	userrec* targetuser  = ServerInstance->FindNick(parameters[0]);

	log(DEBUG,"ModeParser::Process start");

	/* Special case for displaying the list for listmodes,
	 * e.g. MODE #chan b, or MODE #chan +b without a parameter
	 */
	if ((targetchannel) && (pcnt == 2))
	{
		const char* mode = parameters[1];
		if (*mode == '+')
		mode++;
		unsigned char handler_id = ((*mode) - 65) | MASK_CHANNEL;
		ModeHandler* mh = modehandlers[handler_id];
		if ((mh) && (mh->IsListMode()))
		{
			mh->DisplayList(user, targetchannel);
			return;
		}
	}

	if (pcnt == 1)
	{
		this->DisplayCurrentModes(user, targetuser, targetchannel, parameters[0]);
	}
	else if (pcnt > 1)
	{
		if (targetchannel)
		{
			type = MODETYPE_CHANNEL;
			mask = MASK_CHANNEL;

			/* Extra security checks on channel modes
			 * (e.g. are they a (half)op?
			 */

			if ((IS_LOCAL(user)) && (targetchannel->GetStatus(user) < STATUS_HOP))
			{
				/* We don't have halfop */
				log(DEBUG,"The user is not a halfop or above, checking other reasons for being able to set the modes");

				/* Are we a uline or is it a servermode? */
				if ((!is_uline(user->server)) && (!servermode))
				{
					/* Not enough permission:
					 * NOT a uline and NOT a servermode,
					 * OR, NOT halfop or above.
					 */
					user->WriteServ("482 %s %s :You're not a channel (half)operator",user->nick, targetchannel->name);
					return;
				}
			}
		}
		else if (targetuser)
		{
			type = MODETYPE_USER;
			mask = MASK_USER;
		}
		else
		{
			/* No such nick/channel */
			user->WriteServ("401 %s %s :No such nick/channel",user->nick, parameters[0]);
			return;
		}

		std::string mode_sequence = parameters[1];
		std::string parameter = "";
		std::ostringstream parameter_list;
		std::string output_sequence = "";
		bool adding = true, state_change = false;
		unsigned char handler_id = 0;
		int parameter_counter = 2; /* Index of first parameter */

		for (std::string::const_iterator letter = mode_sequence.begin(); letter != mode_sequence.end(); letter++)
		{
			unsigned char modechar = *letter;

			switch (modechar)
			{
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

						for (ModeWatchIter watchers = modewatchers[handler_id].begin(); watchers != modewatchers[handler_id].end(); watchers++)
						{
							if ((*watchers)->BeforeMode(user, targetuser, targetchannel, parameter, adding, type) == MODEACTION_DENY)
								abort = true;
						}
						if ((modehandlers[handler_id]->GetModeType() == type) && (!abort))
						{
							if (modehandlers[handler_id]->GetNumParams(adding))
							{
								/* This mode expects a parameter, do we have any parameters left in our list to use? */
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

							/* It's an oper only mode, check if theyre an oper. If they arent,
							 * eat any parameter that  came with the mode, and continue to next
							 */
							if ((IS_LOCAL(user)) && (modehandlers[handler_id]->NeedsOper()) && (!*user->oper))
								continue;

							/* Call the handler for the mode */
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
								/* We're about to output a valid mode letter - was there previously a pending state-change? */
								if (state_change)
									output_sequence.append(adding ? "+" : "-");
								
								/* Add the mode letter */
								output_sequence.push_back(modechar);

								/* Is there a valid parameter for this mode? If so add it to the parameter list */
								if ((modehandlers[handler_id]->GetNumParams(adding)) && (parameter != ""))
									parameter_list << " " << parameter;

								/* Call all the AfterMode events in the mode watchers for this mode */
								for (ModeWatchIter watchers = modewatchers[handler_id].begin(); watchers != modewatchers[handler_id].end(); watchers++)
									(*watchers)->AfterMode(user, targetuser, targetchannel, parameter, adding, type);

								/* Reset the state change flag */
								state_change = false;
							}
						}
					}
					else
					{
						/* No mode handler? Unknown mode character then. */
						user->WriteServ("472 %s %c :is unknown mode char to me",user->nick, modechar);
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
					targetchannel->WriteChannelWithServ(ServerInstance->Config->ServerName, "MODE %s %s%s", targetchannel->name, output_sequence.c_str(), parameter_list.str().c_str());
				}
				else
				{
					targetuser->WriteServ("MODE %s %s",targetuser->nick,output_sequence.c_str());
				}
			}
			else
			{
				if (type == MODETYPE_CHANNEL)
				{
					log(DEBUG,"Write output sequence and parameters to channel: %s %s%s",targetchannel->name,output_sequence.c_str(),parameter_list.str().c_str());
					targetchannel->WriteChannel(user,"MODE %s %s%s",targetchannel->name,output_sequence.c_str(),parameter_list.str().c_str());
					FOREACH_MOD(I_OnMode,OnMode(user, targetchannel, TYPE_CHANNEL, output_sequence + parameter_list.str()));
				}
				else
				{
					user->WriteTo(targetuser,"MODE %s %s",targetuser->nick,output_sequence.c_str());
					FOREACH_MOD(I_OnMode,OnMode(user, targetuser, TYPE_USER, output_sequence));
				}
			}
		}
	}
}


void cmd_mode::Handle (const char** parameters, int pcnt, userrec *user)
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
	if ((mh->GetModeChar() < 'A') || (mh->GetModeChar() > 'z'))
		return false;

	mh->GetModeType() == MODETYPE_USER ? mask = MASK_USER : mask = MASK_CHANNEL;
	pos = (mh->GetModeChar()-65) | mask;

	if (modehandlers[pos])
		return false;

	modehandlers[pos] = mh;
	log(DEBUG,"ModeParser::AddMode: added mode %c",mh->GetModeChar());
	return true;
}

ModeHandler* ModeParser::FindMode(unsigned const char modeletter, ModeType mt)
{
	unsigned char mask = 0;
	unsigned char pos = 0;

	if ((modeletter < 'A') || (modeletter > 'z'))
		return NULL;

	mt == MODETYPE_USER ? mask = MASK_USER : mask = MASK_CHANNEL;
	pos = (modeletter-65) | mask;

	return modehandlers[pos];
}

std::string ModeParser::UserModeList()
{
	char modestr[256];
	int pointer = 0;

	for (unsigned char mode = 'A'; mode <= 'z'; mode++)
	{
		unsigned char pos = (mode-65) | MASK_USER;

		if (modehandlers[pos])
			modestr[pointer++] = mode;
	}
	modestr[pointer++] = 0;
	return modestr;
}

std::string ModeParser::ChannelModeList()
{
	char modestr[256];
	int pointer = 0;

	for (unsigned char mode = 'A'; mode <= 'z'; mode++)
	{
		unsigned char pos = (mode-65) | MASK_CHANNEL;

		if (modehandlers[pos])
			modestr[pointer++] = mode;
	}
	modestr[pointer++] = 0;
	return modestr;
}

std::string ModeParser::ParaModeList()
{
	char modestr[256];
	int pointer = 0;

	for (unsigned char mode = 'A'; mode <= 'z'; mode++)
	{
		unsigned char pos = (mode-65) | MASK_CHANNEL;

		if ((modehandlers[pos]) && (modehandlers[pos]->GetNumParams(true)))
			modestr[pointer++] = mode;
	}
	modestr[pointer++] = 0;
	return modestr;
}

bool ModeParser::AddModeWatcher(ModeWatcher* mw)
{
	unsigned char mask = 0;
	unsigned char pos = 0;

	if (!mw)
		return false;

	if ((mw->GetModeChar() < 'A') || (mw->GetModeChar() > 'z'))
		return false;

	mw->GetModeType() == MODETYPE_USER ? mask = MASK_USER : mask = MASK_CHANNEL;
	pos = (mw->GetModeChar()-65) | mask;

	modewatchers[pos].push_back(mw);
	log(DEBUG,"ModeParser::AddModeWatcher: watching mode %c",mw->GetModeChar());

	return true;
}

bool ModeParser::DelModeWatcher(ModeWatcher* mw)
{
	unsigned char mask = 0;
	unsigned char pos = 0;

	if (!mw)
		return false;

	if ((mw->GetModeType() < 'A') || (mw->GetModeType() > 'z'))
		return false;

	mw->GetModeType() == MODETYPE_USER ? mask = MASK_USER : mask = MASK_CHANNEL;
	pos = (mw->GetModeChar()-65) | mask;

	ModeWatchIter a = find(modewatchers[pos].begin(),modewatchers[pos].end(),mw);

	if (a == modewatchers[pos].end())
		return false;

	modewatchers[pos].erase(a);
	log(DEBUG,"ModeParser::DelModeWatcher: stopped watching mode %c",mw->GetModeChar());

	return true;
}

ModeParser::ModeParser(InspIRCd* Instance) : ServerInstance(Instance)
{
	/* Clear mode list */
	memset(modehandlers, 0, sizeof(modehandlers));
	memset(modewatchers, 0, sizeof(modewatchers));

	/* Initialise the RFC mode letters */

	/* Start with channel simple modes, no params */
	this->AddMode(new ModeChannelSecret(Instance), 's');
	this->AddMode(new ModeChannelPrivate(Instance), 'p');
	this->AddMode(new ModeChannelModerated(Instance), 'm');
	this->AddMode(new ModeChannelTopicOps(Instance), 't');
	this->AddMode(new ModeChannelNoExternal(Instance), 'n');
	this->AddMode(new ModeChannelInviteOnly(Instance), 'i');

	/* Cannel modes with params */
	this->AddMode(new ModeChannelKey(Instance), 'k');
	this->AddMode(new ModeChannelLimit(Instance), 'l');

	/* Channel listmodes */
	this->AddMode(new ModeChannelBan(Instance), 'b');
	this->AddMode(new ModeChannelOp(Instance), 'o');
	this->AddMode(new ModeChannelHalfOp(Instance), 'h');
	this->AddMode(new ModeChannelVoice(Instance), 'v');

	/* Now for usermodes */
	this->AddMode(new ModeUserServerNotice(Instance), 's');
	this->AddMode(new ModeUserWallops(Instance), 'w');
	this->AddMode(new ModeUserInvisible(Instance), 'i');
	this->AddMode(new ModeUserOperator(Instance), 'o');
	this->AddMode(new ModeUserServerNoticeMask(Instance), 'n');
}

bool ModeParser::InsertMode(std::string &output, const char* mode, unsigned short section)
{
	unsigned short currsection = 1;
	unsigned int pos = output.find("CHANMODES=", 0) + 10; // +10 for the length of "CHANMODES="

	if(section > 4 || section == 0)
	{
		log(DEBUG, "InsertMode: CHANMODES doesn't have a section %dh :/", section);
		return false;
	}

	for(; pos < output.size(); pos++)
	{
		if(section == currsection)
			break;

		if(output[pos] == ',')
			currsection++;
	}

	output.insert(pos, mode);
	return true;
}

