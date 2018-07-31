/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013 Peter Powell <petpow@saberuk.com>
 *   Copyright (C) 2008 Thomas Stagner <aquanight@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
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
 * These macros enable the use of the C++11 override control keywords in
 * compilers which support them.
 */
#if __cplusplus >= 201103L
# define HAS_CXX11_FINAL_OVERRIDE
#elif defined __clang__
# if __has_feature(cxx_override_control)
#  define HAS_CXX11_FINAL_OVERRIDE
# endif
#elif (__GNUC__ > 4) || (__GNUC__ == 4 && __GNUC_MINOR__ >= 7)
# if defined __GXX_EXPERIMENTAL_CXX0X__
#  define HAS_CXX11_FINAL_OVERRIDE
# endif
#elif _MSC_VER >= 1700
# define HAS_CXX11_FINAL_OVERRIDE
#endif

#if defined HAS_CXX11_FINAL_OVERRIDE
# define CXX11_FINAL final
# define CXX11_OVERRIDE override
#else
# define CXX11_FINAL
# define CXX11_OVERRIDE
#endif

/**
 * Windows is very different to UNIX so we have to wrap certain features in
 * order to build on Windows correctly.
 */
#if defined _WIN32
# include "inspircd_win32wrapper.h"
# include "threadengines/threadengine_win32.h"
#else
# define ENTRYPOINT int main(int argc, char** argv)
# define DllExport __attribute__ ((visibility ("default")))
# define CoreExport __attribute__ ((visibility ("default")))
# include <unistd.h>
# include "threadengines/threadengine_pthread.h"
#endif
