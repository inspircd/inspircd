/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2008 Thomas Stagner <aquanight@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#ifndef FILELOGGER_H
#define FILELOGGER_H

#include "logger.h"

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

