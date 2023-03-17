/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2019 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006-2008 Craig Edwards <brain@inspircd.org>
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


#pragma once

/** Valid exit codes to be used with InspIRCd::Exit()
 */
enum ExitStatus {
    EXIT_STATUS_NOERROR = 0,        /* No error */
    EXIT_STATUS_DIE = 1,            /* Operator issued DIE */
    EXIT_STATUS_CONFIG = 2,         /* Config error */
    EXIT_STATUS_LOG = 3,            /* Log file error */
    EXIT_STATUS_FORK = 4,           /* fork() failed */
    EXIT_STATUS_ARGV = 5,           /* Invalid program arguments */
    EXIT_STATUS_PID = 6,            /* Couldn't write PID file */
    EXIT_STATUS_SOCKETENGINE = 7,   /* Couldn't start socket engine */
    EXIT_STATUS_ROOT = 8,           /* Refusing to start as root */
    EXIT_STATUS_MODULE = 9,         /* Couldn't load a required module */
    EXIT_STATUS_SIGTERM = 10        /* Received SIGTERM */
};

/** Array that maps exit codes (ExitStatus types) to
 * human-readable strings to be shown on shutdown.
 */
extern const char * ExitCodes[];
