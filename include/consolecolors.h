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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef CONSOLECOLORS_H
#define CONSOLECOLORS_H

#include <ostream>

#ifdef _WIN32

#include <windows.h>

extern WORD g_wOriginalColors;
extern WORD g_wBackgroundColor;
extern HANDLE g_hStdout;

inline std::ostream& con_green(std::ostream &s)
{
    SetConsoleTextAttribute(g_hStdout, FOREGROUND_GREEN|FOREGROUND_INTENSITY|g_wBackgroundColor);
    return s;
}

inline std::ostream& con_red(std::ostream &s)
{
    SetConsoleTextAttribute(g_hStdout, FOREGROUND_RED|FOREGROUND_INTENSITY|g_wBackgroundColor);
    return s;
}

inline std::ostream& con_white(std::ostream &s)
{
    SetConsoleTextAttribute(g_hStdout, FOREGROUND_RED|FOREGROUND_BLUE|FOREGROUND_GREEN|g_wBackgroundColor);
    return s;
}

inline std::ostream& con_white_bright(std::ostream &s)
{
    SetConsoleTextAttribute(g_hStdout, FOREGROUND_RED|FOREGROUND_BLUE|FOREGROUND_GREEN|FOREGROUND_INTENSITY|g_wBackgroundColor);
    return s;
}

inline std::ostream& con_bright(std::ostream &s)
{
    SetConsoleTextAttribute(g_hStdout, FOREGROUND_INTENSITY|g_wBackgroundColor);
    return s;
}

inline std::ostream& con_reset(std::ostream &s)
{
    SetConsoleTextAttribute(g_hStdout, g_wOriginalColors);
    return s;
}

#else

inline std::ostream& con_green(std::ostream &s)
{
    return s << "\033[1;32m";
}

inline std::ostream& con_red(std::ostream &s)
{
    return s << "\033[1;31m";
}

inline std::ostream& con_white(std::ostream &s)
{
    return s << "\033[0m";
}

inline std::ostream& con_white_bright(std::ostream &s)
{
    return s << "\033[1m";
}

inline std::ostream& con_bright(std::ostream &s)
{
    return s << "\033[1m";
}

inline std::ostream& con_reset(std::ostream &s)
{
    return s << "\033[0m";
}

#endif

#endif
