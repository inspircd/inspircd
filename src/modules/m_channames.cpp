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

/* $ModDesc: Implements config tags which allow changing characters allowed in channel names */

static bool allowedmap[256];

class NewIsChannelHandler : public HandlerBase2<bool, const char*, size_t>
{
 public:
	NewIsChannelHandler() { }
	virtual ~NewIsChannelHandler() { }
	virtual bool Call(const char*, size_t);
};

bool NewIsChannelHandler::Call(const char* chname, size_t max)
{
		const char *c = chname + 1;

		/* check for no name - don't check for !*chname, as if it is empty, it won't be '#'! */
		if (!chname || *chname != '#')
			return false;

		while (*c)
			if (!allowedmap[(unsigned int)(*c++)]) return false;

		size_t len = c - chname;
		return len <= max;
}

class ModuleChannelNames : public Module
{
 private:
	NewIsChannelHandler myhandler;
	caller2<bool, const char*, size_t> rememberer;
	bool badchan;

 public:
	ModuleChannelNames() : rememberer(ServerInstance->IsChannel)
	{
		ServerInstance->IsChannel = &myhandler;
		badchan = false;
		Implementation eventlist[] = { I_OnRehash, I_OnUserKick };
		ServerInstance->Modules->Attach(eventlist, this, 2);
		OnRehash(NULL);
	}

	void ValidateChans()
	{
		badchan = true;
		std::vector<Channel*> chanvec;
		for (chan_hash::const_iterator i = ServerInstance->chanlist->begin(); i != ServerInstance->chanlist->end(); ++i)
		{
			if (!ServerInstance->IsChannel(i->second->name.c_str(), MAXBUF))
				chanvec.push_back(i->second);
		}
		std::vector<Channel*>::reverse_iterator c2 = chanvec.rbegin();
		while (c2 != chanvec.rend())
		{
			Channel* c = *c2++;
			if (c->IsModeSet('P') && c->GetUserCounter())
			{
				std::vector<std::string> modes;
				modes.push_back(c->name);
				modes.push_back("-P");

				ServerInstance->SendMode(modes, ServerInstance->FakeClient);
				ServerInstance->PI->SendMode(c->name, ServerInstance->Modes->GetLastParseParams(), ServerInstance->Modes->GetLastParseTranslate());
			}
			const UserMembList* users = c->GetUsers();
			for(UserMembCIter j = users->begin(); j != users->end(); ++j)
				if (IS_LOCAL(j->first))
					c->KickUser(ServerInstance->FakeClient, j->first, "Channel name no longer valid");
		}
		badchan = false;
	}

	virtual void OnRehash(User* user)
	{
		ConfigReader Conf;
		std::string denyToken = Conf.ReadValue("channames", "denyrange", 0);
		std::string allowToken = Conf.ReadValue("channames", "allowrange", 0);
		memset(allowedmap, 1, sizeof(allowedmap));

		irc::portparser denyrange(denyToken, false);
		int denyno = -1;
		while (0 != (denyno = denyrange.GetToken()))
			allowedmap[(unsigned char)(denyno)] = false;

		irc::portparser allowrange(allowToken, false);
		int allowno = -1;
		while (0 != (allowno = allowrange.GetToken()))
			allowedmap[(unsigned char)(allowno)] = true;

		allowedmap[7] = false;
		allowedmap[' '] = false;
		allowedmap[','] = false;

		ValidateChans();
	}

	virtual void OnUserKick(User* source, Membership* memb, const std::string &reason, CUList& except_list)
	{
		if (badchan)
		{
			const UserMembList* users = memb->chan->GetUsers();
			for(UserMembCIter i = users->begin(); i != users->end(); i++)
				if (i->first != memb->user)
					except_list.insert(i->first);
		}
	}

	virtual ~ModuleChannelNames()
	{
		ServerInstance->IsChannel = rememberer;
		ValidateChans();
	}

	virtual Version GetVersion()
	{
		return Version("Implements config tags which allow changing characters allowed in channel names", VF_VENDOR);
	}
};

MODULE_INIT(ModuleChannelNames)
