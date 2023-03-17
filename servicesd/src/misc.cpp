/* Miscellaneous routines.
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
#include "build.h"
#include "modules.h"
#include "lists.h"
#include "config.h"
#include "bots.h"
#include "language.h"
#include "regexpr.h"
#include "sockets.h"

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifndef _WIN32
#include <sys/socket.h>
#include <netdb.h>
#endif

NumberList::NumberList(const Anope::string &list,
                       bool descending) : is_valid(true), desc(descending) {
    Anope::string error;
    commasepstream sep(list);
    Anope::string token;

    sep.GetToken(token);
    if (token.empty()) {
        token = list;
    }
    do {
        size_t t = token.find('-');

        if (t == Anope::string::npos) {
            try {
                unsigned num = convertTo<unsigned>(token, error, false);
                if (error.empty()) {
                    numbers.insert(num);
                }
            } catch (const ConvertException &) {
                error = "1";
            }

            if (!error.empty()) {
                if (!this->InvalidRange(list)) {
                    is_valid = false;
                    return;
                }
            }
        } else {
            Anope::string error2;
            try {
                unsigned num1 = convertTo<unsigned>(token.substr(0, t), error, false);
                unsigned num2 = convertTo<unsigned>(token.substr(t + 1), error2, false);
                if (error.empty() && error2.empty())
                    for (unsigned i = num1; i <= num2; ++i) {
                        numbers.insert(i);
                    }
            } catch (const ConvertException &) {
                error = "1";
            }

            if (!error.empty() || !error2.empty()) {
                if (!this->InvalidRange(list)) {
                    is_valid = false;
                    return;
                }
            }
        }
    } while (sep.GetToken(token));
}

NumberList::~NumberList() {
}

void NumberList::Process() {
    if (!is_valid) {
        return;
    }

    if (this->desc) {
        for (std::set<unsigned>::reverse_iterator it = numbers.rbegin(),
                it_end = numbers.rend(); it != it_end; ++it) {
            this->HandleNumber(*it);
        }
    } else {
        for (std::set<unsigned>::iterator it = numbers.begin(), it_end = numbers.end();
                it != it_end; ++it) {
            this->HandleNumber(*it);
        }
    }
}

void NumberList::HandleNumber(unsigned) {
}

bool NumberList::InvalidRange(const Anope::string &) {
    return true;
}

ListFormatter::ListFormatter(NickCore *acc) : nc(acc) {
}

ListFormatter &ListFormatter::AddColumn(const Anope::string &name) {
    this->columns.push_back(name);
    return *this;
}

void ListFormatter::AddEntry(const ListEntry &entry) {
    this->entries.push_back(entry);
}

bool ListFormatter::IsEmpty() const {
    return this->entries.empty();
}

void ListFormatter::Process(std::vector<Anope::string> &buffer) {
    std::vector<Anope::string> tcolumns;
    std::map<Anope::string, size_t> lengths;
    std::set<Anope::string> breaks;
    for (unsigned i = 0; i < this->columns.size(); ++i) {
        tcolumns.push_back(Language::Translate(this->nc, this->columns[i].c_str()));
        lengths[this->columns[i]] = tcolumns[i].length();
    }
    for (unsigned i = 0; i < this->entries.size(); ++i) {
        ListEntry &e = this->entries[i];
        for (unsigned j = 0; j < this->columns.size(); ++j)
            if (e[this->columns[j]].length() > lengths[this->columns[j]]) {
                lengths[this->columns[j]] = e[this->columns[j]].length();
            }
    }
    unsigned length = 0;
    for (std::map<Anope::string, size_t>::iterator it = lengths.begin(),
            it_end = lengths.end(); it != it_end; ++it) {
        /* Break lines at 80 chars */
        if (length > 80) {
            breaks.insert(it->first);
            length = 0;
        } else {
            length += it->second;
        }
    }

    /* Only put a list header if more than 1 column */
    if (this->columns.size() > 1) {
        Anope::string s;
        for (unsigned i = 0; i < this->columns.size(); ++i) {
            if (breaks.count(this->columns[i])) {
                buffer.push_back(s);
                s = "  ";
            } else if (!s.empty()) {
                s += "  ";
            }
            s += tcolumns[i];
            if (i + 1 != this->columns.size())
                for (unsigned j = tcolumns[i].length(); j < lengths[this->columns[i]]; ++j) {
                    s += " ";
                }
        }
        buffer.push_back(s);
    }

    for (unsigned i = 0; i < this->entries.size(); ++i) {
        ListEntry &e = this->entries[i];

        Anope::string s;
        for (unsigned j = 0; j < this->columns.size(); ++j) {
            if (breaks.count(this->columns[j])) {
                buffer.push_back(s);
                s = "  ";
            } else if (!s.empty()) {
                s += "  ";
            }
            s += e[this->columns[j]];
            if (j + 1 != this->columns.size())
                for (unsigned k = e[this->columns[j]].length(); k < lengths[this->columns[j]];
                        ++k) {
                    s += " ";
                }
        }
        buffer.push_back(s);
    }
}

