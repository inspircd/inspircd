/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

/* $Core */
/* $ExtraDeps: $(RELCPPFILES) */
/* $ExtraObjects: modes/modeclasses.a */
/* $ExtraBuild: @${MAKE} -C "modes" DIRNAME="src/modes" CC="$(CC)" $(MAKEARGS) CPPFILES="$(CPPFILES)" */

#include "inspircd.h"
#include "inspstring.h"

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
/* +w (see wallops) */
#include "modes/umode_w.h"
/* +i (invisible) */
#include "modes/umode_i.h"
/* +o (operator) */
#include "modes/umode_o.h"
/* +s (server notice masks) */
#include "modes/umode_s.h"

ModeHandler::ModeHandler(InspIRCd* Instance, char modeletter, int parameters_on, int parameters_off, bool listmode, ModeType type, bool operonly, char mprefix, char prefixrequired, TranslateType translate)
	: ServerInstance(Instance), mode(modeletter), n_params_on(parameters_on), n_params_off(parameters_off), list(listmode), m_type(type), m_paramtype(translate), oper(operonly), prefix(mprefix), count(0), prefixneeded(prefixrequired)
{
}

ModeHandler::~ModeHandler()
{
}

bool ModeHandler::IsListMode()
{
	return list;
}

char ModeHandler::GetNeededPrefix()
{
	return prefixneeded;
}

void ModeHandler::SetNeededPrefix(char needsprefix)
{
	prefixneeded = needsprefix;
}

unsigned int ModeHandler::GetPrefixRank()
{
	return 0;
}

unsigned int ModeHandler::GetCount()
{
	return 0;
}

void ModeHandler::ChangeCount(int modifier)
{
	count += modifier;
	ServerInstance->Logs->Log("MODE", DEBUG,"Change count for mode %c is now %d", mode, count);
}

ModeType ModeHandler::GetModeType()
{
	return m_type;
}

TranslateType ModeHandler::GetTranslateType()
{
	return m_paramtype;
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

std::string ModeHandler::GetUserParameter(User* user)
{
	return "";
}

ModeAction ModeHandler::OnModeChange(User*, User*, Channel*, std::string&, bool, bool)
{
	return MODEACTION_DENY;
}

ModePair ModeHandler::ModeSet(User*, User* dest, Channel* channel, const std::string&)
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

void ModeHandler::DisplayList(User*, Channel*)
{
}

void ModeHandler::DisplayEmptyList(User*, Channel*)
{
}

void ModeHandler::OnParameterMissing(User* user, User* dest, Channel* channel)
{
}

bool ModeHandler::CheckTimeStamp(time_t theirs, time_t ours, const std::string&, const std::string&, Channel*)
{
	return (ours < theirs);
}

SimpleUserModeHandler::SimpleUserModeHandler(InspIRCd* Instance, char modeletter) : ModeHandler(Instance, modeletter, 0, 0, false, MODETYPE_USER, false)
{
}

SimpleUserModeHandler::~SimpleUserModeHandler()
{
}

SimpleChannelModeHandler::~SimpleChannelModeHandler()
{
}

SimpleChannelModeHandler::SimpleChannelModeHandler(InspIRCd* Instance, char modeletter) : ModeHandler(Instance, modeletter, 0, 0, false, MODETYPE_CHANNEL, false)
{
}

ModeAction SimpleUserModeHandler::OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding, bool servermode)
{
	if (adding)
	{
		if (!dest->IsModeSet(this->GetModeChar()))
		{
			dest->SetMode(this->GetModeChar(),true);
			return MODEACTION_ALLOW;
		}
	}
	else
	{
		if (dest->IsModeSet(this->GetModeChar()))
		{
			dest->SetMode(this->GetModeChar(),false);
			return MODEACTION_ALLOW;
		}
	}

	return MODEACTION_DENY;
}


