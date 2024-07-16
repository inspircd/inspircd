/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2021 Dominic Hamon
 *   Copyright (C) 2020-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2018 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2014 Adam <Adam@anope.org>
 *   Copyright (C) 2013-2014, 2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2013 Daniel Vassdal <shutter@canternet.org>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2009 Matt Smith <dz@inspircd.org>
 *   Copyright (C) 2008 Uli Schlachter <psychon@znc.in>
 *   Copyright (C) 2008 Craig Edwards <brain@inspircd.org>
 *   Copyright (C) 2007-2009 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Oliver Lupton <om@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
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
#include "listmode.h"
#include "modules/extban.h"

/* Originally written by Om, January 2009
 */

class BanRedirectEntry final
{
public:
	std::string targetchan;
	std::string banmask;

	BanRedirectEntry(const std::string& target = "", const std::string& mask = "")
		: targetchan(target)
		, banmask(mask)
	{
	}
};

typedef std::vector<BanRedirectEntry> BanRedirectList;

class BanRedirect final
	: public ModeWatcher
{
public:
	ChanModeReference banmode;
	ExtBan::ManagerRef extbanmgr;
	SimpleExtItem<BanRedirectList> redirectlist;

	BanRedirect(Module* parent)
		: ModeWatcher(parent, "ban", MODETYPE_CHANNEL)
		, banmode(parent, "ban")
		, extbanmgr(parent)
		, redirectlist(parent, "banredirect", ExtensionType::CHANNEL)
	{
	}

	bool BeforeMode(User* source, User* dest, Channel* channel, Modes::Change& change) override
	{
		/* nick!user@host -> nick!user@host
		 * nick!user@host#chan -> nick!user@host#chan
		 * nick@host#chan -> nick!*@host#chan
		 * nick!user#chan -> nick!user@*#chan
		 * nick#chan -> nick!*@*#chan
		 */

		if ((channel) && !change.param.empty())
		{
			BanRedirectList* redirects;

			std::string mask[4];
			enum { NICK, USER, HOST, CHAN } current = NICK;
			std::string::iterator start_pos = change.param.begin();

			std::string unused = change.param;
			if (extbanmgr && extbanmgr->Canonicalize(unused))
				return true; // Skip extbans.

			if (change.param.find('#') == std::string::npos)
				return true;

			ListModeBase* banlm = static_cast<ListModeBase*>(*banmode);
			unsigned long maxbans = banlm->GetLimit(channel);
			ListModeBase::ModeList* list = banlm->GetList(channel);
			if (list && change.adding && maxbans <= list->size())
			{
				source->WriteNumeric(ERR_BANLISTFULL, channel->name, banlm->GetModeChar(), fmt::format("Channel ban list for {} is full (maximum entries for this channel is {})",
					channel->name, maxbans));
				return false;
			}

			for(std::string::iterator curr = start_pos; curr != change.param.end(); curr++)
			{
				switch(*curr)
				{
					case '!':
						if (current != NICK)
							break;
						mask[current].assign(start_pos, curr);
						current = USER;
						start_pos = curr+1;
						break;
					case '@':
						if (current != USER)
							break;
						mask[current].assign(start_pos, curr);
						current = HOST;
						start_pos = curr+1;
						break;
					case '#':
						if (current == CHAN)
							break;
						mask[current].assign(start_pos, curr);
						current = CHAN;
						start_pos = curr;
						break;
				}
			}

			if(mask[current].empty())
			{
				mask[current].assign(start_pos, change.param.end());
			}

			/* nick@host wants to be changed to *!nick@host rather than nick!*@host... */
			if(mask[NICK].length() && mask[HOST].length() && mask[USER].empty())
			{
				/* std::string::swap() is fast - it runs in constant time */
				mask[NICK].swap(mask[USER]);
			}

			if (!mask[NICK].empty() && mask[USER].empty() && mask[HOST].empty())
			{
				if (mask[NICK].find('.') != std::string::npos || mask[NICK].find(':') != std::string::npos)
				{
					mask[NICK].swap(mask[HOST]);
				}
			}

			for(int i = 0; i < 3; i++)
			{
				if(mask[i].empty())
				{
					mask[i].assign("*");
				}
			}

			change.param.assign(mask[NICK]).append(1, '!').append(mask[USER]).append(1, '@').append(mask[HOST]);

			if(mask[CHAN].length())
			{
				if (change.adding && IS_LOCAL(source))
				{
					if (!ServerInstance->Channels.IsChannel(mask[CHAN]))
					{
						source->WriteNumeric(ERR_NOSUCHCHANNEL, channel->name, fmt::format("Invalid channel name in redirection ({})", mask[CHAN]));
						return false;
					}

					auto* c = ServerInstance->Channels.Find(mask[CHAN]);
					if (!c)
					{
						source->WriteNumeric(690, fmt::format("Target channel {} must exist to be set as a redirect.", mask[CHAN]));
						return false;
					}
					else if (change.adding && c->GetPrefixValue(source) < OP_VALUE)
					{
						source->WriteNumeric(690, fmt::format("You must be opped on {} to set it as a redirect.", mask[CHAN]));
						return false;
					}

					if (irc::equals(channel->name, mask[CHAN]))
					{
						source->WriteNumeric(690, channel->name, "You cannot set a ban redirection to the channel the ban is on");
						return false;
					}
				}

				if (change.adding)
				{
					/* It's a properly valid redirecting ban, and we're adding it */
					redirects = redirectlist.Get(channel);
					if (!redirects)
					{
						redirects = new BanRedirectList;
						redirectlist.Set(channel, redirects);
					}
					else
					{
						for (const auto& redirect : *redirects)
						{
							// Mimic the functionality used when removing the mode
							if (irc::equals(redirect.targetchan, mask[CHAN]) && irc::equals(redirect.banmask, change.param))
							{
								// Make sure the +b handler will still set the right ban
								change.param.append(mask[CHAN]);
								// Silently ignore the duplicate and don't set metadata
								// This still allows channel ops to set/unset a redirect ban to clear "ghost" redirects
								return true;
							}
						}
					}

					/* Here 'param' doesn't have the channel on it yet */
					redirects->emplace_back(mask[CHAN], change.param);

					/* Now it does */
					change.param.append(mask[CHAN]);
				}
				else
				{
					/* Removing a ban, if there's no extensible there are no redirecting bans and we're fine. */
					redirects = redirectlist.Get(channel);
					if (redirects)
					{
						/* But there were, so we need to remove the matching one if there is one */

						for(BanRedirectList::iterator redir = redirects->begin(); redir != redirects->end(); redir++)
						{
							if ((irc::equals(redir->targetchan, mask[CHAN])) && (irc::equals(redir->banmask, change.param)))
							{
								redirects->erase(redir);

								if(redirects->empty())
								{
									redirectlist.Unset(channel);
								}

								break;
							}
						}
					}

					/* Append the channel so the default +b handler can remove the entry too */
					change.param.append(mask[CHAN]);
				}
			}
		}

		return true;
	}
};

