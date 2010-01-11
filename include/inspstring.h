/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef __IN_INSPSTRING_H
#define __IN_INSPSTRING_H

// This (inspircd_config) is needed as inspstring doesn't pull in the central header
#include "inspircd_config.h"
#include <cstring>
//#include <cstddef>

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

/** Binary to hexadecimal conversion */
CoreExport std::string BinToHex(const std::string& data);

#endif

