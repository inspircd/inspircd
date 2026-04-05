/**
 * @file yuarel.h
 * @brief URL parsing and decoding utilities.
 *
 * This file provides functions and structures for parsing and manipulating URLs.
 * It includes functions for parsing a URL into components, splitting paths,
 * parsing query strings, and decoding percent-encoded URL strings.
 *
 * @copyright Copyright (C) 2016 Jack Engqvist Johansson
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

#ifndef INC_YUAREL_H
#define INC_YUAREL_H

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @struct yuarel
 * @brief Structure that stores parsed URL components.
 *
 * scheme ":" [ "//" ] [ username ":" password "@" ] host [ ":" port ] [ "/" ] [ path ] [ "?" query ]
 *
 * Note: to make sure that no strings are copied, the first slash "/" in the
 * path will be used to null terminate the hostname if no port is supplied.
 */
struct yuarel
{
    char *scheme;   /**< @brief Scheme, without ":" and "//" */
    char *username; /**< @brief Username, default: NULL */
    char *password; /**< @brief Password, default: NULL */
    char *host;     /**< @brief Hostname or IP address */
    int port;       /**< @brief Port, default: 0 */
    char *path;     /**< @brief Path, without leading "/", default: NULL */
    char *query;    /**< @brief Query string, default: NULL */
    char *fragment; /**< @brief Fragment identifier, default: NULL */
};

/**
 * @struct yuarel_param
 * @brief Structure for holding query string parameters.
 *
 * This structure is used to store key-value pairs representing query
 * parameters in the URL's query string.
 */
struct yuarel_param
{
    char *key; /**< @brief Key of the query parameter */
    char *val; /**< @brief Value of the query parameter */
};

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
extern int yuarel_parse(struct yuarel *url, char *url_str);

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
extern int yuarel_split_path(char *path, char **parts, int max_parts);

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
extern int yuarel_parse_query(char *query, char delimiter, struct yuarel_param *params, int max_params);

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
extern char *yuarel_url_decode(char *str);

#ifdef __cplusplus
}
#endif

#endif /* INC_YUAREL_H */
