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

#include "inspircd.h"
#include "users.h"
#include "modules.h"
#include "inspstring.h"
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

ModeHandler::ModeHandler(InspIRCd* Instance, char modeletter, int parameters_on, int parameters_off, bool listmode, ModeType type, bool operonly, char mprefix)
	: ServerInstance(Instance), mode(modeletter), n_params_on(parameters_on), n_params_off(parameters_off), list(listmode), m_type(type), oper(operonly), prefix(mprefix)
{
}

ModeHandler::~ModeHandler()
{
}

bool ModeHandler::IsListMode()
{
	return list;
}

unsigned int ModeHandler::GetPrefixRank()
{
	return 0;
}

ModeType ModeHandler::GetModeType()
{
	return m_type;
}

bool ModeHandler::NeedsOper()
{
	return oper;
}

char ModeHandler::GetPrefix()
{
	return prefix;
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
			ServerInstance->Log(DEBUG,"grant: %s %s",n->channel->name,d->nick);
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
			ServerInstance->Log(DEBUG,"revoke: %s %s",n->channel->name,d->nick);
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
		user->WriteServ("329 %s %s %lu", user->nick, targetchannel->name, (unsigned long)targetchannel->age);
		return;
	}
	else if (targetuser)
	{
		if ((targetuser == user) || (*user->oper))
		{
			/* Display user's current mode string */
			user->WriteServ("221 %s :+%s",targetuser->nick,targetuser->FormatModes());
			if (*targetuser->oper)
				user->WriteServ("008 %s +%s :Server notice mask", targetuser->nick, targetuser->FormatNoticeMasks());
			return;
		}
		else
		{
			user->WriteServ("502 %s :Can't change mode for other users", user->nick);
		}
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

	ServerInstance->Log(DEBUG,"ModeParser::Process start: pcnt=%d",pcnt);
	for (int j = 0; j < pcnt; j++)
		ServerInstance->Log(DEBUG,"    parameters[%d] = '%s'", j, parameters[j]);

	LastParse = "";

	/* Special case for displaying the list for listmodes,
	 * e.g. MODE #chan b, or MODE #chan +b without a parameter
	 */
	if ((targetchannel) && (pcnt == 2))
	{
		ServerInstance->Log(DEBUG,"Spool list");
		const char* mode = parameters[1];

		while (mode && *mode)
		{
			if (*mode == '+')
			{
				mode++;
				continue;
			}

			ModeHandler *mh = this->FindMode(*mode, MODETYPE_CHANNEL);

			if ((mh) && (mh->IsListMode()))
			{
				mh->DisplayList(user, targetchannel);
			}

			mode++;
		}
	}

	if (pcnt == 1)
	{
		ServerInstance->Log(DEBUG,"Mode list request");
		this->DisplayCurrentModes(user, targetuser, targetchannel, parameters[0]);
	}
	else if (pcnt > 1)
	{
		ServerInstance->Log(DEBUG,"More than one parameter");

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
				ServerInstance->Log(DEBUG,"The user is not a halfop or above, checking other reasons for being able to set the modes");

				int MOD_RESULT = 0;
				FOREACH_RESULT(I_OnAccessCheck,OnAccessCheck(user, NULL, targetchannel, AC_GENERAL_MODE));
				if (MOD_RESULT == ACR_DENY)
					return;

				if (MOD_RESULT == ACR_DEFAULT)
				{
					/* Are we a uline or is it a servermode? */
					if ((!ServerInstance->ULine(user->server)) && (!servermode))
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
		}
		else if (targetuser)
		{
			type = MODETYPE_USER;
			mask = MASK_USER;
			if ((user != targetuser) && (!ServerInstance->ULine(user->server)))
			{
				user->WriteServ("502 %s :Can't change mode for other users", user->nick);
				return;
			}
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
		int parameter_count = 0;

		/* A mode sequence that doesnt start with + or -. Assume +. - Thanks for the suggestion spike (bug#132) */
		if ((*mode_sequence.begin() != '+') && (*mode_sequence.begin() != '-'))
			mode_sequence.insert(0, "+");

		for (std::string::const_iterator letter = mode_sequence.begin(); letter != mode_sequence.end(); letter++)
		{
			unsigned char modechar = *letter;

			ServerInstance->Log(DEBUG,"Process letter %c", modechar);

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
									ServerInstance->Log(DEBUG,"parameter_counter = %d, pcnt = %d", parameter_counter, pcnt);
									parameter = parameters[parameter_counter++];

									/* Yerk, invalid! */
									if ((parameter.find(':') == 0) || (parameter.rfind(' ') != std::string::npos))
										parameter = "";
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
							{
								user->WriteServ("481 %s :Permission Denied- Only IRC operators may %sset %s mode %c", user->nick,
										adding ? "" : "un", type == MODETYPE_CHANNEL ? "channel" : "user",
										modehandlers[handler_id]->GetModeChar());
								continue;
							}

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
								{
									parameter_list << " " << parameter;
									parameter_count++;
									/* Does this mode have a prefix? */
									if (modehandlers[handler_id]->GetPrefix() && targetchannel)
									{
										userrec* user_to_prefix = ServerInstance->FindNick(parameter);
										if (user_to_prefix)
											targetchannel->SetPrefix(user_to_prefix, modehandlers[handler_id]->GetPrefix(),
													modehandlers[handler_id]->GetPrefixRank(), adding);
									}
								}

								/* Call all the AfterMode events in the mode watchers for this mode */
								for (ModeWatchIter watchers = modewatchers[handler_id].begin(); watchers != modewatchers[handler_id].end(); watchers++)
									(*watchers)->AfterMode(user, targetuser, targetchannel, parameter, adding, type);

								/* Reset the state change flag */
								state_change = false;

								if ((output_sequence.length() + parameter_list.str().length() > 450) || (output_sequence.length() > 100)
										|| (parameter_count > MAXMODES))
								{
									/* We cant have a mode sequence this long */
									letter = mode_sequence.end() - 1;
									continue;
								}
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
					this->LastParse = targetchannel->name;
				}
				else
				{
					targetuser->WriteServ("MODE %s %s%s",targetuser->nick,output_sequence.c_str(), parameter_list.str().c_str());
					this->LastParse = targetuser->nick;
				}
			}
			else
			{
				if (type == MODETYPE_CHANNEL)
				{
					ServerInstance->Log(DEBUG,"Write output sequence and parameters to channel: %s %s%s",targetchannel->name,output_sequence.c_str(),parameter_list.str().c_str());
					targetchannel->WriteChannel(user,"MODE %s %s%s",targetchannel->name,output_sequence.c_str(),parameter_list.str().c_str());
					FOREACH_MOD(I_OnMode,OnMode(user, targetchannel, TYPE_CHANNEL, output_sequence + parameter_list.str()));
					this->LastParse = targetchannel->name;
				}
				else
				{
					user->WriteTo(targetuser,"MODE %s %s%s",targetuser->nick,output_sequence.c_str(), parameter_list.str().c_str());
					FOREACH_MOD(I_OnMode,OnMode(user, targetuser, TYPE_USER, output_sequence + parameter_list.str()));
					this->LastParse = targetuser->nick;
				}
			}

			LastParse.append(" ");
			LastParse.append(output_sequence);
			LastParse.append(parameter_list.str());
		}
	}
}

