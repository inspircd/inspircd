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

/** A structure that defines a command
 */
class command_t : public classbase
{
 public:
    /** Command name
     */
	char command[MAXBUF];
	/** Handler function as in typedef
     */
	handlerfunc *handler_function; 
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
};

#endif

