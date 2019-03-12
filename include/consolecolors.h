/*
 * InspIRCd -- Internet Relay Chat Daemon
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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */


#pragma once

#include <ostream>
#include <stdio.h>

#ifdef _WIN32
# include <io.h>
# define isatty(x) _isatty((x))
# define fileno(x) _fileno((x))
#else
# include <unistd.h>
#endif

namespace
{
	inline bool CanUseColors()
	{
#ifdef INSPIRCD_DISABLE_COLORS
		return false;
#else
		return isatty(fileno(stdout));
#endif
	}
}

#ifdef _WIN32

#include <windows.h>

extern WORD g_wOriginalColors;
extern WORD g_wBackgroundColor;
extern HANDLE g_hStdout;

inline std::ostream& con_green(std::ostream &s)
{
	if (CanUseColors())
		SetConsoleTextAttribute(g_hStdout, FOREGROUND_GREEN|FOREGROUND_INTENSITY|g_wBackgroundColor);
	return s;
}

inline std::ostream& con_red(std::ostream &s)
{
	if (CanUseColors())
		SetConsoleTextAttribute(g_hStdout, FOREGROUND_RED|FOREGROUND_INTENSITY|g_wBackgroundColor);
	return s;
}

inline std::ostream& con_white(std::ostream &s)
{
	if (CanUseColors())
		SetConsoleTextAttribute(g_hStdout, FOREGROUND_RED|FOREGROUND_BLUE|FOREGROUND_GREEN|g_wBackgroundColor);
	return s;
}

inline std::ostream& con_white_bright(std::ostream &s)
{
	if (CanUseColors())
		SetConsoleTextAttribute(g_hStdout, FOREGROUND_RED|FOREGROUND_BLUE|FOREGROUND_GREEN|FOREGROUND_INTENSITY|g_wBackgroundColor);
	return s;
}

inline std::ostream& con_bright(std::ostream &s)
{
	if (CanUseColors())
		SetConsoleTextAttribute(g_hStdout, FOREGROUND_INTENSITY|g_wBackgroundColor);
	return s;
}

inline std::ostream& con_reset(std::ostream &s)
{
	if (CanUseColors())
		SetConsoleTextAttribute(g_hStdout, g_wOriginalColors);
	return s;
}

#else

inline std::ostream& con_green(std::ostream &s)
{
	if (!CanUseColors())
		return s;
	return s << "\033[1;32m";
}

inline std::ostream& con_red(std::ostream &s)
{
	if (!CanUseColors())
		return s;
	return s << "\033[1;31m";
}

inline std::ostream& con_white(std::ostream &s)
{
	if (!CanUseColors())
		return s;
	return s << "\033[0m";
}

inline std::ostream& con_white_bright(std::ostream &s)
{
	if (!CanUseColors())
		return s;
	return s << "\033[1m";
}

inline std::ostream& con_bright(std::ostream &s)
{
	if (!CanUseColors())
		return s;
	return s << "\033[1m";
}

inline std::ostream& con_reset(std::ostream &s)
{
	if (!CanUseColors())
		return s;
	return s << "\033[0m";
}

#endif
