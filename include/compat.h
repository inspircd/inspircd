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
 * This macro enables the compile-time checking of printf format strings. This
 * makes the compiler show a warning if the format of a printf arguments are
 * incorrect.
 */
#if defined __clang__ || defined __GNUC__
# define CUSTOM_PRINTF(stringpos, firstpos) __attribute__((format(printf, stringpos, firstpos)))
#else
# define CUSTOM_PRINTF(stringpos, firstpos)
#endif

/**
 * Windows is very different to UNIX so we have to wrap certain features in
 * order to build on Windows correctly.
 */
#if defined _WIN32
# include "inspircd_win32wrapper.h"
#else
# define ENTRYPOINT int main(int argc, char** argv)
# define DllExport __attribute__ ((visibility ("default")))
# define CoreExport __attribute__ ((visibility ("default")))
# include <unistd.h>
#endif
