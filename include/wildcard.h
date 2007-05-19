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

CoreExport bool match(const char *str, const char *mask);
CoreExport bool match(const char *str, const char *mask, bool use_cidr_match);
CoreExport bool match(bool case_sensitive, const char *str, const char *mask);
CoreExport bool match(bool case_sensitive, const char *str, const char *mask, bool use_cidr_match);