InfoFormatter::InfoFormatter(NickCore *acc) : nc(acc), longest(0) {
}

void InfoFormatter::Process(std::vector<Anope::string> &buffer) {
    buffer.clear();

    for (std::vector<std::pair<Anope::string, Anope::string> >::iterator it =
                this->replies.begin(), it_end = this->replies.end(); it != it_end; ++it) {
        Anope::string s;
        for (unsigned i = it->first.length(); i < this->longest; ++i) {
            s += " ";
        }
        s += it->first + ": " + Language::Translate(this->nc, it->second.c_str());

        buffer.push_back(s);
    }
}

Anope::string& InfoFormatter::operator[](const Anope::string &key) {
    Anope::string tkey = Language::Translate(this->nc, key.c_str());
    if (tkey.length() > this->longest) {
        this->longest = tkey.length();
    }
    this->replies.push_back(std::make_pair(tkey, ""));
    return this->replies.back().second;
}

void InfoFormatter::AddOption(const Anope::string &opt) {
    Anope::string options = Language::Translate(this->nc, "Options");
    Anope::string *optstr = NULL;
    for (std::vector<std::pair<Anope::string, Anope::string> >::iterator it =
                this->replies.begin(), it_end = this->replies.end(); it != it_end; ++it) {
        if (it->first == options) {
            optstr = &it->second;
            break;
        }
    }
    if (!optstr) {
        optstr = &(*this)[_("Options")];
    }

    if (!optstr->empty()) {
        *optstr += ", ";
    }
    *optstr += Language::Translate(nc, opt.c_str());
}

bool Anope::IsFile(const Anope::string &filename) {
    struct stat fileinfo;
    if (!stat(filename.c_str(), &fileinfo)) {
        return true;
    }

    return false;
}

time_t Anope::DoTime(const Anope::string &s) {
    if (s.empty()) {
        return 0;
    }

    int amount = 0;
    Anope::string end;

    try {
        amount = convertTo<int>(s, end, false);
        if (!end.empty()) {
            switch (end[0]) {
            case 's':
                return amount;
            case 'm':
                return amount * 60;
            case 'h':
                return amount * 3600;
            case 'd':
                return amount * 86400;
            case 'w':
                return amount * 86400 * 7;
            case 'y':
                return amount * 86400 * 365;
            default:
                break;
            }
        }
    } catch (const ConvertException &) {
        amount = -1;
    }

    return amount;
}

