/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "opflags.h"

class OpFlagProviderImpl : public OpFlagProvider
{
 public:
	LocalStringExt ext;
	OpFlagProviderImpl(Module* parent) : OpFlagProvider(parent, "opflags"), ext(EXTENSIBLE_MEMBERSHIP, "flaglist", parent)
	{
	}

	std::string GetFlags(Membership* memb)
	{
		std::string* v = ext.get(memb);
		if (v)
			return *v;
		return "";
	}

	void SetFlags(Membership* memb, const std::string& flags, bool sendGlobal)
	{
		if (flags.empty())
			ext.unset(memb);
		else
			ext.set(memb, flags);
		if (sendGlobal)
		{
			parameterlist encap;
			encap.push_back("*");
			encap.push_back("OPFLAGS");
			encap.push_back(memb->chan->name);
			encap.push_back(memb->user->uuid);
			encap.push_back(":=" + flags);
			ServerInstance->PI->SendEncapsulatedData(encap);
		}
	}

	std::string SetFlags(Membership* memb, const std::set<std::string>& flags, bool sendGlobal)
	{
		std::string v;
		for(std::set<std::string>::iterator i = flags.begin(); i != flags.end(); i++)
		{
			if (i != flags.begin())
				v.push_back(',');
			v.append(*i);
		}
		SetFlags(memb, v, sendGlobal);
		return v;
	}

	bool PermissionCheck(Membership* memb, const std::string& needed)
	{
		if (!memb || needed.empty())
			return false;

		std::string* mine = ext.get(memb);
		if (!mine)
			return false;

		irc::commasepstream flags(needed);
		std::string flag;

		while (flags.GetToken(flag))
		{
			irc::commasepstream myflags(*mine);
			std::string myflag;
			while (myflags.GetToken(myflag))
			{
				if (flag == myflag)
					return true;
			}
		}
		return false;
	}
};

class FlagCmd : public Command
{
 public:
	OpFlagProviderImpl prov;
	unsigned int conflevel;
	unsigned int usermax;
	FlagCmd(Module* parent) : Command(parent, "OPFLAGS", 1), prov(parent)
	{
		syntax = "<channel> [nick] {+-=}[<flags>]";
		TRANSLATE4(TR_TEXT, TR_NICK, TR_TEXT, TR_END);
	}