class ModuleBanRedirect final
	: public Module
{
private:
	BanRedirect banwatcher;
	bool nofollow = false;
	ChanModeReference limitmode;
	ChanModeReference redirectmode;

public:
	ModuleBanRedirect()
		: Module(VF_VENDOR | VF_COMMON, "Allows specifying a channel to redirect a banned user to in the ban mask.")
		, banwatcher(this)
		, limitmode(this, "limit")
		, redirectmode(this, "redirect")
	{
	}

	void OnCleanup(ExtensionType type, Extensible* item) override
	{
		if (type == ExtensionType::CHANNEL)
		{
			Channel* chan = static_cast<Channel*>(item);
			BanRedirectList* redirects = banwatcher.redirectlist.Get(chan);

			if(redirects)
			{
				Modes::ChangeList changelist;

				for (auto& redirect : *redirects)
					changelist.push_remove(*banwatcher.banmode, redirect.targetchan.insert(0, redirect.banmask));

				for (const auto& redirect : *redirects)
					changelist.push_add(*banwatcher.banmode, redirect.banmask);

				ServerInstance->Modes.Process(ServerInstance->FakeClient, chan, nullptr, changelist, ModeParser::MODE_LOCALONLY);
			}
		}
	}

	ModResult OnUserPreJoin(LocalUser* user, Channel* chan, const std::string& cname, std::string& privs, const std::string& keygiven, bool override) override
	{
		if (!override && chan)
		{
			BanRedirectList* redirects = banwatcher.redirectlist.Get(chan);

			if (redirects)
			{
				/* We actually had some ban redirects to check */
				ModResult result;
				FIRST_MOD_RESULT(OnCheckChannelBan, result, (user, chan));
				if (result == MOD_RES_ALLOW)
				{
					// they have a ban exception
					return MOD_RES_PASSTHRU;
				}

				std::string ipmask(user->nick);
				ipmask.append(1, '!').append(user->GetUserAddress());

				for (const auto& redirect : *redirects)
				{
					if (InspIRCd::Match(user->GetRealMask(), redirect.banmask) || InspIRCd::Match(user->GetMask(), redirect.banmask) || InspIRCd::MatchCIDR(ipmask, redirect.banmask))
					{
						/* This prevents recursion when a user sets multiple ban redirects in a chain
						 * (thanks Potter)
						 *
						 * If we're here and nofollow is true then we're already redirecting this user
						 * and there's a redirecting ban set on this channel that matches him, too.
						 * Deny both joins.
						 */
						if (nofollow)
							return MOD_RES_DENY;

						/* tell them they're banned and are being transferred */
						auto* destchan = ServerInstance->Channels.Find(redirect.targetchan);
						std::string destlimit;

						if (destchan)
							destlimit = destchan->GetModeParameter(limitmode);

						if(destchan && destchan->IsModeSet(redirectmode) && !destlimit.empty() && (destchan->GetUsers().size() >= ConvToNum<size_t>(destlimit)))
						{
							user->WriteNumeric(ERR_BANNEDFROMCHAN, chan->name, "Cannot join channel (you're banned)");
							return MOD_RES_DENY;
						}
						else
						{
							user->WriteNumeric(ERR_BANNEDFROMCHAN, chan->name, "Cannot join channel (you're banned)");
							user->WriteNumeric(470, chan->name, redirect.targetchan, "You are banned from this channel, so you are automatically being transferred to the redirected channel.");
							nofollow = true;
							Channel::JoinUser(user, redirect.targetchan);
							nofollow = false;
							return MOD_RES_DENY;
						}
					}
				}
			}
		}
		return MOD_RES_PASSTHRU;
	}
};

MODULE_INIT(ModuleBanRedirect)
