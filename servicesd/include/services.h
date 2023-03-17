/*
 *
 * (C) 2003-2023 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 *
 * Based on the original code of Epona by Lara.
 * Based on the original code of Services by Andy Church.
 */

#ifndef SERVICES_H
#define SERVICES_H

#include "sysconf.h"

#define BUFSIZE 1024

#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <stdexcept>

#include <string.h>
#if HAVE_STRINGS_H
# include <strings.h>
#endif

#ifndef _WIN32
#include <unistd.h>
#endif

/* Pull in the various bits of STL */
#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <exception>
#include <list>
#include <vector>
#include <deque>
#include <bitset>
#include <set>
#include <algorithm>
#include <iterator>

#include "defs.h"

#define _(x) x

#if defined __GXX_EXPERIMENTAL_CXX0X__ || __cplusplus >= 201103L
# define anope_override override
# define anope_final final
#else
# define anope_override
# define anope_final
#endif

#ifndef _WIN32
# define DllExport
# define CoreExport
# define MARK_DEPRECATED __attribute((deprecated))
# define anope_close close
#else
# include "anope_windows.h"
#endif

#endif // SERVICES_H
