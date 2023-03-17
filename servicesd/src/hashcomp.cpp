/*
 *
 * (C) 2002-2011 InspIRCd Development Team
 * (C) 2008-2023 Anope Team <team@anope.org>
 *
 * Please read COPYING and README for further details.
 */

#include "services.h"
#include "hashcomp.h"
#include "anope.h"

/* Case map in use by Anope */
std::locale Anope::casemap = std::locale(std::locale(),
                             new Anope::ascii_ctype<char>());
/* Cache of the above case map, forced upper */
static unsigned char case_map_upper[256], case_map_lower[256];

/* called whenever Anope::casemap is modified to rebuild the casemap cache */
void Anope::CaseMapRebuild() {
    const std::ctype<char> &ct = std::use_facet<std::ctype<char> >(Anope::casemap);

    for (unsigned i = 0; i < sizeof(case_map_upper); ++i) {
        case_map_upper[i] = ct.toupper(i);
        case_map_lower[i] = ct.tolower(i);
    }
}

unsigned char Anope::tolower(unsigned char c) {
    return case_map_lower[c];
}

unsigned char Anope::toupper(unsigned char c) {
    return case_map_upper[c];
}

/*
 *
 * This is an implementation of a special string class, ci::string,
 * which is a case-insensitive equivalent to std::string.
 *
 */

bool ci::ci_char_traits::eq(char c1st, char c2nd) {
    return case_map_upper[static_cast<unsigned char>(c1st)] ==
           case_map_upper[static_cast<unsigned char>(c2nd)];
}

bool ci::ci_char_traits::ne(char c1st, char c2nd) {
    return !eq(c1st, c2nd);
}

bool ci::ci_char_traits::lt(char c1st, char c2nd) {
    return case_map_upper[static_cast<unsigned char>(c1st)] <
           case_map_upper[static_cast<unsigned char>(c2nd)];
}

int ci::ci_char_traits::compare(const char *str1, const char *str2, size_t n) {
    for (unsigned i = 0; i < n; ++i) {
        unsigned char c1 = case_map_upper[static_cast<unsigned char>(*str1)],
                      c2 = case_map_upper[static_cast<unsigned char>(*str2)];

        if (c1 > c2) {
            return 1;
        } else if (c1 < c2) {
            return -1;
        } else if (!c1 || !c2) {
            return 0;
        }

        ++str1;
        ++str2;
    }
    return 0;
}

const char *ci::ci_char_traits::find(const char *s1, int n, char c) {
    while (n-- > 0
            && case_map_upper[static_cast<unsigned char>(*s1)] !=
            case_map_upper[static_cast<unsigned char>(c)]) {
        ++s1;
    }
    return n >= 0 ? s1 : NULL;
}

bool ci::less::operator()(const Anope::string &s1,
                          const Anope::string &s2) const {
    return s1.ci_str().compare(s2.ci_str()) < 0;
}

sepstream::sepstream(const Anope::string &source, char separator,
                     bool ae) : tokens(source), sep(separator), pos(0), allow_empty(ae) {
}

bool sepstream::GetToken(Anope::string &token) {
    if (this->StreamEnd()) {
        token.clear();
        return false;
    }

    if (!this->allow_empty) {
        this->pos = this->tokens.find_first_not_of(this->sep, this->pos);
        if (this->pos == std::string::npos) {
            this->pos = this->tokens.length() + 1;
            token.clear();
            return false;
        }
    }

    size_t p = this->tokens.find(this->sep, this->pos);
    if (p == std::string::npos) {
        p = this->tokens.length();
    }

    token = this->tokens.substr(this->pos, p - this->pos);
    this->pos = p + 1;

    return true;
}

bool sepstream::GetToken(Anope::string &token, int num) {
    int i;
    for (i = 0; i < num + 1 && this->GetToken(token); ++i);
    return i == num + 1;
}

int sepstream::NumTokens() {
    int i;
    Anope::string token;
    for (i = 0; this->GetToken(token); ++i);
    return i;
}

bool sepstream::GetTokenRemainder(Anope::string &token, int num) {
    if (this->GetToken(token, num)) {
        if (!this->StreamEnd()) {
            token += sep + this->GetRemaining();
        }

        return true;
    }

    return false;
}

const Anope::string sepstream::GetRemaining() {
    return !this->StreamEnd() ? this->tokens.substr(this->pos) : "";
}

bool sepstream::StreamEnd() {
    return this->pos > this->tokens.length();
}
