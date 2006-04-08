/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *           	  <Craig@chatspike.net>
 *     
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */
 
#ifndef __CTABLES_H__
#define __CTABLES_H__

#include "inspircd_config.h"
#include "hash_map.h"

class userrec;

/*typedef void (handlerfunc) (char**, int, userrec*);*/

/** A structure that defines a command
 */
class command_t
{
 public:
	/** Command name
	*/
	 std::string command;
	/** User flags needed to execute the command or 0
	 */
	char flags_needed;
	/** Minimum number of parameters command takes
	*/
	int min_params;
	/** used by /stats m
	 */
	long use_count;
	/** used by /stats m
 	 */
	long total_bytes;
	/** used for resource tracking between modules
	 */
	std::string source;

	command_t(const std::string &cmd, char flags, int minpara) : command(cmd), flags_needed(flags), min_params(minpara)
	{
		use_count = total_bytes = 0;
		source = "<core>";
	}

	virtual void Handle(char** parameters, int pcnt, userrec* user) = 0;

	virtual ~command_t() {}
};

typedef nspace::hash_map<std::string,command_t*> command_table;

#endif
