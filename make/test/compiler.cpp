/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2014-2015 Peter Powell <petpow@saberuk.com>
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
#if defined _LIBCPP_VERSION
# include <array>
# include <type_traits>
# include <unordered_map>
#else
# include <tr1/array>
# include <tr1/type_traits>
# include <tr1/unordered_map>
#endif

#if defined __llvm__ && !defined __clang__ && __GNUC__ == 4 && __GNUC_MINOR__ == 2 && __GNUC_PATCHLEVEL__ == 1
# error "LLVM-GCC 4.2.1 has broken visibility support."
#endif

int main() {
	std::cout << "Hello, World!" << std::endl;
	return 0;
}
