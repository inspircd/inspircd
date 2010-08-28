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
/** Base64 encode */
CoreExport std::string BinToBase64(const std::string& data, const char* table = NULL, char pad = 0);
/** Base64 decode */
CoreExport std::string Base64ToBin(const std::string& data, const char* table = NULL);

/** Wrapping class to allow simple replacement of lookup function */
class CoreExport FormatSubstitute : public interfacebase
{
 public:
	/** Substitute $var expressions within the parameter */
	std::string format(const std::string& what);
	/** Variable lookup function: given a name, give the value */
	virtual std::string lookup(const std::string&) = 0;
};

class CoreExport MapFormatSubstitute : public FormatSubstitute
{
 public:
	const SubstMap& map;
	MapFormatSubstitute(const SubstMap& Map) : map(Map) {}
	virtual std::string lookup(const std::string&);
};

/** Given format = "foo $bar $baz!" and Map('bar' => 'one'), returns "foo one !" */
inline std::string MapFormatSubst(const std::string& format, const SubstMap& Map)
{
	MapFormatSubstitute m(Map);
	return m.format(format);
}

#endif

