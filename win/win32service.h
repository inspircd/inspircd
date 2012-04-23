/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
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


#ifndef _WIN32SERVICE_H_
#define _WIN32SERVICE_H_

/* Hook for win32service.cpp to exit properly with the service specific error code */
void SetServiceStopped(int status);

/* Marks the service as running, not called until the config is parsed */
void SetServiceRunning();

#endif