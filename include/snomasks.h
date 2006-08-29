/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *                <Craig@chatspike.net>
 *
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * --------------------------------------------------
 */

#ifndef __SNOMASKS_H__
#define __SNOMASKS_H__

#include <string>
#include <vector>
#include <map>
#include "configreader.h"
#include "inspircd.h"

/** A list of snomasks which are valid, and their descriptive texts
 */
typedef std::map<char, std::string> SnoList;

/** Snomask manager handles routing of SNOMASK (usermode +n) messages to opers.
 * Modules and the core can enable and disable snomask characters. If they do,
 * then sending snomasks using these characters becomes possible.
 */
class SnomaskManager : public Extensible
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
	/** Write to all users with a given snomask.
	 * @param letter The snomask letter to write to
	 * @param text The text to send to the users
	 */
	void WriteToSnoMask(char letter, const std::string &text);
	/** Write to all users with a given snomask.
	 * @param letter The snomask letter to write to
	 * @param text A format string containing text to send
	 * @param ... Format arguments
	 */
	void WriteToSnoMask(char letter, const char* text, ...);
	/** Check if a snomask is enabled.
	 * @param letter The snomask letter to check.
	 * @return True if the snomask has been enabled.
	 */
	bool IsEnabled(char letter);
};

#endif
