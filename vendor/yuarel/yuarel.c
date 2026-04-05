/**
 * Copyright (C) 2016,2017 Jack Engqvist Johansson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include "yuarel.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Parse a non-null terminated string into an integer.
 *
 * This function converts a substring of numeric characters (specified by `len`)
 * into an integer. It does not check for null termination and relies on `len`
 * to determine the length of the string to parse.
 *
 * @param str: The string containing the number (non-null terminated).
 * @param len: The number of characters in `str` to parse.
 *
 * @return: The parsed integer value.
 */
static inline int natoi(const char *str, size_t len)
{
    int r = 0;
    for (size_t i = 0; i < len; i++)
    {
        r *= 10;
        r += str[i] - '0';
    }

    return r;
}

/**
 * Check if a URL is relative (no scheme and hostname).
 *
 * A relative URL starts with a "/" but does not include a scheme (e.g., "http://")
 * or a hostname. This function determines whether the given URL is relative based
 * on this criteria.
 *
 * @param url: The string containing the URL to check.
 *
 * @return: 1 if the URL is relative, otherwise 0.
 */
static inline int is_relative(const char *url) { return (*url == '/') ? 1 : 0; }

/**
 * Parse the scheme of a URL by inserting a null terminator after the scheme.
 *
 * This function attempts to parse the scheme part of a URL, which is followed by
 * a colon (":") and two slashes ("//"). If the scheme is present and valid, the
 * function null-terminates the scheme and returns a pointer to the host part of the URL.
 *
 * @warning: Modifies the input string as part of the parsing process.
 *
 * @param str: The string containing the URL to parse. This string will be modified
 *             by null-terminating the scheme portion.
 *
 * @return: A pointer to the part of the string after the scheme (i.e., the host),
 *         or NULL if the scheme is not found or invalid.
 */
static inline char *parse_scheme(char *str)
{
    char *s;

    /* If not found or first in string, return error */
    s = strchr(str, ':');
    if (s == NULL || s == str)
    {
        return NULL;
    }

    /* If not followed by two slashes, return error */
    if (s[1] == '\0' || s[1] != '/' || s[2] == '\0' || s[2] != '/')
    {
        return NULL;
    }

    *s = '\0'; // Replace ':' with NULL

    return s + 3;
}

/**
 * Find a character in a string, replace it with '\0' and return the next
 * character in the string.
 *
 * This function searches for a specific character in the string and, if found,
 * replaces it with a null terminator ('\0'). It then returns a pointer to the
 * character immediately after the one found. If the character is not found,
 * the function returns NULL.
 *
 * @warning: Modifies the input string as part of the parsing process.
 *
 * @param str: The string to search in.
 * @param find: The character to search for.
 *
 * @return: A pointer to the character after the found character, or NULL if
 *         the character was not found.
 */
static inline char *find_and_terminate(char *str, char find)
{
    str = strchr(str, find);
    if (NULL == str)
    {
        return NULL;
    }

    *str = '\0';
    return str + 1;
}

/*
    Dev Note: Regarding find_fragment(), find_query() and find_path()
                Yes, the following functions could be implemented as preprocessor macros
                instead of inline functions, but I think that this approach will be more
                clean in this case.
*/

/**
 * Find the fragment part of a URL (everything after the '#' character).
 *
 * This function searches for the fragment delimiter ('#') in the URL and,
 * if found, replaces it with a null terminator ('\0'). It returns a pointer
 * to the part of the string after the fragment delimiter.
 *
 * @param str: The URL string to search in.
 *
 * @return: A pointer to the part of the string after the fragment part,
 *         or NULL if no fragment is found.
 */
static inline char *find_fragment(char *str) { return find_and_terminate(str, '#'); }

/**
 * Find the query part of a URL (everything after the '?' character).
 *
 * This function searches for the query delimiter ('?') in the URL and,
 * if found, replaces it with a null terminator ('\0'). It returns a pointer
 * to the part of the string after the query delimiter.
 *
 * @param str: The URL string to search in.
 *
 * @return: A pointer to the part of the string after the query part,
 *         or NULL if no query is found.
 */
static inline char *find_query(char *str) { return find_and_terminate(str, '?'); }

/**
 * Find the path part of a URL (everything after the '/' character).
 *
 * This function searches for the path delimiter ('/') in the URL and,
 * if found, replaces it with a null terminator ('\0'). It returns a pointer
 * to the part of the string after the path delimiter.
 *
 * @param str: The URL string to search in.
 *
 * @return: A pointer to the part of the string after the path part,
 *         or NULL if no path is found.
 */
static inline char *find_path(char *str) { return find_and_terminate(str, '/'); }

