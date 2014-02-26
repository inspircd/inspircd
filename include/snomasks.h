/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2014 Adam <Adam@anope.org>
 *   Copyright (C) 2010 Daniel De Graaf <danieldg@inspircd.org>
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


#pragma once

class Snomasks
{
	std::vector<bool> snomasks;

 public:
	std::vector<bool>::reference operator[](size_t idx);
	bool operator[](size_t idx) const;
	bool none() const;
	inline size_t size() const { return snomasks.size(); }
};

class SnomaskManager;
class CoreExport Snomask
{
	/** Information about the last sent message,
	 * used for sending "last message repeated X times" messages
	 */
	std::string LastMessage;
	bool LastRemote;
	unsigned int Count;

	/** Log and send a message to all opers who have the given snomask set
	 * @param letter The target users of this message
	 * @param desc The description of this snomask, will be prepended to the message
	 * @param msg The message to send
	 */
	void Send(bool remote, const std::string& desc, const std::string& msg);

 public:
	std::string name;
	unsigned int pos;

	/** Create a new Snomask
	 */
	Snomask(const std::string &name);
	~Snomask();

	/** Sends a message to all opers with this snomask.
	 * @param message The message to send
	 * @param remote If true the message will go to the uppercase variant of this snomask
	 */
	void SendMessage(const std::string& message, bool remote);

	/** Sends out the (last message repeated N times) message
	 */
	void Flush();

	friend class SnomaskManager;
};

enum
{
	SNO_LOCAL,
	SNO_REMOTE,
	SNO_BROADCAST
};

/** Snomask manager handles routing of SNOMASK (usermode +s) messages to opers.
 * Modules and the core can enable and disable snomask characters. If they do,
 * then sending snomasks using these characters becomes possible.
 */
class CoreExport SnomaskManager
{
	static unsigned int pos;
	static std::map<std::string, Snomask *> snomasksByName;
	static std::map<unsigned int, Snomask *> snomasksByPos;

 public:
	static Snomask connect, quit, kill, oper, announcement, debug, xline, stats;

	static void RegisterSnomask(Snomask *);
	static void UnregisterSnomask(Snomask *);

	static Snomask* FindSnomaskByName(const std::string &name);
	static Snomask* FindSnomaskByPos(unsigned int);
	static std::vector<Snomask*> GetSnomasks();

	static void Write(int where, Snomask &sno, const std::string &text);
	static void Write(int where, Snomask &sno, const char* text, ...) CUSTOM_PRINTF(3, 4);

	/** Called once per 5 seconds from the mainloop, this flushes any cached
	 * snotices. The way the caching works is as follows:
	 * Calls to WriteToSnoMask write to a cache, if the call is the same as it was
	 * for the previous call, then a count is incremented. If it is different,
	 * the previous message it just sent normally via NOTICE (with count if > 1)
	 * and the new message is cached. This acts as a sender in case the number of notices
	 * is not particularly significant, in order to keep notices going out.
	 */
	static void FlushSnotices();
};
