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

#ifndef __EXITCODE_H__
#define __EXITCODE_H__

/** Valid exit codes to be used with InspIRCd::Exit()
 */
enum ExitStatus
{
	EXIT_STATUS_NOERROR = 0,
	EXIT_STATUS_DIE = 1,
	EXIT_STATUS_FAILED_EXEC = 2,
	EXIT_STATUS_INTERNAL = 3,
	EXIT_STATUS_CONFIG = 4,
	EXIT_STATUS_LOG = 5,
	EXIT_STATUS_FORK = 6,
	EXIT_STATUS_ARGV = 7,
	EXIT_STATUS_BIND = 8,
	EXIT_STATUS_PID = 9,
	EXIT_STATUS_SOCKETENGINE = 10,
	EXIT_STATUS_ROOT = 11,
	EXIT_STATUS_DIETAG = 12,
	EXIT_STATUS_MODULE = 13,
	EXIT_STATUS_SIGTERM = 15	/* Note: dont move this value. It corresponds with the value of #define SIGTERM. */
};

/** Array that maps exit codes (ExitStatus types) to
 * human-readable strings to be shown on shutdown.
 */
extern const char * ExitCodes[];

#endif
