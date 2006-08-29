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

typedef std::map<char, std::string> SnoList;

class SnomaskManager : public Extensible
{
 private:
	InspIRCd* ServerInstance;
	SnoList SnoMasks;
	void SetupDefaults();
 public:
	SnomaskManager(InspIRCd* Instance);
	~SnomaskManager();

	bool EnableSnomask(char letter, const std::string &description);
	bool DisableSnomask(char letter);
	void WriteToSnoMask(char letter, const std::string &text);
	void WriteToSnoMask(char letter, const char* text, ...);
	bool IsEnabled(char letter);
};

#endif
