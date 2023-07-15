/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013-2014, 2017, 2021 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013-2014, 2016 Attila Molnar <attilamolnar@hush.com>
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

/**
 * @def ATTR_NOT_NULL(...)
 * Enables the compile-time checking of arguments that must never be be null. If
 * a function is marked with this attribute then the compiler will warnif a null
 * pointer is passed to any of the specified arguments.
 */
#if defined __GNUC__
# define ATTR_NOT_NULL(...) __attribute__((nonnull(__VA_ARGS__)))
#else
# define ATTR_NOT_NULL(...)
#endif

/**
 * Windows is very different to UNIX so we have to wrap certain features in
 * order to build on Windows correctly.
 */
#if defined _WIN32
# include "win32wrapper.h"
#else
# define DllExport __attribute__ ((visibility ("default")))
# define CoreExport __attribute__ ((visibility ("default")))
#endif
