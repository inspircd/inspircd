/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 Sadie Powell <sadie@witchery.services>
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

/// $ModAuthor: Sadie Powell
/// $ModAuthorMail: sadie@witchery.services
/// $ModConfig: <zombie maxzombies="100" serverzombietime="5m" cleansplit="no" dirtysplit="yes">
/// $ModDepends: core 3
/// $ModDesc: Provides support for zombifying users who have split because of a network issue.


#include "inspircd.h"
#include "modules/server.h"

typedef insp::flat_map<std::string, std::vector<std::string> > NeighbourMap;

struct ZombieUser
{
	// The nick used by the zombie.
	std::string nick;

	// The username (ident) used by the zombie.
	std::string user;

	// The display hostname used by the zombie.
	std::string host;

	// The neighbors that can see this zombie.
	NeighbourMap neighbors;
};

typedef insp::flat_map<std::string, ZombieUser*> ZombieUserMap;

class ZombieTimer
	: public Timer
{
 public:
	// Whether this timer is dead.
	bool dead;

	// The zombie users who are presently visible.
	ZombieUserMap users;

	// The id of the server this timer is waiting on.
	const std::string sid;

	ZombieTimer(const Server* server, unsigned duration)
		: Timer(duration)
		, dead(false)
		, sid(server->GetId())
	{
	}

	void Cleanup()
	{
		for (ZombieUserMap::iterator uiter = users.begin(); uiter != users.end(); )
		{
			// Check that the user still exists.
			User* user = ServerInstance->FindUUID(uiter->first);
			if (!user || !irc::equals(user->nick, uiter->second->nick))
			{
				ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Expiring %s as the user no longer exists!", uiter->first.c_str());
				SendQuit(uiter->second);
			}

			delete uiter->second;
			uiter = users.erase(uiter);
		}
	}

	void MarkAsZombie(User* user)
	{
		NeighbourMap neighbors;
		std::map<User*, bool> exceptions;
		IncludeChanList include_chans;
		for (User::ChanList::const_iterator citer = user->chans.begin(); citer != user->chans.end(); ++citer)
		{
			exceptions.clear();
			include_chans.clear();
			include_chans.push_back(*citer);

			FOREACH_MOD(OnBuildNeighborList, (user, include_chans, exceptions));
			const already_sent_t newid = ServerInstance->Users.NextAlreadySentId();

			// Handle special users.
			for (std::map<User*, bool>::const_iterator eiter = exceptions.begin(); eiter != exceptions.end(); ++eiter)
			{
				LocalUser* luser = IS_LOCAL(eiter->first);
				if (!luser)
					continue;

				luser->already_sent = newid;
				if (eiter->second)
					neighbors[luser->uuid].push_back((*citer)->chan->name.c_str());
			}

			// Handle normal users.
			const Channel::MemberMap& userlist = (*citer)->chan->GetUsers();
			for (Channel::MemberMap::const_iterator miter = userlist.begin(); miter != userlist.end(); ++miter)
			{
				LocalUser* luser = IS_LOCAL(miter->first);
				if (luser && luser->already_sent != newid)
				{
					neighbors[luser->uuid].push_back((*citer)->chan->name.c_str());
					luser->already_sent = newid;
				}
 			}
		}

		if (neighbors.empty())
		{
			ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Not marking %s as a zombie as nobody can see them",
				user->uuid.c_str());
			return;
		}

		ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Marking %s as a zombie until %s with %lu neighbors in %lu channels",
			user->uuid.c_str(), InspIRCd::TimeString(GetTrigger()).c_str(), neighbors.size(), user->chans.size());
		ZombieUser* zombie = new ZombieUser();
		zombie->nick = user->nick;
		zombie->user = user->ident;
		zombie->host = user->GetDisplayedHost();
		std::swap(zombie->neighbors, neighbors);
		users.insert(std::make_pair(user->uuid, zombie));
	}

	void SendQuit(ZombieUser* user)
	{
		static std::string quitsrc;
		quitsrc.assign(user->nick + "!" + user->user + "@" + user->host);

		ClientProtocol::Message quitmsg("QUIT", quitsrc);
		quitmsg.PushParam("Zombie connection timed out");

		ClientProtocol::Event quitevent(ServerInstance->GetRFCEvents().quit, quitmsg);
		for (NeighbourMap::const_iterator niter = user->neighbors.begin(); niter != user->neighbors.end(); ++niter)
		{
			LocalUser* luser = IS_LOCAL(ServerInstance->FindUUID(niter->first));
			if (!luser)
				continue;

			// Attempt to find any common channels.
			bool found_chan = false;
			for (User::ChanList::const_iterator citer = luser->chans.begin(); citer != luser->chans.end(); ++citer)
			{
				Channel* chan = (*citer)->chan;
				if (stdalgo::isin(niter->second, chan->name))
				{
					found_chan = true;
					break;
				}
			}

			// If we didn't find any common channels then we don't need to send a quit.
			if (found_chan)
				luser->Send(quitevent);
		}
	}

	bool Tick(time_t currtime) CXX11_OVERRIDE
	{
		ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Server %s timed out; cleaning up dead sessions", sid.c_str());
		Cleanup();
		dead = true;
		return false;
	}

};

