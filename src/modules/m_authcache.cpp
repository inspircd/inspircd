/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "account.h"
#include "ssl.h"

// Password description - method, authdata
typedef std::pair<std::string, std::string> passdesc;
typedef std::multimap<std::string,passdesc> AuthMap;

class ModuleAuthCache : public Module
{
	AuthMap usernames;

 public:
	ModuleAuthCache()
	{
	}

	void init()
	{
		Implementation eventlist[] = { I_OnCheckReady, I_OnDecodeMetaData, I_OnSyncNetwork };
		ServerInstance->Modules->Attach(eventlist, this, 3);
	}

	void OnSyncNetwork(Module* proto, void* opaque)
	{
		for(AuthMap::iterator it = usernames.begin(); it != usernames.end(); it++)
			proto->ProtoSendMetaData(opaque, NULL, "authcache", it->second.first +
				" " + it->first + " " + it->second.second);
	}

	void OnDecodeMetaData(Extensible* dest, const std::string& name, const std::string& value)
	{
		if (dest || name != "authcache")
			return;
		irc::spacesepstream splitter(value);
		std::string method, id, data;
		splitter.GetToken(method);
		splitter.GetToken(id);
		data = splitter.GetRemaining();

		std::pair<AuthMap::iterator, AuthMap::iterator> it = usernames.equal_range(id);
		while (it.first != it.second)
		{
			if (it.first->second.first == method)
			{
				usernames.erase(it.first);
				break;
			}
			it.first++;
		}

		if (!data.empty())
		{
			usernames.insert(std::make_pair(id, std::make_pair(method, data)));
		}
	}

	ModResult OnCheckReady(LocalUser* user)
	{
		std::string login;

		std::pair<AuthMap::iterator, AuthMap::iterator> it = usernames.equal_range(user->nick);
		while (it.first != it.second)
		{
			if (!ServerInstance->PassCompare(user, it.first->second.second, user->password, it.first->second.first))
				login = user->nick;
			it.first++;
		}

		SocketCertificateRequest req(&user->eh, this);
		req.Send();
		if (req.cert)
		{
			it = usernames.equal_range(req.cert->GetFingerprint());
			while (it.first != it.second)
			{
				if (it.first->second.first == "sslfp")
					login = it.first->second.second;
				it.first++;
			}
		}

		if (!login.empty())
		{
			AccountExtItem* ext = GetAccountExtItem();
			if (ext)
			{
				ext->set(user, login);
				AccountEvent(this, user, login).Send();
			}
		}
		return MOD_RES_PASSTHRU;
	}

	Version GetVersion()
	{
		return Version("Provides an authorization cache for user accounts", VF_OPTCOMMON | VF_VENDOR);
	}
};

MODULE_INIT(ModuleAuthCache)
