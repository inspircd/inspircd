/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  Inspire is copyright (C) 2002-2003 ChatSpike-Dev.
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
#include "inspircd_config.h"
#include "inspircd.h"
#include "base.h"

#ifndef __CTABLES_H__
#define __CTABLES_H__

typedef void (handlerfunc) (char**, int, userrec*);

/* a structure that defines a command */

class command_t : public classbase
{
 public:
	char command[MAXBUF]; /* command name */
	handlerfunc *handler_function; /* handler function as in typedef */
	char flags_needed; /* user flags needed to execute the command or 0 */
	int min_params; /* minimum number of parameters command takes */
	long use_count; /* used by /stats m */
	long total_bytes; /* used by /stats m */
};

#endif