typedef insp::flat_map<std::string, ZombieTimer*> ZombieServerMap;

class JoinHook
	: public ClientProtocol::EventHook
{
 private:
	// The timers for servers which have split recently.
	ZombieServerMap& servers;

 public:
	JoinHook(Module* mod, ZombieServerMap& Servers)
		: ClientProtocol::EventHook(mod, "JOIN", 25)
		, servers(Servers)
	{
	}

	ModResult OnPreEventSend(LocalUser* user, const ClientProtocol::Event& ev, ClientProtocol::MessageList& messagelist) CXX11_OVERRIDE
	{
		const ClientProtocol::Events::Join& join = static_cast<const ClientProtocol::Events::Join&>(ev);

		User* joiner = join.GetMember()->user;
		for (ZombieServerMap::iterator siter = servers.begin(); siter != servers.end(); ++siter)
		{
			ZombieTimer* server = siter->second;
			for (ZombieUserMap::iterator uiter = server->users.begin(); uiter != server->users.end(); ++uiter)
			{
				if (!irc::equals(uiter->second->nick, joiner->nick))
					continue;

				// Check whether its the same joiner.
				if (uiter->first == joiner->uuid)
					return MOD_RES_DENY;

				server->SendQuit(uiter->second);
				delete uiter->second;

				server->users.erase(uiter);
				return MOD_RES_PASSTHRU;
			}
		}

		return MOD_RES_PASSTHRU;
	}
};

class QuitHook
	: public ClientProtocol::EventHook
{
 private:
	// The timers for servers which have split recently.
	ZombieServerMap& servers;

 public:
	QuitHook(Module* mod, ZombieServerMap& Servers)
		: ClientProtocol::EventHook(mod, "QUIT", 25)
		, servers(Servers)
	{
	}

	ModResult OnPreEventSend(LocalUser* user, const ClientProtocol::Event& ev, ClientProtocol::MessageList& messagelist) CXX11_OVERRIDE
	{
		if (messagelist.empty())
			return MOD_RES_PASSTHRU;

		User* quitter = messagelist.front()->GetSourceUser();
		if (!quitter)
			return MOD_RES_PASSTHRU;

		ZombieServerMap::iterator iter = servers.find(quitter->server->GetId());
		if (iter == servers.end())
			return MOD_RES_PASSTHRU;

		iter->second->MarkAsZombie(quitter);
		return MOD_RES_DENY;
	}
};

class ModuleZombie
	: public Module
	, public ServerProtocol::LinkEventListener
{
private:
	ZombieServerMap servers;
	JoinHook joinhook;
	QuitHook quithook;
	unsigned int maxzombies;
	unsigned int zombietime;
	bool cleansplit;
	bool dirtysplit;

 public:
	using ServerProtocol::LinkEventListener::OnServerSplit;

	ModuleZombie()
		: ServerProtocol::LinkEventListener(this)
		, joinhook(this, servers)
		, quithook(this, servers)
	{
	}

	void ReadConfig(ConfigStatus&) CXX11_OVERRIDE
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("zombie");
		maxzombies = tag->getUInt("maxzombies", 100, 1, 250);
		zombietime = tag->getDuration("serverzombietime", 60, 10, 5*60);
		cleansplit = tag->getBool("cleansplit", true);
		dirtysplit = tag->getBool("dirtysplit", true);
	}

	void OnBackgroundTimer(time_t) CXX11_OVERRIDE
	{
		// Clean up any dead timers.
		for (ZombieServerMap::iterator siter = servers.begin(); siter != servers.end(); )
		{
			if (siter->second->dead)
			{
				delete siter->second;
				siter = servers.erase(siter);
			}
			else
				siter++;
		}
	}

	void OnServerBurst(const Server* server) CXX11_OVERRIDE
	{
		ZombieServerMap::iterator siter = servers.find(server->GetId());
		if (siter == servers.end())
			return;

		// This server is no longer a zombie.
		ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Server %s reconnected; cleaning up dead sessions",
			server->GetId().c_str());
		ServerInstance->Timers.DelTimer(siter->second);
		siter->second->Cleanup();

		delete siter->second;
		servers.erase(siter);
	}

	void OnServerSplit(const Server* server, bool error) CXX11_OVERRIDE
	{
		if (error && !dirtysplit)
			return;

		if (!error && !cleansplit)
			return;

		if (maxzombies)
		{
			ProtocolInterface::ServerList sl;
			ServerInstance->PI->GetServerList(sl);

			for (ProtocolInterface::ServerList::const_iterator s = sl.begin(); s != sl.end(); ++s)
			{
				if (s->servername == server->GetName())
				{
					// Does the server have too many users to zombify? If so
					// then we return early to avoid using too much memory.
					if (s->usercount > maxzombies)
						return;

					// We found the server and its below the maxzombie count.
					break;
				}
			}
		}

		ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Marking server %s as a zombie", server->GetName().c_str());
		ZombieTimer* timer = new ZombieTimer(server, zombietime);
		ServerInstance->Timers.AddTimer(timer);
		servers.insert(std::make_pair(server->GetId(), timer));
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides support for zombifying users who have split because of a network partition.", VF_OPTCOMMON);
	}
};

MODULE_INIT(ModuleZombie)