const std::string& ModeParser::GetLastParse()
{
	return LastParse;
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

	/* A mode prefix of ',' is not acceptable, it would fuck up server to server.
	 * A mode prefix of ':' will fuck up both server to server, and client to server.
	 * A mode prefix of '#' will mess up /whois and /privmsg
	 */
	if ((mh->GetPrefix() == ',') || (mh->GetPrefix() == ':') || (mh->GetPrefix() == '#'))
		return false;

	mh->GetModeType() == MODETYPE_USER ? mask = MASK_USER : mask = MASK_CHANNEL;
	pos = (mh->GetModeChar()-65) | mask;

	if (modehandlers[pos])
		return false;

	modehandlers[pos] = mh;
	ServerInstance->Log(DEBUG,"ModeParser::AddMode: added mode %c",mh->GetModeChar());
	return true;
}

bool ModeParser::DelMode(ModeHandler* mh)
{
	unsigned char mask = 0;
	unsigned char pos = 0;

	if ((mh->GetModeChar() < 'A') || (mh->GetModeChar() > 'z'))
		return false;

	mh->GetModeType() == MODETYPE_USER ? mask = MASK_USER : mask = MASK_CHANNEL;
	pos = (mh->GetModeChar()-65) | mask;

	if (!modehandlers[pos])
		return false;

	switch (mh->GetModeType())
	{
		case MODETYPE_USER:
			for (user_hash::iterator i = ServerInstance->clientlist.begin(); i != ServerInstance->clientlist.end(); i++)
			{
				mh->RemoveMode(i->second);
			}
		break;
		case MODETYPE_CHANNEL:
			for (chan_hash::iterator i = ServerInstance->chanlist.begin(); i != ServerInstance->chanlist.end(); i++)
			{
				mh->RemoveMode(i->second);
			}
		break;
	}

	modehandlers[pos] = NULL;

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

ModeHandler* ModeParser::FindPrefix(unsigned const char pfxletter)
{
	for (unsigned char mode = 'A'; mode <= 'z'; mode++)
	{
		unsigned char pos = (mode-65) | MASK_CHANNEL;

		if ((modehandlers[pos]) && (modehandlers[pos]->GetPrefix() == pfxletter))
		{
			return modehandlers[pos];
		}
	}
	return NULL;
}

std::string ModeParser::ModeString(userrec* user, chanrec* channel)
{
	std::string types;
	std::string pars;

	if (!channel || !user)
		return "";

	for (unsigned char mode = 'A'; mode <= 'z'; mode++)
	{
		unsigned char pos = (mode-65) | MASK_CHANNEL;
		ModeHandler* mh = modehandlers[pos];
		if ((mh) && (mh->GetNumParams(true)) && (mh->GetNumParams(false)))
		{
			ModePair ret;
			ret = mh->ModeSet(NULL, user, channel, user->nick);
			if ((ret.first) && (ret.second == user->nick))
			{
				pars.append(" ");
				pars.append(user->nick);
				types.push_back(mh->GetModeChar());
			}
		}
	}

	return types+pars;
}

std::string ModeParser::ChanModes()
{
	std::string type1;	/* Listmodes EXCEPT those with a prefix */
	std::string type2;	/* Modes that take a param when adding or removing */
	std::string type3;	/* Modes that only take a param when adding */
	std::string type4;	/* Modes that dont take a param */

	for (unsigned char mode = 'A'; mode <= 'z'; mode++)
	{
		unsigned char pos = (mode-65) | MASK_CHANNEL;
		 /* One parameter when adding */
		if (modehandlers[pos])
		{	
			if (modehandlers[pos]->GetNumParams(true))
			{
				if ((modehandlers[pos]->IsListMode()) && (!modehandlers[pos]->GetPrefix()))
				{
					type1 += modehandlers[pos]->GetModeChar();
				}
				else
				{
					/* ... and one parameter when removing */
					if (modehandlers[pos]->GetNumParams(false))
					{
						/* But not a list mode */
						if (!modehandlers[pos]->GetPrefix())
						{
							type2 += modehandlers[pos]->GetModeChar();
						}
					}
					else
					{
						/* No parameters when removing */
						type3 += modehandlers[pos]->GetModeChar();
					}
				}
			}
			else
			{
				type4 += modehandlers[pos]->GetModeChar();
			}
		}
			 
	}

	return type1 + "," + type2 + "," + type3 + "," + type4;
}

bool ModeParser::PrefixComparison(const prefixtype one, const prefixtype two)
{       
	return one.second > two.second;
}

std::string ModeParser::BuildPrefixes()
{
	std::string mletters = "";
	std::string mprefixes = "";
	pfxcontainer pfx;
	std::map<char,char> prefix_to_mode;

	for (unsigned char mode = 'A'; mode <= 'z'; mode++)
	{
		unsigned char pos = (mode-65) | MASK_CHANNEL;

		if ((modehandlers[pos]) && (modehandlers[pos]->GetPrefix()))
		{
			pfx.push_back(std::make_pair<char,unsigned int>(modehandlers[pos]->GetPrefix(), modehandlers[pos]->GetPrefixRank()));
			prefix_to_mode[modehandlers[pos]->GetPrefix()] = modehandlers[pos]->GetModeChar();
		}
	}

	sort(pfx.begin(), pfx.end(), ModeParser::PrefixComparison);

	for (pfxcontainer::iterator n = pfx.begin(); n != pfx.end(); n++)
	{
		mletters = mletters + n->first;
		mprefixes = mprefixes + prefix_to_mode.find(n->first)->second;
	}

	return "(" + mprefixes + ")" + mletters;
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
	ServerInstance->Log(DEBUG,"ModeParser::AddModeWatcher: watching mode %c",mw->GetModeChar());

	return true;
}

bool ModeParser::DelModeWatcher(ModeWatcher* mw)
{
	unsigned char mask = 0;
	unsigned char pos = 0;

	if (!mw)
		return false;

	if ((mw->GetModeChar() < 'A') || (mw->GetModeChar() > 'z'))
		return false;

	mw->GetModeType() == MODETYPE_USER ? mask = MASK_USER : mask = MASK_CHANNEL;
	pos = (mw->GetModeChar()-65) | mask;

	ModeWatchIter a = find(modewatchers[pos].begin(),modewatchers[pos].end(),mw);

	if (a == modewatchers[pos].end())
	{
		ServerInstance->Log(DEBUG, "ModeParser::DelModeWatcher: Couldn't find watcher for mode %c in list", mw->GetModeChar());
		return false;
	}

	modewatchers[pos].erase(a);
	ServerInstance->Log(DEBUG,"ModeParser::DelModeWatcher: stopped watching mode %c",mw->GetModeChar());

	return true;
}

/** This default implementation can remove simple user modes
 */
void ModeHandler::RemoveMode(userrec* user)
{
	char moderemove[MAXBUF];
	const char* parameters[] = { user->nick, moderemove };

	if (user->IsModeSet(this->GetModeChar()))
	{
		sprintf(moderemove,"-%c",this->GetModeChar());
		ServerInstance->Parser->CallHandler("MODE", parameters, 2, user);
	}
}

/** This default implementation can remove simple channel modes
 * (no parameters)
 */
void ModeHandler::RemoveMode(chanrec* channel)
{
	char moderemove[MAXBUF];
	const char* parameters[] = { channel->name, moderemove };

	if (channel->IsModeSet(this->GetModeChar()))
	{
		userrec* n = new userrec(ServerInstance);

		sprintf(moderemove,"-%c",this->GetModeChar());
		n->SetFd(FD_MAGIC_NUMBER);

		ServerInstance->SendMode(parameters, 2, n);

		delete n;
	}
}

ModeParser::ModeParser(InspIRCd* Instance) : ServerInstance(Instance)
{
	/* Clear mode list */
	memset(modehandlers, 0, sizeof(modehandlers));
	memset(modewatchers, 0, sizeof(modewatchers));

	/* Last parse string */
	LastParse = "";

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
