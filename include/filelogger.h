/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2011 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *      the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef LOG_H
#define LOG_H

/** Debug levels for use with InspIRCd::Log()
 *  */
enum DebugLevel
{
    RAWIO       =   5,
    DEBUG       =   10,
    VERBOSE     =   20,
    DEFAULT     =   30,
    SPARSE      =   40,
    NONE        =   50
};


/* Forward declaration -- required */
class InspIRCd;

/** A logging class which logs to a streamed file.
 */
class CoreExport FileLogStream : public LogStream
{
 private:
	FileWriter *f;
 public:
	FileLogStream(int loglevel, FileWriter *fw);

	virtual ~FileLogStream();

	virtual void OnLog(int loglevel, const std::string &type, const std::string &msg);
};

#endif

