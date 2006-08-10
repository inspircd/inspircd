#ifndef __IN_INSPSTRING_H
#define __IN_INSPSTRING_H

#include "inspircd_config.h"
#include <cstddef>

#ifndef HAS_STRLCPY
size_t strlcpy(char *dst, const char *src, size_t siz);
size_t strlcat(char *dst, const char *src, size_t siz);
#endif

int charlcat(char* x,char y,int z);
bool charremove(char* mp, char remove);

#endif