	CmdResult Handle(const std::vector<std::string> &parameters, User *src)
	{
		Channel* chan = ServerInstance->FindChan(parameters[0]);
		if (!chan)
		{
			src->WriteNumeric(ERR_NOSUCHNICK, "%s %s :No such nick/channel",
				src->nick.c_str(), parameters[0].c_str());
			return CMD_FAILURE;
		}

		if (parameters.size() == 1)
		{
			std::map<std::string, std::vector<std::string> > opflags;
			std::string tmp;
			for(UserMembCIter i = chan->GetUsers()->begin(); i != chan->GetUsers()->end(); ++i)
			{
				irc::commasepstream flags(prov.GetFlags(i->second));
				while(flags.GetToken(tmp))
					opflags[tmp].push_back(i->first->nick);
			}
			for(std::map<std::string, std::vector<std::string> >::iterator i = opflags.begin(); i != opflags.end(); ++i)
			{
				for(std::vector<std::string>::iterator j = i->second.begin(); j != i->second.end(); ++j)
				{
					if(j == i->second.begin())
						tmp.clear();
					else
						tmp.push_back(',');
					tmp.append(*j);
				}
				src->WriteNumeric(926, "%s %s %s %s :Opflag is held by these users", src->nick.c_str(),
					chan->name.c_str(), i->first.c_str(), tmp.c_str());
			}
			src->WriteNumeric(925, "%s %s :End of channel opflags", src->nick.c_str(), chan->name.c_str());
			return CMD_SUCCESS;
		}

		User* user = ServerInstance->FindNick(parameters[1]);
		if (!user)
		{
			src->WriteNumeric(ERR_NOSUCHNICK, "%s %s :No such nick/channel",
				src->nick.c_str(), parameters[1].c_str());
			return CMD_FAILURE;
		}

		Membership* memb = chan->GetUser(user);
		if (!memb)
			return CMD_FAILURE;
		std::string* ptr = prov.ext.get(memb);

		if (parameters.size() == 2)
		{
			src->WriteServ("NOTICE %s :User %s has %s%s", chan->name.c_str(),
				user->nick.c_str(), ptr ? "opflags " : "no opflags", ptr ? ptr->c_str() : "");
			return CMD_SUCCESS;
		}

		std::string delta = parameters[2];

		if (IS_LOCAL(src))
		{
			ModResult res = ServerInstance->CheckExemption(src,chan,"opflags");
			OpFlagPermissionData perm(src, chan, user, delta);

			if (!perm.result.check(chan->GetAccessRank(src) >= conflevel))
			{
				src->WriteNumeric(ERR_CHANOPRIVSNEEDED, "%s %s :You cannot change opflags on this channel.",
					src->nick.c_str(), chan->name.c_str());
				return CMD_FAILURE;
			}
		}

		std::string flag;
		std::set<std::string> flags;
		if (ptr)
		{
			irc::commasepstream myflags(*ptr);
			while (myflags.GetToken(flag))
				flags.insert(flag);
		}

		bool adding = true;
		irc::commasepstream deltaflags(delta);
		while (deltaflags.GetToken(flag))
		{
			if (flag[0] == '=')
			{
				flags.clear();
				flag = flag.substr(1);
			}
			else if (flag[0] == '+' || flag[0] == '-')
			{
				adding = (flag[0] == '+');
				flag = flag.substr(1);
			}
			if (flag.empty())
				continue;
			if (adding)
				flags.insert(flag);
			else
				flags.erase(flag);
		}
		if (IS_LOCAL(src) && flags.size() > usermax)
		{
			src->WriteNumeric(ERR_CHANOPRIVSNEEDED, "%s %s :Users cannot have more than %d opflags set.",
				src->nick.c_str(), chan->name.c_str(), usermax);
			return CMD_FAILURE;
		}
		if (flags.empty())
		{
			prov.ext.unset(memb);
			if (!IS_SERVER(src))
				chan->WriteChannelWithServ(src->server, "NOTICE %s :%s removed all opflags from %s",
					chan->name.c_str(), src->nick.c_str(), user->nick.c_str());
		}
		else
		{
			std::string v = prov.SetFlags(memb, flags, false);
			if (!IS_SERVER(src))
				chan->WriteChannelWithServ(src->server, "NOTICE %s :%s set %s opflags to %s",
					chan->name.c_str(), src->nick.c_str(), user->nick.c_str(), v.c_str());
		}
		return CMD_SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters)
	{
		if (parameters.size() == 2)
			return ROUTE_LOCALONLY;
		return ROUTE_OPT_BCAST;
	}
};

class ModuleOpFlags : public Module
{
	FlagCmd cmd;
 public:
	ModuleOpFlags() : cmd(this)
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(cmd);
		ServerInstance->Modules->AddService(cmd.prov);
		ServerInstance->Modules->AddService(cmd.prov.ext);

		Implementation eventlist[] = { I_OnSyncChannel, I_OnPermissionCheck };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	void ReadConfig(ConfigReadStatus&)
	{
		ConfigTag* tag = ServerInstance->Config->GetTag("opflags");
		cmd.conflevel = tag->getInt("level", OP_VALUE);
		cmd.usermax = tag->getInt("maxflags", 15);
	}

	void OnSyncChannel(Channel* channel, SyncTarget* target)
	{
		const UserMembList* users = channel->GetUsers();
		for(UserMembCIter u = users->begin(); u != users->end(); u++)
		{
			std::string* ptr = cmd.prov.ext.get(u->second);
			if (ptr)
			{
				parameterlist flags;
				flags.push_back(channel->name);
				flags.push_back(u->first->uuid);
				flags.push_back(*ptr);
				target->SendEncap("OPFLAGS", flags);
			}
		}
	}

	void OnPermissionCheck(PermissionData& perm)
	{
		if (perm.chan && perm.source && perm.result == MOD_RES_PASSTHRU)
		{
			Membership* memb = perm.chan->GetUser(perm.source);
			if (cmd.prov.PermissionCheck(memb, perm.name))
				perm.result = MOD_RES_ALLOW;
		}
	}

	Version GetVersion()
	{
		return Version("Provides per-channel access flags", VF_OPTCOMMON | VF_VENDOR);
	}
};

MODULE_INIT(ModuleOpFlags)