Anope::string Anope::Duration(time_t t, const NickCore *nc) {
    /* We first calculate everything */
    time_t years = t / 31536000;
    time_t days = (t / 86400) % 365;
    time_t hours = (t / 3600) % 24;
    time_t minutes = (t / 60) % 60;
    time_t seconds = (t) % 60;

    if (!years && !days && !hours && !minutes) {
        return stringify(seconds) + " " + (seconds != 1 ? Language::Translate(nc,
                                           _("seconds")) : Language::Translate(nc, _("second")));
    } else {
        bool need_comma = false;
        Anope::string buffer;
        if (years) {
            buffer = stringify(years) + " " + (years != 1 ? Language::Translate(nc,
                                               _("years")) : Language::Translate(nc, _("year")));
            need_comma = true;
        }
        if (days) {
            buffer += need_comma ? ", " : "";
            buffer += stringify(days) + " " + (days != 1 ? Language::Translate(nc,
                                               _("days")) : Language::Translate(nc, _("day")));
            need_comma = true;
        }
        if (hours) {
            buffer += need_comma ? ", " : "";
            buffer += stringify(hours) + " " + (hours != 1 ? Language::Translate(nc,
                                                _("hours")) : Language::Translate(nc, _("hour")));
            need_comma = true;
        }
        if (minutes) {
            buffer += need_comma ? ", " : "";
            buffer += stringify(minutes) + " " + (minutes != 1 ? Language::Translate(nc,
                                                  _("minutes")) : Language::Translate(nc, _("minute")));
        }
        return buffer;
    }
}

Anope::string Anope::strftime(time_t t, const NickCore *nc, bool short_output) {
    tm tm = *localtime(&t);
    char buf[BUFSIZE];
    strftime(buf, sizeof(buf), Language::Translate(nc, _("%b %d %H:%M:%S %Y %Z")),
             &tm);
    if (short_output) {
        return buf;
    }
    if (t < Anope::CurTime) {
        return Anope::string(buf) + " " + Anope::printf(Language::Translate(nc,
                _("(%s ago)")), Duration(Anope::CurTime - t, nc).c_str(), nc);
    } else if (t > Anope::CurTime) {
        return Anope::string(buf) + " " + Anope::printf(Language::Translate(nc,
                _("(%s from now)")), Duration(t - Anope::CurTime, nc).c_str(), nc);
    } else {
        return Anope::string(buf) + " " + Language::Translate(nc, _("(now)"));
    }
}

Anope::string Anope::Expires(time_t expires, const NickCore *nc) {
    if (!expires) {
        return Language::Translate(nc, NO_EXPIRE);
    } else if (expires <= Anope::CurTime) {
        return Language::Translate(nc, _("expires momentarily"));
    } else {
        char buf[256];
        time_t diff = expires - Anope::CurTime + 59;

        if (diff >= 86400) {
            int days = diff / 86400;
            snprintf(buf, sizeof(buf), Language::Translate(nc,
                     days == 1 ? _("expires in %d day") : _("expires in %d days")), days);
        } else {
            if (diff <= 3600) {
                int minutes = diff / 60;
                snprintf(buf, sizeof(buf), Language::Translate(nc,
                         minutes == 1 ? _("expires in %d minute") : _("expires in %d minutes")),
                         minutes);
            } else {
                int hours = diff / 3600, minutes;
                diff -= hours * 3600;
                minutes = diff / 60;
                snprintf(buf, sizeof(buf), Language::Translate(nc, hours == 1
                         && minutes == 1 ? _("expires in %d hour, %d minute") : (hours == 1
                                 && minutes != 1 ? _("expires in %d hour, %d minutes") : (hours != 1
                                         && minutes == 1 ? _("expires in %d hours, %d minute") :
                                         _("expires in %d hours, %d minutes")))), hours, minutes);
            }
        }

        return buf;
    }
}