/**
 * @brief Parse a URL into its components.
 *
 * This function parses a URL into its components:
 *     scheme, host, port, path, query, and fragment.
 * The URL string should be in one of the following formats:
 *
 * Absolute URL:
 * scheme ":" [ "//" ] [ username ":" password "@" ] host [ ":" port ] [ "/" ] [ path ] [ "?" query ] [ "#" fragment ]
 *
 * Relative URL:
 * path [ "?" query ] [ "#" fragment ]
 *
 * The following parts will be parsed to the corresponding struct member.
 *
 * @warning: Modifies the input string as part of the parsing process.
 *
 * @param[out] url A pointer to the `yuarel` struct where the parsed values will be stored.
 * @param[in,out] url_str A pointer to the URL string to be parsed. The string will be modified.
 *
 * @return 0 on success, otherwise -1 on error.
 */
int yuarel_parse(struct yuarel *url, char *url_str)
{
    if (NULL == url || NULL == url_str)
    {
        return -1;
    }

    memset(url, 0, sizeof(struct yuarel));

    /* (Fragment) */
    url->fragment = find_fragment(url_str);

    /* (Query) */
    url->query = find_query(url_str);

    /* Relative URL? Parse scheme and hostname */
    if (is_relative(url_str))
    {
        /* Relative (Path) Found */
        url->path = find_path(url_str);
        return 0;
    }

    /* Scheme */
    url->scheme = url_str;
    url_str = parse_scheme(url_str);
    if (url_str == NULL)
    {
        /* Scheme Missing */
        return -1;
    }

    /* Check if host is omitted but path is provided (e.g. File URI) */
    if ('/' == *url_str)
    {
        /* URI Omits Hostname. Record only path */
        url->path = url_str;
        return 0;
    }

    /* Host */
    if ('\0' == *url_str)
    {
        return -1;
    }
    url->host = url_str;

    /* (Path) */
    url->path = find_path(url_str);

    /* (Credentials) */
    url_str = strchr(url->host, '@');
    if (NULL != url_str)
    {
        /* Missing credentials? */
        if (url_str == url->host)
        {
            return -1;
        }

        url->username = url->host;
        url->host = url_str + 1;
        *url_str = '\0';

        url_str = strchr(url->username, ':');
        if (NULL != url_str)
        {
            url->password = url_str + 1;
            *url_str = '\0';
        }
    }

    /* Missing hostname? */
    if ('\0' == *url->host)
    {
        return -1;
    }

    /* IPv6 hostname */
    if ('[' == url->host[0])
    {
        /* IPv6 Literal */
        // If hostname starts with square bracket, it is IPv6 literal
        // example:
        // http://[1080:0:0:0:8:800:200C:417A]:80/index.html
        // http://[3ffe:2a00:100:7031::1]
        // http://[::192.9.5.5]/ipng
        url->host++;

        /* Locate end of IPv6 Literal marker */
        url_str = strchr(url->host, ']');
        if (NULL == url_str)
        {
            /* Missing end ']' marker */
            return -1;
        }

        /* Null terminate IPv6 host literal (In place of ']') */
        *url_str = '\0';

        /* Check if next character is ':' is present.
         * Which would indicate port number is present */
        url_str++;
        if ('\0' == *url_str)
        {
            /* Already end of host string. No port number present */
            url_str = NULL;
        }
        else if (':' != *url_str)
        {
            /* Port number delimiter is missing. Invalid. */
            return -1;
        }
    }
    else
    {
        /* Check for port number delimiter */
        // example: http://192.156.1.1:80 ('<HOST>:<PORT>')
        url_str = strchr(url->host, ':');
    }

    if (NULL != url_str && (NULL == url->path || url_str < url->path))
    {
        *(url_str++) = '\0';
        if ('\0' == *url_str)
        {
            return -1;
        }

        if (url->path)
        {
            url->port = natoi(url_str, url->path - url_str - 1);
        }
        else
        {
            url->port = atoi(url_str);
        }
    }

    /* Missing hostname? */
    if ('\0' == *url->host)
    {
        return -1;
    }

    return 0;
}

/**
 * @brief Split a URL path into parts.
 *
 * No data is copied, the slashed are used as null terminators and then
 * pointers to each path part will be stored in **parts. Double slashes will be
 * treated as one.
 *
 * @warning: Modifies the input string as part of the parsing process.
 *
 * @param[in,out] path The path string to split. The string will be modified.
 * @param[out] parts An array where the resulting path parts will be stored.
 * @param[in] max_parts The maximum number of parts to parse.
 *
 * @return The number of parsed path parts, or -1 on error.
 */
