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

#define BRANCH   "InspIRCd-@VERSION_MAJOR@.@VERSION_MINOR@"
#define VERSION  "InspIRCd-@VERSION_MAJOR@.@VERSION_MINOR@.@VERSION_PATCH@"
#define REVISION "@VERSION_LABEL@"
#define SYSTEM   "@SYSTEM_NAME_VERSION@"

#define CONFIG_PATH "@CONFIG_DIR@"
#define DATA_PATH   "@DATA_DIR@"
#define LOG_PATH    "@LOG_DIR@"
#define MOD_PATH    "@MODULE_DIR@"

#define INSPIRCD_SOCKETENGINE_NAME "@SOCKETENGINE@"

#ifndef _WIN32
 %target include/config.h
 %define HAS_CLOCK_GETTIME
 %define HAS_EVENTFD
#endif
