/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *      the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef __LOG_H__
#define __LOG_H__

#include <time.h>
#include <string>
#include <sstream>
#include "socketengine.h"


/** Debug levels for use with InspIRCd::Log()
 *  */
enum DebugLevel
{
    DEBUG       =   10,
    VERBOSE     =   20,
    DEFAULT     =   30,
    SPARSE      =   40,
    NONE        =   50
};


/* Forward declaration -- required */
class InspIRCd;

/** This class implements a nonblocking log-writer.
 * Most people writing an ircd give little thought to their disk
 * i/o. On a congested system, disk writes can block for long
 * periods of time (e.g. if the system is busy and/or swapping
 * a lot). If we just use a blocking fprintf() call, this could
 * block for undesirable amounts of time (half of a second through
 * to whole seconds). We DO NOT want this, so we make our logfile
 * nonblocking and hook it into the SocketEngine.
 * NB: If the operating system does not support nonblocking file
 * I/O (linux seems to, as does freebsd) this will default to
 * blocking behaviour.
 */
class CoreExport FileLogger : public EventHandler
{
 protected:
	/** The creator/owner of this object
	 */
    InspIRCd* ServerInstance;
	/** The log file (fd is inside this somewhere,
	 * we get it out with fileno())
	 */
	FILE* log;
	/** Buffer of pending log lines to be written
	 */
	std::string buffer;
	/** Number of write operations that have occured
	 */
	int writeops;
 public:
	/** The constructor takes an already opened logfile.
	 */
	FileLogger(InspIRCd* Instance, FILE* logfile);
	/** This returns false, logfiles are writeable.
	 */
	virtual bool Readable();
	/** Handle pending write events.
	 * This will flush any waiting data to disk.
	 * If any data remains after the fprintf call,
	 * another write event is scheduled to write
	 * the rest of the data when possible.
	 */
	virtual void HandleEvent(EventType et, int errornum = 0);
	/** Write one or more preformatted log lines.
	 * If the data cannot be written immediately,
	 * this class will insert itself into the
	 * SocketEngine, and register a write event,
	 * and when the write event occurs it will
	 * attempt again to write the data.
	 */
	void WriteLogLine(const std::string &line);
	/** Close the log file and cancel any events.
	 */
	virtual void Close();
	/** Close the log file and cancel any events.
	 * (indirectly call Close()
	 */
	virtual ~FileLogger();
};


#endif
