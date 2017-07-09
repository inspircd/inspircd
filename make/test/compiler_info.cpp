/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017 Peter Powell <petpow@saberuk.com>
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

#if defined __INTEL_COMPILER // Also defines __clang__ and __GNUC__
# define INSPIRCD_COMPILER_NAME "Intel"
# define INSPIRCD_COMPILER_VERSION (__INTEL_COMPILER / 100) << '.' << (__INTEL_COMPILER % 100)
#elif defined __clang__ // Also defines __GNUC__
# if defined __apple_build_version__
#  define INSPIRCD_COMPILER_NAME "AppleClang"
# else
#  define INSPIRCD_COMPILER_NAME "Clang"
# endif
# define INSPIRCD_COMPILER_VERSION __clang_major__ << '.' << __clang_minor__
#elif defined __GNUC__
# define INSPIRCD_COMPILER_NAME "GCC"
# define INSPIRCD_COMPILER_VERSION __GNUC__ << '.' << __GNUC_MINOR__
#endif

int main() {
	std::cout << "NAME " << INSPIRCD_COMPILER_NAME << std::endl
		<< "VERSION " << INSPIRCD_COMPILER_VERSION << std::endl;
	return 0;
}
