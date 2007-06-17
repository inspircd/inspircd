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
/** strlcpy() implementation for systems that don't have it (linux) */
CoreExport size_t strlcpy(char *dst, const char *src, size_t siz);
/** strlcat() implementation for systems that don't have it (linux) */
CoreExport size_t strlcat(char *dst, const char *src, size_t siz);
#endif

/** charlcat() will append one character to a string using the same
 * safety scemantics as strlcat().
 * @param x The string to operate on
 * @param y the character to append to the end of x
 * @param z The maximum allowed length for z including null terminator
 */
CoreExport int charlcat(char* x,char y,int z);
/** charremove() will remove all instances of a character from a string
 * @param mp The string to operate on
 * @param remove The character to remove
 */
CoreExport bool charremove(char* mp, char remove);

/** strnewdup() is an implemenetation of strdup() which calls operator new
 * rather than malloc to allocate the new string, therefore allowing it to
 * be hooked into the C++ memory manager, and freed with operator delete.
 * This is required for windows, where we override operators new and delete
 * to allow for global allocation between modules and the core.
 */
inline char * strnewdup(const char * s1)
{
	size_t len = strlen(s1) + 1;
	char * p = new char[len];
	memcpy(p, s1, len);
	return p;
}

#endif

