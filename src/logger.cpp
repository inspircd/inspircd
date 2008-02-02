/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2008 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

/* $Core: libIRCDlogger */

#include "inspircd.h"

/*
 * Suggested implementation...
 *	class LogManager
 *		LogStream *AddLogType(const std::string &type)
 *		LogStream *DelLogType(const std::string &type)
 *		Log(LogStream *, enum loglevel, const std::string &msg)
 *
 *  class LogStream
 *		std::string type
 *		(void)(*)Callback(LogStream *, enum loglevel, const std::string &msg) <---- callback for modules to implement their own logstreams, core will just handle to file/channel(?)
 *
 * Feel free to elaborate on this further.
 */