bool Anope::Match(const Anope::string &str, const Anope::string &mask,
                  bool case_sensitive, bool use_regex) {
    size_t s = 0, m = 0, str_len = str.length(), mask_len = mask.length();

    if (use_regex && mask_len >= 2 && mask[0] == '/'
            && mask[mask.length() - 1] == '/') {
        Anope::string stripped_mask = mask.substr(1, mask_len - 2);
        // This is often called with the same mask multiple times in a row, so cache it
        static Regex *r = NULL;

        if (r == NULL || r->GetExpression() != stripped_mask) {
            ServiceReference<RegexProvider> provider("Regex",
                    Config->GetBlock("options")->Get<const Anope::string>("regexengine"));
            if (provider) {
                try {
                    delete r;
                    r = NULL;
                    // This may throw
                    r = provider->Compile(stripped_mask);
                } catch (const RegexException &ex) {
                    Log(LOG_DEBUG) << ex.GetReason();
                }
            } else {
                delete r;
                r = NULL;
            }
        }

        if (r != NULL && r->Matches(str)) {
            return true;
        }

        // Fall through to non regex match
    }

    while (s < str_len && m < mask_len && mask[m] != '*') {
        char string = str[s], wild = mask[m];
        if (case_sensitive) {
            if (wild != string && wild != '?') {
                return false;
            }
        } else {
            if (Anope::tolower(wild) != Anope::tolower(string) && wild != '?') {
                return false;
            }
        }

        ++m;
        ++s;
    }

    size_t sp = Anope::string::npos, mp = Anope::string::npos;
    while (s < str_len) {
        char string = str[s], wild = mask[m];
        if (wild == '*') {
            if (++m == mask_len) {
                return 1;
            }

            mp = m;
            sp = s + 1;
        } else if (case_sensitive) {
            if (wild == string || wild == '?') {
                ++m;
                ++s;
            } else {
                m = mp;
                s = sp++;
            }
        } else {
            if (Anope::tolower(wild) == Anope::tolower(string) || wild == '?') {
                ++m;
                ++s;
            } else {
                m = mp;
                s = sp++;
            }
        }
    }

    if (m < mask_len && mask[m] == '*') {
        ++m;
    }

    return m == mask_len;
}

void Anope::Encrypt(const Anope::string &src, Anope::string &dest) {
    EventReturn MOD_RESULT;
    FOREACH_RESULT(OnEncrypt, MOD_RESULT, (src, dest));
    static_cast<void>(MOD_RESULT);
}

bool Anope::Decrypt(const Anope::string &src, Anope::string &dest) {
    size_t pos = src.find(':');
    if (pos == Anope::string::npos) {
        Log() << "Error: Anope::Decrypt() called with invalid password string (" << src
              << ")";
        return false;
    }
    Anope::string hashm(src.begin(), src.begin() + pos);

    EventReturn MOD_RESULT;
    FOREACH_RESULT(OnDecrypt, MOD_RESULT, (hashm, src, dest));
    if (MOD_RESULT == EVENT_ALLOW) {
        return true;
    }

    return false;
}

Anope::string Anope::printf(const char *fmt, ...) {
    va_list args;
    char buf[1024];
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    return buf;
}

Anope::string Anope::Hex(const Anope::string &data) {
    const char hextable[] = "0123456789abcdef";

    size_t l = data.length();
    Anope::string rv;
    for (size_t i = 0; i < l; ++i) {
        unsigned char c = data[i];
        rv += hextable[c >> 4];
        rv += hextable[c & 0xF];
    }
    return rv;
}

Anope::string Anope::Hex(const char *data, unsigned len) {
    const char hextable[] = "0123456789abcdef";

    Anope::string rv;
    for (size_t i = 0; i < len; ++i) {
        unsigned char c = data[i];
        rv += hextable[c >> 4];
        rv += hextable[c & 0xF];
    }
    return rv;
}

void Anope::Unhex(const Anope::string &src, Anope::string &dest) {
    size_t len = src.length();
    Anope::string rv;
    for (size_t i = 0; i + 1 < len; i += 2) {
        char h = Anope::tolower(src[i]), l = Anope::tolower(src[i + 1]);
        unsigned char byte = (h >= 'a' ? h - 'a' + 10 : h - '0') << 4;
        byte += (l >= 'a' ? l - 'a' + 10 : l - '0');
        rv += byte;
    }
    dest = rv;
}

void Anope::Unhex(const Anope::string &src, char *dest, size_t sz) {
    Anope::string d;
    Anope::Unhex(src, d);

    memcpy(dest, d.c_str(), std::min(d.length() + 1, sz));
}

int Anope::LastErrorCode() {
#ifndef _WIN32
    return errno;
#else
    return GetLastError();
#endif
}

const Anope::string Anope::LastError() {
#ifndef _WIN32
    return strerror(errno);
#else
    char errbuf[513];
    DWORD err = GetLastError();
    if (!err) {
        return "";
    }
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
                  err, 0, errbuf, 512, NULL);
    return errbuf;
