/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2014 Peter Powell <petpow@saberuk.com>
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

/*** The branch version that is shown to unprivileged users. */
#define INSPIRCD_BRANCH "InspIRCd-@VERSION_MAJOR@"

/*** The full version that is shown to privileged users. */
#define INSPIRCD_VERSION "InspIRCd-@VERSION_FULL@"

/*** Determines whether this version of InspIRCd is older than the requested version. */
#define INSPIRCD_VERSION_BEFORE(MAJOR, MINOR) (((@VERSION_MAJOR@ << 8) | @VERSION_MINOR@) < ((MAJOR << 8) | (MINOR)))

/*** Determines whether this version of InspIRCd is equal to or newer than the requested version. */
#define INSPIRCD_VERSION_SINCE(MAJOR, MINOR) (((@VERSION_MAJOR@ << 16) | @VERSION_MINOR@) >= ((MAJOR << 8) | (MINOR)))

/*** The default location that config files are stored in. */
#define INSPIRCD_CONFIG_PATH "@CONFIG_DIR@"

/*** The default location that data files are stored in. */
#define INSPIRCD_DATA_PATH "@DATA_DIR@"

/*** The default location that log files are stored in. */
#define INSPIRCD_LOG_PATH "@LOG_DIR@"

/*** The default location that module files are stored in. */
#define INSPIRCD_MODULE_PATH "@MODULE_DIR@"

#ifndef _WIN32
 %target include/config.h

/*** Whether the arc4random_buf() function was available at compile time. */
 %define HAS_ARC4RANDOM_BUF

/*** Whether the clock_gettime() function was available at compile time. */
 %define HAS_CLOCK_GETTIME

/*** Whether the eventfd() function was available at compile time. */
 %define HAS_EVENTFD

#endif
