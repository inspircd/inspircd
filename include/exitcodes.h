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
	EXIT_STATUS_NOERROR = 0,	/* No error */
	EXIT_STATUS_DIE = 1,		/* Operator issued DIE */
	EXIT_STATUS_FAILED_EXEC = 2,	/* execv() failed */
	EXIT_STATUS_INTERNAL = 3,	/* Internal error */
	EXIT_STATUS_CONFIG = 4,		/* Config error */
	EXIT_STATUS_LOG = 5,		/* Log file error */
	EXIT_STATUS_FORK = 6,		/* fork() failed */
	EXIT_STATUS_ARGV = 7,		/* Invalid program arguments */
	EXIT_STATUS_BIND = 8,		/* Port binding failed on all ports */
	EXIT_STATUS_PID = 9,		/* Couldn't write PID file */
	EXIT_STATUS_SOCKETENGINE = 10,	/* Couldn't start socket engine */
	EXIT_STATUS_ROOT = 11,		/* Refusing to start as root */
	EXIT_STATUS_DIETAG = 12,	/* Found a die tag in the config file */
	EXIT_STATUS_MODULE = 13,	/* Couldn't load a required module */
	EXIT_STATUS_CREATEPROCESS = 14,	/* CreateProcess failed (windows) */
	EXIT_STATUS_SIGTERM = 15,	/* Note: dont move this value. It corresponds with the value of #define SIGTERM. */
	EXIT_STATUS_BADHANDLER = 16	/* Bad command handler loaded */
};

/** Array that maps exit codes (ExitStatus types) to
 * human-readable strings to be shown on shutdown.
 */
extern const char * ExitCodes[];

#endif

