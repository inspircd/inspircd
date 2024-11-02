/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2020, 2024 Sadie Powell <sadie@witchery.services>
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

class Module;

/** The version of the InspIRCd ABI which is presently in use. */
#define MODULE_ABI 4004UL

/** Stringifies the value of a symbol. */
#define MODULE_STRINGIFY_SYM1(DEF) MODULE_STRINGIFY_SYM2(DEF)
#define MODULE_STRINGIFY_SYM2(DEF) #DEF

/** The name of the symbol which contains the ABI that a module was compiled against. */
#define MODULE_SYM_ABI inspircd_module_abi
#define MODULE_STR_ABI MODULE_STRINGIFY_SYM1(MODULE_SYM_ABI)

/** The name of the symbol which creates a new Module instance. */
#define MODULE_SYM_INIT inspircd_module_init
#define MODULE_STR_INIT MODULE_STRINGIFY_SYM1(MODULE_SYM_INIT)

/** The name of the symbol which contains the version that a module was compiled against. */
#define MODULE_SYM_VERSION inspircd_module_version
#define MODULE_STR_VERSION MODULE_STRINGIFY_SYM1(MODULE_SYM_VERSION)

/** Defines the interface that a shared library must expose in order to be a module. */
#define MODULE_INIT(klass) \
	extern "C" DllExport const unsigned long MODULE_SYM_ABI = MODULE_ABI; \
	extern "C" DllExport const char MODULE_SYM_VERSION[] = INSPIRCD_VERSION; \
	extern "C" DllExport Module* MODULE_SYM_INIT() { return new klass; }
