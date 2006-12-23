#ifndef __EXITCODE_H__
#define __EXITCODE_H__

/** Valid exit codes to be used with InspIRCd::Exit()
 *  */
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
 *  * human-readable strings to be shown on shutdown.
 *   */
const char* ExitCodes[] =
{
	"No error", /* 0 */
	"DIE command", /* 1 */
	"execv() failed", /* 2 */
	"Internal error", /* 3 */
	"Config file error", /* 4 */
	"Logfile error", /* 5 */
	"Fork failed", /* 6 */
	"Bad commandline parameters", /* 7 */
	"No ports could be bound", /* 8 */
	"Can't write PID file", /* 9 */
	"SocketEngine could not initialize", /* 10 */
	"Refusing to start up as root", /* 11 */
	"Found a <die> tag!", /* 12 */
	"Couldn't load module on startup", /* 13 */
	"", /* 14 */
	"Received SIGTERM", /* 15 */
};

#endif