#endif
}

Anope::string Anope::Version() {
#ifdef VERSION_GIT
    return stringify(VERSION_MAJOR) + "." + stringify(VERSION_MINOR) + "." +
           stringify(VERSION_PATCH) + VERSION_EXTRA + " (" + VERSION_GIT + ")";
#else
    return stringify(VERSION_MAJOR) + "." + stringify(VERSION_MINOR) + "." +
           stringify(VERSION_PATCH) + VERSION_EXTRA;
#endif
}

Anope::string Anope::VersionShort() {
    return stringify(VERSION_MAJOR) + "." + stringify(VERSION_MINOR) + "." +
           stringify(VERSION_PATCH);
}

Anope::string Anope::VersionBuildString() {
#ifdef REPRODUCIBLE_BUILD
    Anope::string s = "build #" + stringify(BUILD);
#else
    Anope::string s = "build #" + stringify(BUILD) + ", compiled " +
                      Anope::compiled;
#endif
    Anope::string flags;

#ifdef DEBUG_BUILD
    flags += "D";
#endif
#ifdef VERSION_GIT
    flags += "G";
#endif
#ifdef _WIN32
    flags += "W";
#endif

    if (!flags.empty()) {
        s += ", flags " + flags;
    }

    return s;
}

int Anope::VersionMajor() {
    return VERSION_MAJOR;
}
int Anope::VersionMinor() {
    return VERSION_MINOR;
}
int Anope::VersionPatch() {
    return VERSION_PATCH;
}

Anope::string Anope::NormalizeBuffer(const Anope::string &buf) {
    Anope::string newbuf;

    for (unsigned i = 0, end = buf.length(); i < end; ++i) {
        switch (buf[i]) {
        /* ctrl char */
        case 1:
        /* Bold ctrl char */
        case 2:
            break;
        /* Color ctrl char */
        case 3:
            /* If the next character is a digit, its also removed */
            if (isdigit(buf[i + 1])) {
                ++i;

                /* not the best way to remove colors
                 * which are two digit but no worse then
                 * how the Unreal does with +S - TSL
                 */
                if (isdigit(buf[i + 1])) {
                    ++i;
                }

                /* Check for background color code
                 * and remove it as well
                 */
                if (buf[i + 1] == ',') {
                    ++i;

                    if (isdigit(buf[i + 1])) {
                        ++i;
                    }
                    /* not the best way to remove colors
                     * which are two digit but no worse then
                     * how the Unreal does with +S - TSL
                     */
                    if (isdigit(buf[i + 1])) {
                        ++i;
                    }
                }
            }

            break;
        /* line feed char */
        case 10:
        /* carriage returns char */
        case 13:
        /* Reverse ctrl char */
        case 22:
        /* Italic ctrl char */
        case 29:
        /* Underline ctrl char */
        case 31:
            break;
        /* A valid char gets copied into the new buffer */
        default:
            newbuf += buf[i];
        }
    }

    return newbuf;
}

Anope::string Anope::Resolve(const Anope::string &host, int type) {
    Anope::string result = host;

    addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = type;

    Log(LOG_DEBUG_2) << "Resolver: BlockingQuery: Looking up " << host;

    addrinfo *addrresult = NULL;
    if (getaddrinfo(host.c_str(), NULL, &hints, &addrresult) == 0) {
        sockaddrs addr;
        memcpy(static_cast<void*>(&addr), addrresult->ai_addr, addrresult->ai_addrlen);
        result = addr.addr();
        Log(LOG_DEBUG_2) << "Resolver: " << host << " -> " << result;

        freeaddrinfo(addrresult);
    }

    return result;
}

Anope::string Anope::Random(size_t len) {
    char chars[] = {
        'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l',
        'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y',
        'z', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L',
        'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y',
        'Z', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9'
    };
    Anope::string buf;
    for (size_t i = 0; i < len; ++i) {
        buf.append(chars[rand() % sizeof(chars)]);
    }
    return buf;
}
