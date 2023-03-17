#ifndef _SYSCONF_H_
#define _SYSCONF_H_

#cmakedefine DEBUG_BUILD

#cmakedefine DEFUMASK @DEFUMASK@
#cmakedefine HAVE_CSTDINT 1
#cmakedefine HAVE_STDINT_H 1
#cmakedefine HAVE_STDDEF_H 1
#cmakedefine HAVE_STRCASECMP 1
#cmakedefine HAVE_STRICMP 1
#cmakedefine HAVE_STRINGS_H 1
#cmakedefine HAVE_UMASK 1
#cmakedefine HAVE_EVENTFD 1
#cmakedefine HAVE_EPOLL 1
#cmakedefine HAVE_POLL 1
#cmakedefine GETTEXT_FOUND 1

#ifdef HAVE_CSTDINT
# include <cstdint>
#else
# ifdef HAVE_STDINT_H
#  include <stdint.h>
# else
#  include "pstdint.h"
# endif
#endif
#ifdef HAVE_STDDEF_H
# include <stddef.h>
#endif

#ifdef _WIN32
# define popen _popen
# define pclose _pclose
# define ftruncate _chsize
# ifdef MSVCPP
#  define PATH_MAX MAX_PATH
# endif
# define MAXPATHLEN MAX_PATH
# define bzero(buf, size) memset(buf, 0, size)
# ifdef MSVCPP
#  define strcasecmp stricmp
# endif
# define sleep(x) Sleep(x * 1000)
#endif

#endif
