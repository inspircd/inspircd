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

#include "inspircd_config.h"

/** Match a string against a mask.
 * @param str The string to check
 * @param mask the mask to check against
 * @return true if the strings match
 */
CoreExport bool match(const char *str, const char *mask);
/** Match a string against a mask, and define wether or not to use CIDR rules
 * @param str The string to check
 * @param mask the mask to check against
 * @param use_cidr_match True if CIDR matching rules should be applied first
 * @return true if the strings match
 */
CoreExport bool match(const char *str, const char *mask, bool use_cidr_match);
/** Match a string against a mask, defining wether case sensitivity applies.
 * @param str The string to check
 * @param mask the mask to check against
 * @param case_sensitive True if the match is case sensitive
 * @return True if the strings match
 */
CoreExport bool match(bool case_sensitive, const char *str, const char *mask);
/** Match a string against a mask, defining wether case sensitivity applies,
 * and defining wether or not to use CIDR rules first.
 * @param case_sensitive True if the match is case sensitive
 * @param str The string to check
 * @param mask the mask to check against
 * @param use_cidr_match True if CIDR matching rules should be applied first
 * @return true if the strings match
 */
CoreExport bool match(bool case_sensitive, const char *str, const char *mask, bool use_cidr_match);

