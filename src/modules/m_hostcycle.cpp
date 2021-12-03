/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017-2018 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013-2015, 2018 Attila Molnar <attilamolnar@hush.com>
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
#include "modules/cap.h"

class ModuleHostCycle final
	: public Module
{
	Cap::Reference chghostcap;
	const std::string quitmsghost;
	const std::string quitmsgident;

	/** Send fake quit/join/mode messages for host or ident cycle.
	 */
	void DoHostCycle(User* user, const std::string& newident, const std::string& newhost, const std::string& reason)
	{
		// The user has the original ident/host at the time this function is called
		ClientProtocol::Messages::Quit quitmsg(user, reason);
		ClientProtocol::Event quitevent(ServerInstance->GetRFCEvents().quit, quitmsg);

		uint64_t silent_id = ServerInstance->Users.NextAlreadySentId();
		uint64_t seen_id = ServerInstance->Users.NextAlreadySentId();

		IncludeChanList include_chans(user->chans.begin(), user->chans.end());
		std::map<User*,bool> exceptions;

		FOREACH_MOD(OnBuildNeighborList, (user, include_chans, exceptions));

		// Users shouldn't see themselves quitting when host cycling
		exceptions.erase(user);
		for (const auto& [exception, sendto] : exceptions)
		{
			LocalUser* u = IS_LOCAL(exception);
			if ((u) && (!u->quitting) && (!chghostcap.IsEnabled(u)))
			{
				if (sendto)
				{
					u->already_sent = seen_id;
					u->Send(quitevent);
				}
				else
				{
					u->already_sent = silent_id;
				}
			}
		}

		std::string newfullhost = user->nick + "!" + newident + "@" + newhost;

		for (auto* memb : include_chans)
		{
			Channel* c = memb->chan;
			ClientProtocol::Events::Join joinevent(memb, newfullhost);

			for (const auto& [chanuser, _] : c->GetUsers())
			{
				LocalUser* u = IS_LOCAL(chanuser);
				if (!u || u == user)
					continue;
				if (u->already_sent == silent_id)
					continue;
				if (chghostcap.IsEnabled(u))
					continue;

				if (u->already_sent != seen_id)
				{
					u->Send(quitevent);
					u->already_sent = seen_id;
				}

				u->Send(joinevent);
			}
		}
	}

 public:
	ModuleHostCycle()
		: Module(VF_VENDOR, "Sends a fake disconnection and reconnection when a user's username (ident) or hostname changes to allow clients to update their internal caches.")
		, chghostcap(this, "chghost")
		, quitmsghost("Changing host")
		, quitmsgident("Changing ident")
	{
	}

	void OnChangeIdent(User* user, const std::string& newident) override
	{
		DoHostCycle(user, newident, user->GetDisplayedHost(), quitmsgident);
	}

	void OnChangeHost(User* user, const std::string& newhost) override
	{
		DoHostCycle(user, user->ident, newhost, quitmsghost);
	}
};

MODULE_INIT(ModuleHostCycle)
