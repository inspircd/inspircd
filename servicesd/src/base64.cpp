/* base64 routines.
 *
 * (C) 2003-2023 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 *
 * Based on the original code of Epona by Lara.
 * Based on the original code of Services by Andy Church.
 */

#include "services.h"
#include "anope.h"

static const Anope::string Base64 =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static const char Pad64 = '=';

/* (From RFC1521 and draft-ietf-dnssec-secext-03.txt)
   The following encoding technique is taken from RFC 1521 by Borenstein
   and Freed.  It is reproduced here in a slightly edited form for
   convenience.

   A 65-character subset of US-ASCII is used, enabling 6 bits to be
   represented per printable character. (The extra 65th character, "=",
   is used to signify a special processing function.)

   The encoding process represents 24-bit groups of input bits as output
   strings of 4 encoded characters. Proceeding from left to right, a
   24-bit input group is formed by concatenating 3 8-bit input groups.
   These 24 bits are then treated as 4 concatenated 6-bit groups, each
   of which is translated into a single digit in the base64 alphabet.

   Each 6-bit group is used as an index into an array of 64 printable
   characters. The character referenced by the index is placed in the
   output string.

                         Table 1: The Base64 Alphabet

      Value Encoding  Value Encoding  Value Encoding  Value Encoding
          0 A           17 R            34 i            51 z
          1 B           18 S            35 j            52 0
          2 C           19 T            36 k            53 1
          3 D           20 U            37 l            54 2
          4 E           21 V            38 m            55 3
          5 F           22 W            39 n            56 4
          6 G           23 X            40 o            57 5
          7 H           24 Y            41 p            58 6
          8 I           25 Z            42 q            59 7
          9 J           26 a            43 r            60 8
         10 K           27 b            44 s            61 9
         11 L           28 c            45 t            62 +
         12 M           29 d            46 u            63 /
         13 N           30 e            47 v
         14 O           31 f            48 w         (pad) =
         15 P           32 g            49 x
         16 Q           33 h            50 y

   Special processing is performed if fewer than 24 bits are available
   at the end of the data being encoded.  A full encoding quantum is
   always completed at the end of a quantity.  When fewer than 24 input
   bits are available in an input group, zero bits are added (on the
   right) to form an integral number of 6-bit groups.  Padding at the
   end of the data is performed using the '=' character.

   Since all base64 input is an integral number of octets, only the
         -------------------------------------------------
   following cases can arise:

       (1) the final quantum of encoding input is an integral
           multiple of 24 bits; here, the final unit of encoded
       output will be an integral multiple of 4 characters
       with no "=" padding,
       (2) the final quantum of encoding input is exactly 8 bits;
           here, the final unit of encoded output will be two
       characters followed by two "=" padding characters, or
       (3) the final quantum of encoding input is exactly 16 bits;
           here, the final unit of encoded output will be three
       characters followed by one "=" padding character.
   */

void Anope::B64Encode(const Anope::string &src, Anope::string &target) {
    size_t src_pos = 0, src_len = src.length();
    unsigned char input[3] = { '\0', '\0', '\0' };

    target.clear();

    while (src_len - src_pos > 2) {
        input[0] = src[src_pos++];
        input[1] = src[src_pos++];
        input[2] = src[src_pos++];

        target += Base64[input[0] >> 2];
        target += Base64[((input[0] & 0x03) << 4) + (input[1] >> 4)];
        target += Base64[((input[1] & 0x0f) << 2) + (input[2] >> 6)];
        target += Base64[input[2] & 0x3f];
    }

    /* Now we worry about padding */
    if (src_pos != src_len) {
        input[0] = input[1] = input[2] = 0;
        for (size_t i = 0; i < src_len - src_pos; ++i) {
            input[i] = src[src_pos + i];
        }

        target += Base64[input[0] >> 2];
        target += Base64[((input[0] & 0x03) << 4) + (input[1] >> 4)];
        if (src_pos == src_len - 1) {
            target += Pad64;
        } else {
            target += Base64[((input[1] & 0x0f) << 2) + (input[2] >> 6)];
        }
        target += Pad64;
    }
}

/* skips all whitespace anywhere.
   converts characters, four at a time, starting at (or after)
   src from base - 64 numbers into three 8 bit bytes in the target area.
 */

void Anope::B64Decode(const Anope::string &src, Anope::string &target) {
    target.clear();

    unsigned state = 0;
    Anope::string::const_iterator ch = src.begin(), end = src.end();
    for (; ch != end; ++ch) {
        if (isspace(*ch)) { /* Skip whitespace anywhere */
            continue;
        }

        if (*ch == Pad64) {
            break;
        }

        size_t pos = Base64.find(*ch);
        if (pos == Anope::string::npos) { /* A non-base64 character */
            return;
        }

        switch (state) {
        case 0:
            target += pos << 2;
            state = 1;
            break;
        case 1:
            target[target.length() - 1] |= pos >> 4;
            target += (pos & 0x0f) << 4;
            state = 2;
            break;
        case 2:
            target[target.length() - 1] |= pos >> 2;
            target += (pos & 0x03) << 6;
            state = 3;
            break;
        case 3:
            target[target.length() - 1] |= pos;
            state = 0;
        }
    }
    if (!target.empty() && !target[target.length() - 1]) {
        target.erase(target.length() - 1);
    }
}
