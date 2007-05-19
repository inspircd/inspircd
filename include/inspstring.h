/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef __IN_INSPSTRING_H
#define __IN_INSPSTRING_H

#include "inspircd_config.h"
#include <string.h>
#include <cstddef>

#ifndef HAS_STRLCPY
CoreExport size_t strlcpy(char *dst, const char *src, size_t siz);
CoreExport size_t strlcat(char *dst, const char *src, size_t siz);
#endif

CoreExport int charlcat(char* x,char y,int z);
CoreExport bool charremove(char* mp, char remove);
inline char * strnewdup(const char * s1)
{
	size_t len = strlen(s1) + 1;
	char * p = new char[len];
	memcpy(p, s1, len);
	return p;
}

#endif
