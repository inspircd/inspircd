/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *                <Craig@chatspike.net>
 *                <omster@gmail.com>
 *
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */
 
#ifndef INSPIRCD_HASHMAP_H
#define INSPIRCD_HASHMAP_H

#include "inspircd_config.h"

#ifdef GCC3
#include <ext/hash_map>
#define nspace __gnu_cxx
#else
#include <hash_map>
#define nspace std
#endif

#endif
