/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2013, 2019, 2021 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012 ChrisTX <xpipe@hotmail.de>
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

#include <ostream>
#include <stdio.h>

#ifdef _WIN32
# include <iostream>
# include <io.h>
# define isatty(x) _isatty((x))
# define fileno(x) _fileno((x))
extern WindowsStream StandardError;
extern WindowsStream StandardOutput;
#else
# include <unistd.h>
#endif

namespace {
inline bool CanUseColors() {
#ifdef INSPIRCD_DISABLE_COLORS
    return false;
#else
    return isatty(fileno(stdout));
#endif
}

#ifdef _WIN32
inline WindowsStream& GetStreamHandle(std::ostream& os) {
    if (os.rdbuf() == std::cerr.rdbuf()) {
        return StandardError;
    }

    if (os.rdbuf() == std::cout.rdbuf()) {
        return StandardOutput;
    }

    // This will never happen.
    throw std::invalid_argument("Tried to write color codes to a stream other than stdout or stderr!");
}
#endif
}

#ifdef _WIN32

#include <windows.h>

inline std::ostream& con_green(std::ostream& stream) {
    if (CanUseColors()) {
        const WindowsStream& ws = GetStreamHandle(stream);
        SetConsoleTextAttribute(ws.Handle,
                                FOREGROUND_GREEN | FOREGROUND_INTENSITY | ws.BackgroundColor);
    }
    return stream;
}

inline std::ostream& con_red(std::ostream& stream) {
    if (CanUseColors()) {
        const WindowsStream& ws = GetStreamHandle(stream);
        SetConsoleTextAttribute(ws.Handle,
                                FOREGROUND_RED | FOREGROUND_INTENSITY | ws.BackgroundColor);
    }
    return stream;
}

inline std::ostream& con_white(std::ostream& stream) {
    if (CanUseColors()) {
        const WindowsStream& ws = GetStreamHandle(stream);
        SetConsoleTextAttribute(ws.Handle,
                                FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_GREEN | ws.BackgroundColor);
    }
    return stream;
}

inline std::ostream& con_white_bright(std::ostream& stream) {
    if (CanUseColors()) {
        const WindowsStream& ws = GetStreamHandle(stream);
        SetConsoleTextAttribute(ws.Handle,
                                FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY |
                                ws.BackgroundColor);
    }
    return stream;
}

inline std::ostream& con_bright(std::ostream& stream) {
    if (CanUseColors()) {
        const WindowsStream& ws = GetStreamHandle(stream);
        SetConsoleTextAttribute(ws.Handle, FOREGROUND_INTENSITY | ws.BackgroundColor);
    }
    return stream;
}

inline std::ostream& con_reset(std::ostream& stream) {
    if (CanUseColors()) {
        const WindowsStream& ws = GetStreamHandle(stream);
        SetConsoleTextAttribute(ws.Handle, ws.ForegroundColor);
    }
    return stream;
}

#else

inline std::ostream& con_green(std::ostream& stream) {
    if (!CanUseColors()) {
        return stream;
    }
    return stream << "\033[1;32m";
}

inline std::ostream& con_red(std::ostream& stream) {
    if (!CanUseColors()) {
        return stream;
    }
    return stream << "\033[1;31m";
}

inline std::ostream& con_white(std::ostream& stream) {
    if (!CanUseColors()) {
        return stream;
    }
    return stream << "\033[0m";
}

inline std::ostream& con_white_bright(std::ostream& stream) {
    if (!CanUseColors()) {
        return stream;
    }
    return stream << "\033[1m";
}

inline std::ostream& con_bright(std::ostream& stream) {
    if (!CanUseColors()) {
        return stream;
    }
    return stream << "\033[1m";
}

inline std::ostream& con_reset(std::ostream& stream) {
    if (!CanUseColors()) {
        return stream;
    }
    return stream << "\033[0m";
}

#endif