ModeAction SimpleChannelModeHandler::OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding, bool servermode)
{
	if (adding)
	{
		if (!channel->IsModeSet(this->GetModeChar()))
		{
			channel->SetMode(this->GetModeChar(),true);
			return MODEACTION_ALLOW;
		}
	}
	else
	{
		if (channel->IsModeSet(this->GetModeChar()))
		{
			channel->SetMode(this->GetModeChar(),false);
			return MODEACTION_ALLOW;
		}
	}

	return MODEACTION_DENY;
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

bool ModeWatcher::BeforeMode(User*, User*, Channel*, std::string&, bool, ModeType, bool)
{
	return true;
}

void ModeWatcher::AfterMode(User*, User*, Channel*, const std::string&, bool, ModeType, bool)
{
}

User* ModeParser::SanityChecks(User *user, const char *dest, Channel *chan, int)
{
	User *d;
	if ((!user) || (!dest) || (!chan) || (!*dest))
	{
		return NULL;
	}
	d = ServerInstance->FindNick(dest);
	if (!d)
	{
		user->WriteNumeric(ERR_NOSUCHNICK, "%s %s :No such nick/channel",user->nick.c_str(), dest);
		return NULL;
	}
	return d;
}

const char* ModeParser::Grant(User *d,Channel *chan,int MASK)
{
	if (!chan)
		return "";

	UCListIter n = d->chans.find(chan);
	if (n != d->chans.end())
	{
		if (n->second & MASK)
		{
			return "";
		}
		n->second = n->second | MASK;
		switch (MASK)
		{
			case UCMODE_OP:
				n->first->AddOppedUser(d);
			break;
			case UCMODE_HOP:
				n->first->AddHalfoppedUser(d);
			break;
			case UCMODE_VOICE:
				n->first->AddVoicedUser(d);
			break;
		}
		return d->nick.c_str();
	}
	return "";
}

const char* ModeParser::Revoke(User *d,Channel *chan,int MASK)
{
	if (!chan)
		return "";

	UCListIter n = d->chans.find(chan);
	if (n != d->chans.end())
	{
		if ((n->second & MASK) == 0)
		{
			return "";
		}
		n->second ^= MASK;
		switch (MASK)
		{
			case UCMODE_OP:
				n->first->DelOppedUser(d);
			break;
			case UCMODE_HOP:
				n->first->DelHalfoppedUser(d);
			break;
			case UCMODE_VOICE:
				n->first->DelVoicedUser(d);
			break;
		}
		return d->nick.c_str();
	}
	return "";
}

void ModeParser::DisplayCurrentModes(User *user, User* targetuser, Channel* targetchannel, const char* text)
{
	if (targetchannel)
	{
		/* Display channel's current mode string */
		user->WriteNumeric(RPL_CHANNELMODEIS, "%s %s +%s",user->nick.c_str(), targetchannel->name.c_str(), targetchannel->ChanModes(targetchannel->HasUser(user)));
		user->WriteNumeric(RPL_CHANNELCREATED, "%s %s %lu", user->nick.c_str(), targetchannel->name.c_str(), (unsigned long)targetchannel->age);
		return;
	}
	else if (targetuser)
	{
		if (targetuser->Visibility && !targetuser->Visibility->VisibleTo(user))
		{
			user->WriteNumeric(ERR_NOSUCHNICK, "%s %s :No such nick/channel",user->nick.c_str(), text);
			return;
		}

		if (targetuser == user || user->HasPrivPermission("users/auspex"))
		{
			/* Display user's current mode string */
			user->WriteNumeric(RPL_UMODEIS, "%s :+%s",targetuser->nick.c_str(),targetuser->FormatModes());
			if (IS_OPER(targetuser))
				user->WriteNumeric(RPL_SNOMASKIS, "%s +%s :Server notice mask", targetuser->nick.c_str(), targetuser->FormatNoticeMasks());
			return;
		}
		else
		{
			user->WriteNumeric(ERR_USERSDONTMATCH, "%s :Can't change mode for other users", user->nick.c_str());
			return;
		}
	}

	/* No such nick/channel */
	user->WriteNumeric(ERR_NOSUCHNICK, "%s %s :No such nick/channel",user->nick.c_str(), text);
	return;
}

void ModeParser::Process(const std::vector<std::string>& parameters, User *user, bool servermode)
{
	std::string target = parameters[0];
	ModeType type = MODETYPE_USER;
	unsigned char mask = 0;
	Channel* targetchannel = ServerInstance->FindChan(parameters[0]);
	User* targetuser  = ServerInstance->FindNick(parameters[0]);

	LastParse.clear();
	LastParseParams.clear();
	LastParseTranslate.clear();

	/* Special case for displaying the list for listmodes,
	 * e.g. MODE #chan b, or MODE #chan +b without a parameter
	 */
	if ((targetchannel) && (parameters.size() == 2))
	{
		const char* mode = parameters[1].c_str();
		int nonlistmodes_found = 0;

		seq++;

		mask = MASK_CHANNEL;

		while (mode && *mode)
		{
			unsigned char mletter = *mode;

			if (*mode == '+')
			{
				mode++;
				continue;
			}

			/* Ensure the user doesnt request the same mode twice,
			 * so they cant flood themselves off out of idiocy.
			 */
			if (sent[mletter] != seq)
			{
				sent[mletter] = seq;
			}
			else
			{
				mode++;
				continue;
			}

			ModeHandler *mh = this->FindMode(*mode, MODETYPE_CHANNEL);
			bool display = true;

			if ((mh) && (mh->IsListMode()))
			{
				int MOD_RESULT = 0;
				FOREACH_RESULT(I_OnRawMode, OnRawMode(user, targetchannel, *mode, "", true, 0));
				if (MOD_RESULT == ACR_DENY)
				{
					mode++;
					continue;
				}

				if (!user->HasPrivPermission("channels/auspex"))
				{
					if (ServerInstance->Config->HideModeLists[mletter] && (targetchannel->GetStatus(user) < STATUS_HOP))
					{
						user->WriteNumeric(ERR_CHANOPRIVSNEEDED, "%s %s :Only half-operators and above may view the +%c list",user->nick.c_str(), targetchannel->name.c_str(), *mode++);
						mh->DisplayEmptyList(user, targetchannel);
						continue;
					}
				}

				/** See below for a description of what craq this is :D
				 */
				unsigned char handler_id = (*mode - 65) | mask;

				for(ModeWatchIter watchers = modewatchers[handler_id].begin(); watchers != modewatchers[handler_id].end(); watchers++)
				{
					std::string dummyparam;

					if (!((*watchers)->BeforeMode(user, NULL, targetchannel, dummyparam, true, MODETYPE_CHANNEL)))
						display = false;
				}

				if (display)
					mh->DisplayList(user, targetchannel);
			}
			else
				nonlistmodes_found++;

			mode++;
		}

		/* We didnt have any modes that were non-list, we can return here */
		if (!nonlistmodes_found)
			return;
	}

	if (parameters.size() == 1)
	{
		this->DisplayCurrentModes(user, targetuser, targetchannel, parameters[0].c_str());
	}
	else if (parameters.size() > 1)
	{
		bool SkipAccessChecks = false;

		if (targetchannel)
		{
			type = MODETYPE_CHANNEL;
			mask = MASK_CHANNEL;

			/* Extra security checks on channel modes
			 * (e.g. are they a (half)op?
			 */

			if ((IS_LOCAL(user)) && (!ServerInstance->ULine(user->server)) && (!servermode))
			{
				/* Make modes that are being changed visible to OnAccessCheck */
				LastParse = parameters[1];
				/* We don't have halfop */
				int MOD_RESULT = 0;
				FOREACH_RESULT(I_OnAccessCheck,OnAccessCheck(user, NULL, targetchannel, AC_GENERAL_MODE));
				LastParse.clear();
				if (MOD_RESULT == ACR_DENY)
					return;
				SkipAccessChecks = (MOD_RESULT == ACR_ALLOW);
			}
		}
		else if (targetuser)
		{
			type = MODETYPE_USER;
			mask = MASK_USER;
			if (user != targetuser && IS_LOCAL(user) && !ServerInstance->ULine(user->server))
			{
				user->WriteNumeric(ERR_USERSDONTMATCH, "%s :Can't change mode for other users", user->nick.c_str());
				return;
			}
		}
		else
		{
			/* No such nick/channel */
			user->WriteNumeric(ERR_NOSUCHNICK, "%s %s :No such nick/channel",user->nick.c_str(), parameters[0].c_str());
			return;
		}

		std::string mode_sequence = parameters[1];
		std::string parameter;
		std::ostringstream parameter_list;
		std::string output_sequence;
		bool adding = true, state_change = false;
		unsigned char handler_id = 0;
		unsigned int parameter_counter = 2; /* Index of first parameter */
		unsigned int parameter_count = 0;
		bool last_successful_state_change = false;
		LastParseTranslate.push_back(TR_TEXT);

		/* A mode sequence that doesnt start with + or -. Assume +. - Thanks for the suggestion spike (bug#132) */
		if ((*mode_sequence.begin() != '+') && (*mode_sequence.begin() != '-'))
			mode_sequence.insert(0, "+");

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
					if (!output_sequence.length())
						last_successful_state_change = false;
					continue;
				break;
				case '-':
					if ((adding) || (!output_sequence.length()))
						state_change = true;
					adding = false;
					if (!output_sequence.length())
						last_successful_state_change = true;
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

						if (modehandlers[handler_id]->GetModeType() == type)
						{
							int MOD_RESULT = 0;

							if (modehandlers[handler_id]->GetNumParams(adding))
							{
								/* This mode expects a parameter, do we have any parameters left in our list to use? */
								if (parameter_counter < parameters.size())
								{
									parameter = parameters[parameter_counter++];

									/* Yerk, invalid! */
									if ((parameter.find(':') == 0) || (parameter.rfind(' ') != std::string::npos))
										parameter.clear();
								}
								else
								{
									/* No parameter, continue to the next mode */
									modehandlers[handler_id]->OnParameterMissing(user, targetuser, targetchannel);
									continue;
								}

								FOREACH_RESULT(I_OnRawMode, OnRawMode(user, targetchannel, modechar, parameter, adding, 1, servermode));
							}
							else
							{
								FOREACH_RESULT(I_OnRawMode, OnRawMode(user, targetchannel, modechar, "", adding, 0, servermode));
							}

							if (IS_LOCAL(user) && (MOD_RESULT == ACR_DENY))
								continue;

							if (!SkipAccessChecks && IS_LOCAL(user) && (MOD_RESULT != ACR_ALLOW))
							{
								/* Check access to this mode character */
								if ((type == MODETYPE_CHANNEL) && (modehandlers[handler_id]->GetNeededPrefix()))
								{
									char needed = modehandlers[handler_id]->GetNeededPrefix();
									ModeHandler* prefixmode = FindPrefix(needed);

									/* If the mode defined by the handler is not '\0', but the handler for it
									 * cannot be found, they probably dont have the right module loaded to implement
									 * the prefix they want to compare the mode against, e.g. '&' for m_chanprotect.
									 * Revert to checking against the minimum core prefix, '%'.
									 */
									if (needed && !prefixmode)
										prefixmode = FindPrefix('%');

									unsigned int neededrank = prefixmode->GetPrefixRank();
									/* Compare our rank on the channel against the rank of the required prefix,
									 * allow if >= ours. Because mIRC and xchat throw a tizz if the modes shown
									 * in NAMES(X) are not in rank order, we know the most powerful mode is listed
									 * first, so we don't need to iterate, we just look up the first instead.
									 */
									std::string modestring = targetchannel->GetAllPrefixChars(user);
									char ml = (modestring.empty() ? '\0' : modestring[0]);
									ModeHandler* ourmode = FindPrefix(ml);
									if (!ourmode || ourmode->GetPrefixRank() < neededrank)
									{
										/* Bog off */
										user->WriteNumeric(ERR_CHANOPRIVSNEEDED, "%s %s :You must have channel privilege %c or above to %sset channel mode %c",
												user->nick.c_str(), targetchannel->name.c_str(), needed, adding ? "" : "un", modechar);
										continue;
									}
								}
							}

							bool had_parameter = !parameter.empty();

							for (ModeWatchIter watchers = modewatchers[handler_id].begin(); watchers != modewatchers[handler_id].end(); watchers++)
							{
								if ((*watchers)->BeforeMode(user, targetuser, targetchannel, parameter, adding, type, servermode) == false)
								{
									abort = true;
									break;
								}
								/* A module whacked the parameter completely, and there was one. abort. */
								if ((had_parameter) && (parameter.empty()))
								{
									abort = true;
									break;
								}
							}

							if (abort)
								continue;

							/* If it's disabled, they have to be an oper.
							 */
							if (IS_LOCAL(user) && !IS_OPER(user) && ((type == MODETYPE_CHANNEL ? ServerInstance->Config->DisabledCModes : ServerInstance->Config->DisabledUModes)[modehandlers[handler_id]->GetModeChar() - 'A']))
							{
								user->WriteNumeric(ERR_NOPRIVILEGES, "%s :Permission Denied - %s mode %c has been locked by the administrator",
										user->nick.c_str(),
										type == MODETYPE_CHANNEL ? "channel" : "user",
										modehandlers[handler_id]->GetModeChar());
								continue;
							}

							/* It's an oper only mode, check if theyre an oper. If they arent,
							 * eat any parameter that  came with the mode, and continue to next
							 */
							if (adding && (IS_LOCAL(user)) && (modehandlers[handler_id]->NeedsOper()) && (!user->HasModePermission(modehandlers[handler_id]->GetModeChar(), type)))
							{
								if (IS_OPER(user))
								{
									user->WriteNumeric(ERR_NOPRIVILEGES, "%s :Permission Denied - Oper type %s does not have access to set %s mode %c",
											user->nick.c_str(),
											user->oper.c_str(),
											type == MODETYPE_CHANNEL ? "channel" : "user",
											modehandlers[handler_id]->GetModeChar());
								}
								else
								{
									user->WriteNumeric(ERR_NOPRIVILEGES, "%s :Permission Denied - Only operators may set %s mode %c",
											user->nick.c_str(),
											type == MODETYPE_CHANNEL ? "channel" : "user",
											modehandlers[handler_id]->GetModeChar());
								}
								continue;
							}

							/* Call the handler for the mode */
							ModeAction ma = modehandlers[handler_id]->OnModeChange(user, targetuser, targetchannel, parameter, adding, servermode);

							if ((modehandlers[handler_id]->GetNumParams(adding)) && (parameter.empty()))
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
								{
									if (adding != last_successful_state_change)
										output_sequence.append(adding ? "+" : "-");
									last_successful_state_change = adding;
								}

								/* Add the mode letter */
								output_sequence.push_back(modechar);

								modehandlers[handler_id]->ChangeCount(adding ? 1 : -1);

								/* Is there a valid parameter for this mode? If so add it to the parameter list */
								if ((modehandlers[handler_id]->GetNumParams(adding)) && (!parameter.empty()))
								{
									parameter_list << " " << parameter;
									LastParseParams.push_back(parameter);
									LastParseTranslate.push_back(modehandlers[handler_id]->GetTranslateType());
									parameter_count++;
									/* Does this mode have a prefix? */
									if (modehandlers[handler_id]->GetPrefix() && targetchannel)
									{
										User* user_to_prefix = ServerInstance->FindNick(parameter);
										if (user_to_prefix)
											targetchannel->SetPrefix(user_to_prefix, modehandlers[handler_id]->GetPrefix(),
													modehandlers[handler_id]->GetPrefixRank(), adding);
									}
								}

								/* Call all the AfterMode events in the mode watchers for this mode */
								for (ModeWatchIter watchers = modewatchers[handler_id].begin(); watchers != modewatchers[handler_id].end(); watchers++)
									(*watchers)->AfterMode(user, targetuser, targetchannel, parameter, adding, type, servermode);

								/* Reset the state change flag */
								state_change = false;

								if ((output_sequence.length() + parameter_list.str().length() > 450) || (output_sequence.length() > 100)
										|| (parameter_count > ServerInstance->Config->Limits.MaxModes))
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
						user->WriteServ("%d %s %c :is unknown mode char to me", type == MODETYPE_CHANNEL ? 472 : 501, user->nick.c_str(), modechar);
					}
				break;
			}
		}

		/* Was there at least one valid mode in the sequence? */
		if (!output_sequence.empty())
		{
			if (servermode)
			{
				if (type == MODETYPE_CHANNEL)
				{
					targetchannel->WriteChannelWithServ(ServerInstance->Config->ServerName, "MODE %s %s%s", targetchannel->name.c_str(), output_sequence.c_str(), parameter_list.str().c_str());
					this->LastParse = targetchannel->name;
				}
				else
				{
					targetuser->WriteServ("MODE %s %s%s",targetuser->nick.c_str(),output_sequence.c_str(), parameter_list.str().c_str());
					this->LastParse = targetuser->nick;
				}
			}
			else
			{
				LastParseParams.push_front(output_sequence);
				if (type == MODETYPE_CHANNEL)
				{
					targetchannel->WriteChannel(user, "MODE %s %s%s", targetchannel->name.c_str(), output_sequence.c_str(), parameter_list.str().c_str());
					FOREACH_MOD(I_OnMode,OnMode(user, targetchannel, TYPE_CHANNEL, LastParseParams, LastParseTranslate));
					this->LastParse = targetchannel->name;
				}
				else
				{
					user->WriteTo(targetuser, "MODE %s %s%s", targetuser->nick.c_str(), output_sequence.c_str(), parameter_list.str().c_str());
					FOREACH_MOD(I_OnMode,OnMode(user, targetuser, TYPE_USER, LastParseParams, LastParseTranslate));
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
	std::string::size_type pos_of_colons = mask.find("::"); /* Because ipv6 addresses are colon delimited -- double so it treats extban as nick */

	if (mask.length() >= 2 && mask[1] == ':')
		return; // if it's an extban, don't even try guess how it needs to be formed.

	if ((pos_of_pling == std::string::npos) && (pos_of_at == std::string::npos))
	{
		/* Just a nick, or just a host - or clearly ipv6 (starting with :) */
		if ((pos_of_dot == std::string::npos) && (pos_of_colons == std::string::npos) && mask[0] != ':')
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

bool ModeParser::AddMode(ModeHandler* mh)
{
	unsigned char mask = 0;
	unsigned char pos = 0;

	/* Yes, i know, this might let people declare modes like '_' or '^'.
	 * If they do that, thats their problem, and if i ever EVER see an
	 * official InspIRCd developer do that, i'll beat them with a paddle!
	 */
	if ((mh->GetModeChar() < 'A') || (mh->GetModeChar() > 'z') || (mh->GetPrefix() > 126))
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

	/* Note: We can't stack here, as we have modes potentially being removed across many different channels.
	 * To stack here we have to make the algorithm slower. Discuss.
	 */
	switch (mh->GetModeType())
	{
		case MODETYPE_USER:
			for (user_hash::iterator i = ServerInstance->Users->clientlist->begin(); i != ServerInstance->Users->clientlist->end(); i++)
			{
				mh->RemoveMode(i->second);
			}
		break;
		case MODETYPE_CHANNEL:
			for (chan_hash::iterator i = ServerInstance->chanlist->begin(); i != ServerInstance->chanlist->end(); i++)
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

std::string ModeParser::ModeString(User* user, Channel* channel, bool nick_suffix)
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
				if (nick_suffix)
				{
					pars.append(" ");
					pars.append(user->nick);
				}
				types.push_back(mh->GetModeChar());
			}
		}
	}

	if (nick_suffix)
		return types+pars;
	else
		return types;
}

std::string ModeParser::GiveModeList(ModeMasks m)
{
	std::string type1;	/* Listmodes EXCEPT those with a prefix */
	std::string type2;	/* Modes that take a param when adding or removing */
	std::string type3;	/* Modes that only take a param when adding */
	std::string type4;	/* Modes that dont take a param */

	for (unsigned char mode = 'A'; mode <= 'z'; mode++)
	{
		unsigned char pos = (mode-65) | m;
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

bool ModeParser::PrefixComparison(prefixtype one, prefixtype two)
{
	return one.second > two.second;
}

std::string ModeParser::BuildPrefixes()
{
	std::string mletters;
	std::string mprefixes;
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
		return false;
	}

	modewatchers[pos].erase(a);

	return true;
}

/** This default implementation can remove simple user modes
 */
void ModeHandler::RemoveMode(User* user, irc::modestacker* stack)
{
	char moderemove[MAXBUF];
	std::vector<std::string> parameters;

	if (user->IsModeSet(this->GetModeChar()))
	{
		if (stack)
		{
			stack->Push(this->GetModeChar());
		}
		else
		{
			sprintf(moderemove,"-%c",this->GetModeChar());
			parameters.push_back(user->nick);
			parameters.push_back(moderemove);
			ServerInstance->Modes->Process(parameters, ServerInstance->FakeClient, false);
		}
	}
}

/** This default implementation can remove simple channel modes
 * (no parameters)
 */
void ModeHandler::RemoveMode(Channel* channel, irc::modestacker* stack)
{
	char moderemove[MAXBUF];
	std::vector<std::string> parameters;

	if (channel->IsModeSet(this->GetModeChar()))
	{
		if (stack)
		{
			stack->Push(this->GetModeChar());
		}
		else
		{
			sprintf(moderemove,"-%c",this->GetModeChar());
			parameters.push_back(channel->name);
			parameters.push_back(moderemove);
			ServerInstance->SendMode(parameters, ServerInstance->FakeClient);
		}
	}
}

ModeParser::ModeParser(InspIRCd* Instance) : ServerInstance(Instance)
{
	ModeHandler* modes[] =
	{
		new ModeChannelSecret(Instance),
		new ModeChannelPrivate(Instance),
		new ModeChannelModerated(Instance),
		new ModeChannelTopicOps(Instance),
		new ModeChannelNoExternal(Instance),
		new ModeChannelInviteOnly(Instance),
		new ModeChannelKey(Instance),
		new ModeChannelLimit(Instance),
		new ModeChannelBan(Instance),
		new ModeChannelOp(Instance),
		new ModeChannelHalfOp(Instance),
		new ModeChannelVoice(Instance),
		new ModeUserWallops(Instance),
		new ModeUserInvisible(Instance),
		new ModeUserOperator(Instance),
		new ModeUserServerNoticeMask(Instance),
		NULL
	};

	/* Clear mode handler list */
	memset(modehandlers, 0, sizeof(modehandlers));

	/* Last parse string */
	LastParse.clear();

	/* Initialise the RFC mode letters */
	for (int index = 0; modes[index]; index++)
		this->AddMode(modes[index]);

	seq = 0;
	memset(&sent, 0, sizeof(sent));
}
