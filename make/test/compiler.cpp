/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2014, 2024 Sadie Powell <sadie@witchery.services>
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


#include <iostream>

#if defined __INTEL_CLANG_COMPILER // Also defines __clang__
# define INSPIRCD_COMPILER_NAME "IntelClang"
# define INSPIRCD_COMPILER_VERSION (__INTEL_CLANG_COMPILER / 10000) << '.' << ((__INTEL_CLANG_COMPILER % 10000) / 100)
#elif defined __clang__ // Also defines __GNUC__
# if defined __apple_build_version__
#  define INSPIRCD_COMPILER_NAME "AppleClang"
# else
#  define INSPIRCD_COMPILER_NAME "Clang"
#  if __clang_major__ < 9
#   define INSPIRCD_EXTRA_LDLIBS "-lc++fs"
#  endif
# endif
# define INSPIRCD_COMPILER_VERSION __clang_major__ << '.' << __clang_minor__
#elif defined __GNUC__
# define INSPIRCD_COMPILER_NAME "GCC"
# define INSPIRCD_COMPILER_VERSION __GNUC__ << '.' << __GNUC_MINOR__
# if __GNUC__ < 9
#  define INSPIRCD_EXTRA_LDLIBS "-lstdc++fs"
# endif
#endif

#ifndef INSPIRCD_EXTRA_LDLIBS
# define INSPIRCD_EXTRA_LDLIBS ""
#endif

int main() {
	std::cout
		<< "NAME " << INSPIRCD_COMPILER_NAME << std::endl
		<< "VERSION " << INSPIRCD_COMPILER_VERSION << std::endl
		<< "EXTRA_LDLIBS " << INSPIRCD_EXTRA_LDLIBS << std::endl;
	return 0;
}
