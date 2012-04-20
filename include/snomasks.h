/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006 Craig Edwards <craigedwards@brainbox.cc>
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


#ifndef __SNOMASKS_H__
#define __SNOMASKS_H__

class Snomask : public Extensible
{
 private:
	InspIRCd *ServerInstance;

 public:
	char MySnomask;
	std::string Description;
	std::string LastMessage;
	bool LastBlocked;
	unsigned int Count;

	/** Create a new Snomask
	 */
	Snomask(InspIRCd* Instance, char snomask, const std::string &description) : ServerInstance(Instance), MySnomask(snomask), Description(description), LastMessage(""), Count(0)
	{
	}

	/** Sends a message to all opers with this snomask.
	 */
	void SendMessage(const std::string &message);

	/** Sends out the (last message repeated N times) message
	 */
	void Flush();
};

/** A list of snomasks which are valid, and their descriptive texts
 */
typedef std::map<char, Snomask *> SnoList;

/** Snomask manager handles routing of SNOMASK (usermode +s) messages to opers.
 * Modules and the core can enable and disable snomask characters. If they do,
 * then sending snomasks using these characters becomes possible.
 */
class CoreExport SnomaskManager : public Extensible
{
 private:
	/** Creator/owner
	 */
	InspIRCd* ServerInstance;

	/** Currently active snomask list
	 */
	SnoList SnoMasks;

	/** Set up the default (core available) snomask chars
	 */
	void SetupDefaults();
 public:
	/** Create a new SnomaskManager
	 */
	SnomaskManager(InspIRCd* Instance);

	/** Delete SnomaskManager
	 */
	~SnomaskManager();

	/** Enable a snomask.
	 * @param letter The snomask letter to enable. Once enabled,
	 * server notices may be routed to users with this letter in
	 * their list, and users may add this letter to their list.
	 * @param description The descriptive text sent along with any
	 * server notices, at the start of the notice, e.g. "GLOBOPS".
	 * @return True if the snomask was enabled, false if it already
	 * exists.
	 */
	bool EnableSnomask(char letter, const std::string &description);

	/** Disable a snomask.
	 * @param letter The snomask letter to disable.
	 * @return True if the snomask was disabled, false if it didn't
	 * exist.
	 */
	bool DisableSnomask(char letter);

	/** Write to all users with a given snomask (local server only)
	 * @param letter The snomask letter to write to
	 * @param text The text to send to the users
	 */
	void WriteToSnoMask(char letter, const std::string &text);

	/** Write to all users with a given snomask (local server only)
	 * @param letter The snomask letter to write to
	 * @param text A format string containing text to send
	 * @param ... Format arguments
	 */
	void WriteToSnoMask(char letter, const char* text, ...) CUSTOM_PRINTF(3, 4);

	/** Write to all users with a given snomask (sent globally)
	 * @param letter The snomask letter to write to
	 * @param text The text to send to the users
	 */
	void WriteGlobalSno(char letter, const std::string &text);

	/** Write to all users with a given snomask (sent globally)
	 * @param letter The snomask letter to write to
	 * @param text A format string containing text to send
	 * @param ... Format arguments
	 */
	void WriteGlobalSno(char letter, const char* text, ...) CUSTOM_PRINTF(3, 4);


	/** Called once per 5 seconds from the mainloop, this flushes any cached
	 * snotices. The way the caching works is as follows:
	 * Calls to WriteToSnoMask write to a cache, if the call is the same as it was
	 * for the previous call, then a count is incremented. If it is different,
	 * the previous message it just sent normally via NOTICE (with count if > 1)
	 * and the new message is cached. This acts as a sender in case the number of notices
	 * is not particularly significant, in order to keep notices going out.
	 */
	void FlushSnotices();

	/** Check if a snomask is enabled.
	 * @param letter The snomask letter to check.
	 * @return True if the snomask has been enabled.
	 */
	bool IsEnabled(char letter);
};

#endif