int yuarel_split_path(char *path, char **parts, int max_parts)
{
    int i = 0;

    if (NULL == path || '\0' == *path)
    {
        return -1;
    }

    do
    {
        /* Forward to after slashes */
        while (*path == '/')
        {
            path++;
        }

        if ('\0' == *path)
        {
            break;
        }

        parts[i++] = path;

        path = strchr(path, '/');
        if (NULL == path)
        {
            break;
        }

        *(path++) = '\0';
    } while (i < max_parts);

    return i;
}

/**
 * @brief Parse a query string into key-value pairs.
 *
 * The query string should be a null terminated string of parameters separated by
 * a delimiter. Each parameter are checked for the equal sign character. If it
 * appears in the parameter, it will be used as a null terminator and the part
 * that comes after it will be the value of the parameter.
 *
 * No data are copied, the equal sign and delimiters are used as null
 * terminators and then pointers to each parameter key and value will be stored
 * in the yuarel_param struct.
 *
 * @warning: Modifies the input string as part of the parsing process.
 *
 * @param[in,out] query The query string to parse. The string will be modified.
 * @param[in] delimiter The character that separates key-value pairs in the query.
 * @param[out] params An array where the parsed key-value pairs will be stored.
 * @param[in] max_params The maximum number of parameters to parse.
 *
 * @return The number of parsed parameters, or -1 on error.
 */
int yuarel_parse_query(char *query, char delimiter, struct yuarel_param *params, int max_params)
{
    int param_count = 0;

    if (NULL == query || '\0' == *query)
    {
        return -1;
    }

    while (param_count < max_params)
    {
        char *curr_key_ptr = query;
        char *curr_end_ptr = strchr(curr_key_ptr, delimiter);
        char *val_delim;

        if (curr_end_ptr != NULL)
        {
            // There will be more kv pairs ahead so zero terminate this kv
            *curr_end_ptr = '\0';
        }

        /* Write KV parameter */
        params[param_count].key = curr_key_ptr;
        val_delim = strchr(curr_key_ptr, '=');
        if (val_delim != NULL)
        {
            /* Value Is Present. Split and record */
            *val_delim = '\0';
            params[param_count].val = val_delim + 1;
        }
        else
        {
            /* Value Not Present. Mark as empty */
            params[param_count].val = NULL;
        }
        param_count++;

        /* Handle next KV pair if present */
        if (curr_end_ptr == NULL)
        {
            /* No more kv pairs ahead. Finish up */
            break;
        }

        /* There will be more kv pairs ahead */
        query = curr_end_ptr + 1;
    }

    return param_count;
}

/**
 * @brief Decode a percent-encoded URL string in place.
 *
 * This function decodes percent-encoded characters (`%XX`) in the input URL
 * string and replaces `+` with spaces. The string is modified directly, and
 * no additional memory is allocated.
 *
 * @warning: Modifies the input string as part of the parsing process.
 *
 * @param[in,out] str The input string to decode. The string will be modified.
 *
 * @return The modified input string (same pointer as `str`).
 */
char *yuarel_url_decode(char *str)
{
    // Hex decoding utilities
#define YUAREL_URL_DECODE_IS_HEX(ch) (('0' <= (ch) && (ch) <= '9') || ('a' <= (ch) && (ch) <= 'f') || ('A' <= (ch) && (ch) <= 'F'))
#define YUAREL_URL_DECODE_PARSE_HEX(ch) (('0' <= (ch) && (ch) <= '9') ? ((ch) - '0') : (('a' <= (ch) && (ch) <= 'f') ? (10 + (ch) - 'a') : (('A' <= (ch) && (ch) <= 'F') ? (10 + (ch) - 'A') : 0)))

    const char *read_ptr = str;
    char *write_ptr = str;

    if (NULL == str || '\0' == *str)
    {
        return str;
    }

    while (*read_ptr)
    {
        if (read_ptr[0] == '+')
        {
            // '+' is a space in URL-encoded strings
            *write_ptr = ' ';
            write_ptr += 1;
            read_ptr += 1;
        }
        else if (read_ptr[0] == '%' && YUAREL_URL_DECODE_IS_HEX(read_ptr[1]) && YUAREL_URL_DECODE_IS_HEX(read_ptr[2]))
        {
            // Decode percent encoded hex and skip past the two hex character
            *write_ptr = YUAREL_URL_DECODE_PARSE_HEX(read_ptr[1]) << 4 | YUAREL_URL_DECODE_PARSE_HEX(read_ptr[2]);
            write_ptr += 1;
            read_ptr += 3;
        }
        else
        {
            // Regular character, copy as is
            *write_ptr = *read_ptr;
            write_ptr += 1;
            read_ptr += 1;
        }
    }
#undef YUAREL_URL_DECODE_IS_HEX
#undef YUAREL_URL_DECODE_PARSE_HEX

    // Null-terminate the string
    *write_ptr = '\0';
    return str;
}
