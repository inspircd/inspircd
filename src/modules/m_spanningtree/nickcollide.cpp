/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2008 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "xline.h"

#include "m_spanningtree/treesocket.h"
#include "m_spanningtree/treeserver.h"
#include "m_spanningtree/utils.h"

/* $ModDep: m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/treesocket.h */


/*
 * Yes, this function looks a little ugly.
 * However, in some circumstances we may not have a User, so we need to do things this way.
 * Returns 1 if colliding local client, 2 if colliding remote, 3 if colliding both.
 * Sends SVSNICKs as appropriate and forces nickchanges too.
 */
int TreeSocket::DoCollision(User *u, time_t remotets, const std::string &remoteident, const std::string &remoteip, const std::string &remoteuid)
{
	/*
	 *  Under old protocol rules, we would have had to kill both clients.
	 *  Really, this sucks.
	 * These days, we have UID. And, so what we do is, force nick change client(s)
	 * involved according to timestamp rules.
	 *
	 * RULES:
	 *  user@ip equal:
	 *   Force nick change on OLDER timestamped client
	 *  user@ip differ:
	 *   Force nick change on NEWER timestamped client
	 *  TS EQUAL:
	 *   FNC both.
	 *
	 * This stops abusive use of collisions, simplifies problems with loops, and so on.
	 *   -- w00t
	 */
	bool bChangeLocal = true;
	bool bChangeRemote = true;

	/* for brevity, don't use the User - use defines to avoid any copy */
	#define localts u->age
	#define localident u->ident
	#define localip u->GetIPString()

	/* mmk. let's do this again. */
	if (remotets == localts)
	{
		/* equal. fuck them both! do nada, let the handler at the bottom figure this out. */
	}
	else
	{
		/* fuck. now it gets complex. */

		/* first, let's see if ident@host matches. */
		bool SamePerson = (localident == remoteident)
				&& (localip == remoteip);

		/*
		 * if ident@ip is equal, and theirs is newer, or
		 * ident@ip differ, and ours is newer
		 */
		if((SamePerson && remotets < localts) ||
		   (!SamePerson && remotets > localts))
		{
			/* remote needs to change */
			bChangeLocal = false;
		}
		else
		{
			/* ours needs to change */
			bChangeRemote = false;
		}
	}


	if (bChangeLocal)
	{
		u->ForceNickChange(u->uuid.c_str());

		if (!bChangeRemote)
			return 1;
	}
	if (bChangeRemote)
	{
		/*
		 * Cheat a little here. Instead of a dedicated command to change UID,
		 * use SVSNICK and accept their client with it's UID (as we know the SVSNICK will
		 * not fail under any circumstances -- UIDs are netwide exclusive).
		 *
		 * This means that each side of a collide will generate one extra NICK back to where
		 * they have just linked (and where it got the SVSNICK from), however, it will
		 * be dropped harmlessly as it will come in as :928AAAB NICK 928AAAB, and we already
		 * have 928AAAB's nick set to that.
		 *   -- w00t
		 */
		User *remote = this->Instance->FindUUID(remoteuid);

		if (remote)
		{
			/* buh.. nick change collide. force change their nick. */
			remote->ForceNickChange(remote->uuid.c_str());
		}
		else
		{
			/* user has not been introduced yet, just inform their server */
			this->WriteLine(std::string(":")+this->Instance->Config->GetSID()+" SVSNICK "+remoteuid+" " + remoteuid + " " + ConvToStr(remotets));
		}

		if (!bChangeLocal)
			return 2;
	}

	return 3;
}

