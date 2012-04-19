/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2006-2008 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
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


#ifndef EXITCODE_H
#define EXITCODE_H

/** Valid exit codes to be used with InspIRCd::Exit()
 */
enum ExitStatus
{
	EXIT_STATUS_NOERROR = 0,		/* No error */
	EXIT_STATUS_DIE = 1,			/* Operator issued DIE */
	EXIT_STATUS_FAILED_EXEC = 2,		/* execv() failed */
	EXIT_STATUS_INTERNAL = 3,		/* Internal error */
	EXIT_STATUS_CONFIG = 4,			/* Config error */
	EXIT_STATUS_LOG = 5,			/* Log file error */
	EXIT_STATUS_FORK = 6,			/* fork() failed */
	EXIT_STATUS_ARGV = 7,			/* Invalid program arguments */
	EXIT_STATUS_BIND = 8,			/* Port binding failed on all ports */
	EXIT_STATUS_PID = 9,			/* Couldn't write PID file */
	EXIT_STATUS_SOCKETENGINE = 10,		/* Couldn't start socket engine */
	EXIT_STATUS_ROOT = 11,			/* Refusing to start as root */
	EXIT_STATUS_DIETAG = 12,		/* Found a die tag in the config file */
	EXIT_STATUS_MODULE = 13,		/* Couldn't load a required module */
	EXIT_STATUS_CREATEPROCESS = 14,		/* CreateProcess failed (windows) */
	EXIT_STATUS_SIGTERM = 15,		/* Note: dont move this value. It corresponds with the value of #define SIGTERM. */
	EXIT_STATUS_BADHANDLER = 16,		/* Bad command handler loaded */
	EXIT_STATUS_RSCH_FAILED = 17,		/* Windows service specific failure, will name these later */
	EXIT_STATUS_UPDATESCM_FAILED = 18,	/* Windows service specific failure, will name these later */
	EXIT_STATUS_CREATE_EVENT_FAILED = 19	/* Windows service specific failure, will name these later */
};

/** Array that maps exit codes (ExitStatus types) to
 * human-readable strings to be shown on shutdown.
 */
extern const char * ExitCodes[];

#endif

