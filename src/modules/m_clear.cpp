/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
/* $MODDESC: provides commands to clear a given list mode or kick all users */

class ClearBase : public Command
{
	public:
	ClearBase (Module *me, const std::string &cmd, char flags) : Command (me, cmd, 2, 3)
	{
		flags_needed = flags;
		syntax = "<channel> {users [reason] || +<modeletter> [glob pattern] || <modename> [glob pattern]}";
	}
	/* this access check returns -1 if user doesn't have access, 0 if he does and if further access checks must be skipped, 1 if user has access to 
	the command but kick and mode accesses must be checked */
	virtual int CheckAccess (User *who, Channel *chan) = 0;
	/* function to do things after the clear, like notifying ircops in saclear */
	virtual void PostClear (User *user, Channel *chan, const std::string type)
	{
	}
	/* make the user user clear the mode pointed to by mh on channel chan */
	void ListClear (User *user, Channel *chan, ModeHandler *mh, std::string glob, bool accesscheck)
	{
		/* create the modestacker and fill it with mode changes to be made */
		irc::modestacker ms;
		const modelist *list = mh->GetList (chan);
		const bool matchAll = glob.empty() || (glob == "*");
		if (!list)
			return;
		for (modelist::const_iterator it = list->begin ( ); it != list->end ( ); it++)
			if(matchAll || InspIRCd::Match(it->mask, glob))
				ms.push (irc::modechange (mh->id, it->mask, false));
		/* commit all mode changes and process them, calling handlers and skipping acls, it will update lists */
		ServerInstance->Modes->Process (user, chan, ms, false, !accesscheck);
		/* then, send information about all mode changes to users in the channel to make clients know it */
		ServerInstance->Modes->Send (user, chan, ms);
		/* now, send all mode changes to remaining irc servers, that will make servers also process and send modes */
		ServerInstance->PI->SendMode (user, chan, ms);
		/* write notice */
		if (!mh->GetList (chan))
			chan->WriteChannel (ServerInstance->FakeClient, "NOTICE %s :%s cleared the %s list mode",
				chan->name.c_str(), user->GetFullHost().c_str(), mh->name.c_str());
	}
	/* command handler */
	CmdResult Handle (const std::vector<std::string> &params, User *user)
	{
		std::string reason;
		if (params.size() == 3)
			reason.assign(params[2], 0, ServerInstance->Config->Limits.MaxKick);
		else
			reason = user->nick;
		std::string type = params[1];
		Channel *chan = ServerInstance->FindChan (params[0]);
		std::string channame = params[0];
		if (!chan)
		{
			user->WriteNumeric (ERR_NOSUCHCHANNEL, "%s %s :No such nick/channel", user->nick.c_str ( ), channame.c_str ( ));
			return CMD_FAILURE;
		}
		/* make the access check */
		int acc = CheckAccess (user, chan);
		if (acc == -1)
			return CMD_FAILURE;
		/* this depends on what the user wanted to clear */
		if (irc::string (type) == "USERS")
		{
			/* the given user tries to empty the channel */
			/* send a notice to the whole channel */
			chan->WriteChannel (ServerInstance->FakeClient, "NOTICE %s :This channel is being cleared by %s",
				channame.c_str(), user->GetFullHost().c_str());
			/* get each channel user and kick him from the channel */
			/* if access checks are to be skipped, use FakeClient as source */
			User *fuser;
			if (acc == 0)
				fuser = ServerInstance->FakeClient;
			else
				fuser = user;
			/* kick them */
			UserMembList memblist = *chan->GetUsers ( );
			for (UserMembList::iterator it = memblist.begin ( ); it != memblist.end ( ); it++)
			{
				chan->KickUser (fuser, it->first, reason);
			}
		}
		else if (type[0] == '+')
		{
			/* it is if the user gave a mode letter */
			if (type.size() < 2)
			{
				/* that is called when the user typed + and nothing more, invalid param */
				user->WriteServ ("NOTICE %s :You must provide a mode letter for a list mode to clear", user->nick.c_str ( ));
				return CMD_FAILURE;
			}
			/* find the list mode */
			ModeHandler *mh = ServerInstance->Modes->FindMode (type[1], MODETYPE_CHANNEL);
			if (!mh)
			{
				user->WriteNumeric (472, "%s %c :is unknown mode char to me", user->nick.c_str ( ), type[1]);
				return CMD_FAILURE;
			}
			else if (!mh->IsListMode ( ))
			{
				user->WriteServ ("NOTICE %s :the %s mode is not a list mode", user->nick.c_str ( ), mh->name.c_str ( ));
				return CMD_FAILURE;
			}
			/* clear the list mode */
			ListClear (user, chan, mh, params.size() == 3 ? params[2] : "", acc == 1);
		}
		else
		{
			/* the mode was given as a name */
			/* find it */
			ModeHandler *mh = ServerInstance->Modes->FindMode (type);
			if (!mh)
			{
				user->WriteNumeric (472, "%s %s :is unknown mode string to me", user->nick.c_str ( ), type.c_str ( ));
				return CMD_FAILURE;
			}
			else if (!mh->IsListMode ( ))
			{
				user->WriteServ ("NOTICE %s :the %s mode is not a list mode", user->nick.c_str ( ), type.c_str ( ));
				return CMD_FAILURE;
			}
			/* clear the list mode */
			ListClear (user, chan, mh, params.size() == 3 ? params[2] : "", acc == 1);
		}
		PostClear (user, chan, type);
		return CMD_SUCCESS;
	}
};

class SaclearCommand : public ClearBase
{
        public:
        SaclearCommand (Module *me) : ClearBase (me, "SACLEAR", 'o')
        {

        }
        /* access checks, really used for server noticing, not for real access checks */
        int CheckAccess (User *user, Channel *chan)
        {
                /* the user has permissions to use this command and doesn't need to be on a channel, so */
		/* make it to skip all access checks */
		return 0;
	}
 /* post clear hook */
	void PostClear (User *user, Channel *chan, const std::string type)
	{
		/* send notice */
		ServerInstance->SNO->WriteGlobalSno ('a', "%s used saclear %s on channel %s", user->nick.c_str ( ), type.c_str ( ), chan->name.c_str ( 
));
	}
};

class ClearCommand : public ClearBase
{
	public:
	ClearCommand (Module *me) : ClearBase (me, "CLEAR", 0)
	{

	}
	int CheckAccess (User *user, Channel *chan)
	{
		if (!chan->HasUser (user))
		{
			user->WriteNumeric (ERR_NOTONCHANNEL, "%s %s :you're not on that channel", user->nick.c_str ( ), chan->name.c_str ( ));
			return -1;
		}
		/* allow using the command with access checking */
		return 1;
	}
};

class ClearModule : public Module
{
	SaclearCommand saclearcmd;
	ClearCommand clearcmd;
 public:
	ClearModule ( ) : saclearcmd (this), clearcmd (this)
	{
	}

	Version GetVersion ( )
	{
		return Version("Provides commands to clear a given list mode or kick all users", VF_VENDOR);
	}
	void init ( )
	{
		ServerInstance->Modules->AddService (saclearcmd);
		ServerInstance->Modules->AddService (clearcmd);
	}
};

MODULE_INIT(ClearModule)
