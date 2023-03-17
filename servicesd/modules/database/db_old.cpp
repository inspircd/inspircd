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

#include "module.h"
#include "modules/os_session.h"
#include "modules/bs_kick.h"
#include "modules/cs_mode.h"
#include "modules/bs_badwords.h"
#include "modules/os_news.h"
#include "modules/suspend.h"
#include "modules/os_forbid.h"
#include "modules/cs_entrymsg.h"

#define READ(x) \
if (true) \
{ \
    if ((x) < 0) \
        printf("Error, the database is broken, line %d, trying to continue... no guarantee.\n", __LINE__); \
} \
else \
    static_cast<void>(0)

#define getc_db(f) (fgetc((f)->fp))
#define read_db(f, buf, len) (fread((buf), 1, (len), (f)->fp))
#define read_buffer(buf, f) ((read_db((f), (buf), sizeof(buf)) == sizeof(buf)) ? 0 : -1)

#define OLD_BI_PRIVATE  0x0001

#define OLD_NI_KILLPROTECT  0x00000001 /* Kill others who take this nick */
#define OLD_NI_SECURE       0x00000002 /* Don't recognize unless IDENTIFY'd */
#define OLD_NI_MSG          0x00000004 /* Use PRIVMSGs instead of NOTICEs */
#define OLD_NI_MEMO_HARDMAX 0x00000008 /* Don't allow user to change memo limit */
#define OLD_NI_MEMO_SIGNON  0x00000010 /* Notify of memos at signon and un-away */
#define OLD_NI_MEMO_RECEIVE 0x00000020 /* Notify of new memos when sent */
#define OLD_NI_PRIVATE      0x00000040 /* Don't show in LIST to non-servadmins */
#define OLD_NI_HIDE_EMAIL   0x00000080 /* Don't show E-mail in INFO */
#define OLD_NI_HIDE_MASK    0x00000100 /* Don't show last seen address in INFO */
#define OLD_NI_HIDE_QUIT    0x00000200 /* Don't show last quit message in INFO */
#define OLD_NI_KILL_QUICK   0x00000400 /* Kill in 20 seconds instead of 60 */
#define OLD_NI_KILL_IMMED   0x00000800 /* Kill immediately instead of in 60 sec */
#define OLD_NI_MEMO_MAIL    0x00010000 /* User gets email on memo */
#define OLD_NI_HIDE_STATUS  0x00020000 /* Don't show services access status */
#define OLD_NI_SUSPENDED    0x00040000 /* Nickname is suspended */
#define OLD_NI_AUTOOP       0x00080000 /* Autoop nickname in channels */

#define OLD_NS_NO_EXPIRE        0x0004     /* nick won't expire */
#define OLD_NS_VERBOTEN         0x0002

#define OLD_CI_KEEPTOPIC        0x00000001
#define OLD_CI_SECUREOPS        0x00000002
#define OLD_CI_PRIVATE          0x00000004
#define OLD_CI_TOPICLOCK        0x00000008
#define OLD_CI_RESTRICTED       0x00000010
#define OLD_CI_PEACE            0x00000020
#define OLD_CI_SECURE           0x00000040
#define OLD_CI_VERBOTEN         0x00000080
#define OLD_CI_ENCRYPTEDPW      0x00000100
#define OLD_CI_NO_EXPIRE        0x00000200
#define OLD_CI_MEMO_HARDMAX     0x00000400
#define OLD_CI_OPNOTICE         0x00000800
#define OLD_CI_SECUREFOUNDER    0x00001000
#define OLD_CI_SIGNKICK         0x00002000
#define OLD_CI_SIGNKICK_LEVEL   0x00004000
#define OLD_CI_XOP              0x00008000
#define OLD_CI_SUSPENDED        0x00010000

/* BotServ SET flags */
#define OLD_BS_DONTKICKOPS      0x00000001
#define OLD_BS_DONTKICKVOICES   0x00000002
#define OLD_BS_FANTASY          0x00000004
#define OLD_BS_SYMBIOSIS        0x00000008
#define OLD_BS_GREET            0x00000010
#define OLD_BS_NOBOT            0x00000020

/* BotServ Kickers flags */
#define OLD_BS_KICK_BOLDS       0x80000000
#define OLD_BS_KICK_COLORS      0x40000000
#define OLD_BS_KICK_REVERSES    0x20000000
#define OLD_BS_KICK_UNDERLINES  0x10000000
#define OLD_BS_KICK_BADWORDS    0x08000000
#define OLD_BS_KICK_CAPS        0x04000000
#define OLD_BS_KICK_FLOOD       0x02000000
#define OLD_BS_KICK_REPEAT      0x01000000

#define OLD_NEWS_LOGON  0
#define OLD_NEWS_OPER   1
#define OLD_NEWS_RANDOM 2

static struct mlock_info {
    char c;
    uint32_t m;
} mlock_infos[] = {
    {'i', 0x00000001},
    {'m', 0x00000002},
    {'n', 0x00000004},
    {'p', 0x00000008},
    {'s', 0x00000010},
    {'t', 0x00000020},
    {'R', 0x00000100},
    {'r', 0x00000200},
    {'c', 0x00000400},
    {'A', 0x00000800},
    {'K', 0x00002000},
    {'O', 0x00008000},
    {'Q', 0x00010000},
    {'S', 0x00020000},
    {'G', 0x00100000},
    {'C', 0x00200000},
    {'u', 0x00400000},
    {'z', 0x00800000},
    {'N', 0x01000000},
    {'M', 0x04000000}
};

static Anope::string hashm;

enum {
    LANG_EN_US, /* United States English */
    LANG_JA_JIS, /* Japanese (JIS encoding) */
    LANG_JA_EUC, /* Japanese (EUC encoding) */
    LANG_JA_SJIS, /* Japanese (SJIS encoding) */
    LANG_ES, /* Spanish */
    LANG_PT, /* Portugese */
    LANG_FR, /* French */
    LANG_TR, /* Turkish */
    LANG_IT, /* Italian */
    LANG_DE, /* German */
    LANG_CAT, /* Catalan */
    LANG_GR, /* Greek */
    LANG_NL, /* Dutch */
    LANG_RU, /* Russian */
    LANG_HUN, /* Hungarian */
    LANG_PL /* Polish */
};

static void process_mlock(ChannelInfo *ci, uint32_t lock, bool status,
                          uint32_t *limit, Anope::string *key) {
    ModeLocks *ml = ci->Require<ModeLocks>("modelocks");
    for (unsigned i = 0; i < (sizeof(mlock_infos) / sizeof(mlock_info)); ++i)
        if (lock & mlock_infos[i].m) {
            ChannelMode *cm = ModeManager::FindChannelModeByChar(mlock_infos[i].c);
            if (cm && ml) {
                if (limit && mlock_infos[i].c == 'l') {
                    ml->SetMLock(cm, status, stringify(*limit));
                } else if (key && mlock_infos[i].c == 'k') {
                    ml->SetMLock(cm, status, *key);
                } else {
                    ml->SetMLock(cm, status);
                }
            }
        }
}

static const char Base64[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static const char Pad64 = '=';

static void my_b64_encode(const Anope::string &src, Anope::string &target) {
    size_t src_pos = 0, src_len = src.length();
    unsigned char input[3];

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

static Anope::string Hex(const char *data, size_t l) {
    const char hextable[] = "0123456789abcdef";

    std::string rv;
    for (size_t i = 0; i < l; ++i) {
        unsigned char c = data[i];
        rv += hextable[c >> 4];
        rv += hextable[c & 0xF];
    }
    return rv;
}

static Anope::string GetLevelName(int level) {
    switch (level) {
    case 0:
        return "INVITE";
    case 1:
        return "AKICK";
    case 2:
        return "SET";
    case 3:
        return "UNBAN";
    case 4:
        return "AUTOOP";
    case 5:
        return "AUTODEOP";
    case 6:
        return "AUTOVOICE";
    case 7:
        return "OP";
    case 8:
        return "ACCESS_LIST";
    case 9:
        return "CLEAR";
    case 10:
        return "NOJOIN";
    case 11:
        return "ACCESS_CHANGE";
    case 12:
        return "MEMO";
    case 13:
        return "ASSIGN";
    case 14:
        return "BADWORDS";
    case 15:
        return "NOKICK";
    case 16:
        return "FANTASIA";
    case 17:
        return "SAY";
    case 18:
        return "GREET";
    case 19:
        return "VOICEME";
    case 20:
        return "VOICE";
    case 21:
        return "GETKEY";
    case 22:
        return "AUTOHALFOP";
    case 23:
        return "AUTOPROTECT";
    case 24:
        return "OPME";
    case 25:
        return "HALFOPME";
    case 26:
        return "HALFOP";
    case 27:
        return "PROTECTME";
    case 28:
        return "PROTECT";
    case 29:
        return "KICKME";
    case 30:
        return "KICK";
    case 31:
        return "SIGNKICK";
    case 32:
        return "BANME";
    case 33:
        return "BAN";
    case 34:
        return "TOPIC";
    case 35:
        return "INFO";
    default:
        return "INVALID";
    }
}

static char *strscpy(char *d, const char *s, size_t len) {
    char *d_orig = d;

    if (!len) {
        return d;
    }
    while (--len && (*d++ = *s++));
    *d = '\0';
    return d_orig;
}

struct dbFILE {
    int mode;               /* 'r' for reading, 'w' for writing */
    FILE *fp;               /* The normal file descriptor */
    char filename[1024];    /* Name of the database file */
};

static dbFILE *open_db_read(const char *service, const char *filename,
                            int version) {
    dbFILE *f;
    FILE *fp;
    int myversion;

    f = new dbFILE;
    strscpy(f->filename, (Anope::DataDir + "/" + filename).c_str(),
            sizeof(f->filename));
    f->mode = 'r';
    fp = fopen(f->filename, "rb");
    if (!fp) {
        Log() << "Can't read " << service << " database " << f->filename;
        delete f;
        return NULL;
    }
    f->fp = fp;
    myversion = fgetc(fp) << 24 | fgetc(fp) << 16 | fgetc(fp) << 8 | fgetc(fp);
    if (feof(fp)) {
        Log() << "Error reading version number on " << f->filename <<
              ": End of file detected.";
        delete f;
        return NULL;
    } else if (myversion < version) {
        Log() << "Unsupported database version (" << myversion << ") on " << f->filename
              << ".";
        delete f;
        return NULL;
    }
    return f;
}

void close_db(dbFILE *f) {
    fclose(f->fp);
    delete f;
}

static int read_int16(int16_t *ret, dbFILE *f) {
    int c1, c2;

    *ret = 0;

    c1 = fgetc(f->fp);
    c2 = fgetc(f->fp);
    if (c1 == EOF || c2 == EOF) {
        return -1;
    }
    *ret = c1 << 8 | c2;
    return 0;
}

static int read_uint16(uint16_t *ret, dbFILE *f) {
    int c1, c2;

    *ret = 0;

    c1 = fgetc(f->fp);
    c2 = fgetc(f->fp);
    if (c1 == EOF || c2 == EOF) {
        return -1;
    }
    *ret = c1 << 8 | c2;
    return 0;
}

static int read_string(Anope::string &str, dbFILE *f) {
    str.clear();
    uint16_t len;

    if (read_uint16(&len, f) < 0) {
        return -1;
    }
    if (len == 0) {
        return 0;
    }
    char *s = new char[len];
    if (len != fread(s, 1, len, f->fp)) {
        delete [] s;
        return -1;
    }
    str = s;
    delete [] s;
    return 0;
}

static int read_uint32(uint32_t *ret, dbFILE *f) {
    int c1, c2, c3, c4;

    *ret = 0;

    c1 = fgetc(f->fp);
    c2 = fgetc(f->fp);
    c3 = fgetc(f->fp);
    c4 = fgetc(f->fp);
    if (c1 == EOF || c2 == EOF || c3 == EOF || c4 == EOF) {
        return -1;
    }
    *ret = c1 << 24 | c2 << 16 | c3 << 8 | c4;
    return 0;
}

int read_int32(int32_t *ret, dbFILE *f) {
    int c1, c2, c3, c4;

    *ret = 0;

    c1 = fgetc(f->fp);
    c2 = fgetc(f->fp);
    c3 = fgetc(f->fp);
    c4 = fgetc(f->fp);
    if (c1 == EOF || c2 == EOF || c3 == EOF || c4 == EOF) {
        return -1;
    }
    *ret = c1 << 24 | c2 << 16 | c3 << 8 | c4;
    return 0;
}

static void LoadNicks() {
    ServiceReference<ForbidService> forbid("ForbidService", "forbid");
    dbFILE *f = open_db_read("NickServ", "nick.db", 14);
    if (f == NULL) {
        return;
    }
    for (int i = 0; i < 1024; ++i)
        for (int c; (c = getc_db(f)) == 1;) {
            Anope::string buffer;

            READ(read_string(buffer, f));
            NickCore *nc = new NickCore(buffer);

            const Anope::string settings[] = { "killprotect", "kill_quick", "ns_secure", "ns_private", "hide_email",
                                               "hide_mask", "hide_quit", "memo_signon", "memo_receive", "autoop", "msg", "ns_keepmodes"
                                             };
            for (unsigned j = 0; j < sizeof(settings) / sizeof(Anope::string); ++j) {
                nc->Shrink<bool>(settings[j].upper());
            }

            char pwbuf[32];
            READ(read_buffer(pwbuf, f));
            if (hashm == "plain") {
                my_b64_encode(pwbuf, nc->pass);
            } else if (hashm == "md5" || hashm == "oldmd5") {
                nc->pass = Hex(pwbuf, 16);
            } else if (hashm == "sha1") {
                nc->pass = Hex(pwbuf, 20);
            } else {
                nc->pass = Hex(pwbuf, strlen(pwbuf));
            }
            nc->pass = hashm + ":" + nc->pass;

            READ(read_string(buffer, f));
            nc->email = buffer;

            READ(read_string(buffer, f));
            if (!buffer.empty()) {
                nc->Extend<Anope::string>("greet", buffer);
            }

            uint32_t u32;
            READ(read_uint32(&u32, f));
            //nc->icq = u32;

            READ(read_string(buffer, f));
            //nc->url = buffer;

            READ(read_uint32(&u32, f));
            if (u32 & OLD_NI_KILLPROTECT) {
                nc->Extend<bool>("KILLPROTECT");
            }
            if (u32 & OLD_NI_SECURE) {
                nc->Extend<bool>("NS_SECURE");
            }
            if (u32 & OLD_NI_MSG) {
                nc->Extend<bool>("MSG");
            }
            if (u32 & OLD_NI_MEMO_HARDMAX) {
                nc->Extend<bool>("MEMO_HARDMAX");
            }
            if (u32 & OLD_NI_MEMO_SIGNON) {
                nc->Extend<bool>("MEMO_SIGNON");
            }
            if (u32 & OLD_NI_MEMO_RECEIVE) {
                nc->Extend<bool>("MEMO_RECEIVE");
            }
            if (u32 & OLD_NI_PRIVATE) {
                nc->Extend<bool>("NS_PRIVATE");
            }
            if (u32 & OLD_NI_HIDE_EMAIL) {
                nc->Extend<bool>("HIDE_EMAIL");
            }
            if (u32 & OLD_NI_HIDE_MASK) {
                nc->Extend<bool>("HIDE_MASK");
            }
            if (u32 & OLD_NI_HIDE_QUIT) {
                nc->Extend<bool>("HIDE_QUIT");
            }
            if (u32 & OLD_NI_KILL_QUICK) {
                nc->Extend<bool>("KILL_QUICK");
            }
            if (u32 & OLD_NI_KILL_IMMED) {
                nc->Extend<bool>("KILL_IMMED");
            }
            if (u32 & OLD_NI_MEMO_MAIL) {
                nc->Extend<bool>("MEMO_MAIL");
            }
            if (u32 & OLD_NI_HIDE_STATUS) {
                nc->Extend<bool>("HIDE_STATUS");
            }
            if (u32 & OLD_NI_SUSPENDED) {
                SuspendInfo si;
                si.what = nc->display;
                si.when = si.expires = 0;
                nc->Extend("NS_SUSPENDED", si);
            }
            if (!(u32 & OLD_NI_AUTOOP)) {
                nc->Extend<bool>("AUTOOP");
            }

            uint16_t u16;
            READ(read_uint16(&u16, f));
            switch (u16) {
            case LANG_ES:
                nc->language = "es_ES.UTF-8";
                break;
            case LANG_PT:
                nc->language = "pt_PT.UTF-8";
                break;
            case LANG_FR:
                nc->language = "fr_FR.UTF-8";
                break;
            case LANG_TR:
                nc->language = "tr_TR.UTF-8";
                break;
            case LANG_IT:
                nc->language = "it_IT.UTF-8";
                break;
            case LANG_DE:
                nc->language = "de_DE.UTF-8";
                break;
            case LANG_CAT:
                nc->language = "ca_ES.UTF-8"; // yes, iso639 defines catalan as CA
                break;
            case LANG_GR:
                nc->language = "el_GR.UTF-8";
                break;
            case LANG_NL:
                nc->language = "nl_NL.UTF-8";
                break;
            case LANG_RU:
                nc->language = "ru_RU.UTF-8";
                break;
            case LANG_HUN:
                nc->language = "hu_HU.UTF-8";
                break;
            case LANG_PL:
                nc->language = "pl_PL.UTF-8";
                break;
            case LANG_EN_US:
            case LANG_JA_JIS:
            case LANG_JA_EUC:
            case LANG_JA_SJIS: // these seem to be unused
            default:
                nc->language = "en";
            }

            READ(read_uint16(&u16, f));
            for (uint16_t j = 0; j < u16; ++j) {
                READ(read_string(buffer, f));
                nc->access.push_back(buffer);
            }

            int16_t i16;
            READ(read_int16(&i16, f));
            READ(read_int16(&nc->memos.memomax, f));
            for (int16_t j = 0; j < i16; ++j) {
                Memo *m = new Memo;
                READ(read_uint32(&u32, f));
                uint16_t flags;
                READ(read_uint16(&flags, f));
                int32_t tmp32;
                READ(read_int32(&tmp32, f));
                m->time = tmp32;
                char sbuf[32];
                READ(read_buffer(sbuf, f));
                m->sender = sbuf;
                READ(read_string(m->text, f));
                m->owner = nc->display;
                nc->memos.memos->push_back(m);
                m->mi = &nc->memos;
            }
            READ(read_uint16(&u16, f));
            READ(read_int16(&i16, f));

            Log(LOG_DEBUG) << "Loaded NickCore " << nc->display;
        }

    for (int i = 0; i < 1024; ++i)
        for (int c; (c = getc_db(f)) == 1;) {
            Anope::string nick, last_usermask, last_realname, last_quit;
            time_t time_registered, last_seen;

            READ(read_string(nick, f));
            READ(read_string(last_usermask, f));
            READ(read_string(last_realname, f));
            READ(read_string(last_quit, f));

            int32_t tmp32;
            READ(read_int32(&tmp32, f));
            time_registered = tmp32;
            READ(read_int32(&tmp32, f));
            last_seen = tmp32;

            uint16_t tmpu16;
            READ(read_uint16(&tmpu16, f));

            Anope::string core;
            READ(read_string(core, f));
            NickCore *nc = NickCore::Find(core);
            if (nc == NULL) {
                Log() << "Skipping coreless nick " << nick << " with core " << core;
                continue;
            }

            if (tmpu16 & OLD_NS_VERBOTEN) {
                if (!forbid) {
                    delete nc;
                    continue;
                }

                if (nc->display.find_first_of("?*") != Anope::string::npos) {
                    delete nc;
                    continue;
                }

                ForbidData *d = forbid->CreateForbid();
                d->mask = nc->display;
                d->creator = last_usermask;
                d->reason = last_realname;
                d->expires = 0;
                d->created = 0;
                d->type = FT_NICK;
                delete nc;
                forbid->AddForbid(d);
                continue;
            }

            NickAlias *na = new NickAlias(nick, nc);
            na->last_usermask = last_usermask;
            na->last_realname = last_realname;
            na->last_quit = last_quit;
            na->time_registered = time_registered;
            na->last_seen = last_seen;

            if (tmpu16 & OLD_NS_NO_EXPIRE) {
                na->Extend<bool>("NS_NO_EXPIRE");
            }

            Log(LOG_DEBUG) << "Loaded NickAlias " << na->nick;
        }

    close_db(f); /* End of section Ia */
}

static void LoadVHosts() {
    dbFILE *f = open_db_read("HostServ", "hosts.db", 3);
    if (f == NULL) {
        return;
    }

    for (int c; (c = getc_db(f)) == 1;) {
        Anope::string nick, ident, host, creator;
        int32_t vtime;

        READ(read_string(nick, f));
        READ(read_string(ident, f));
        READ(read_string(host, f));
        READ(read_string(creator, f));
        READ(read_int32(&vtime, f));

        NickAlias *na = NickAlias::Find(nick);
        if (na == NULL) {
            Log() << "Removing vhost for nonexistent nick " << nick;
            continue;
        }

        na->SetVhost(ident, host, creator, vtime);

        Log() << "Loaded vhost for " << na->nick;
    }

    close_db(f);
}

static void LoadBots() {
    dbFILE *f = open_db_read("Botserv", "bot.db", 10);
    if (f == NULL) {
        return;
    }

    for (int c; (c = getc_db(f)) == 1;) {
        Anope::string nick, user, host, real;
        int16_t flags, chancount;
        int32_t created;

        READ(read_string(nick, f));
        READ(read_string(user, f));
        READ(read_string(host, f));
        READ(read_string(real, f));
        READ(read_int16(&flags, f));
        READ(read_int32(&created, f));
        READ(read_int16(&chancount, f));

        BotInfo *bi = BotInfo::Find(nick, true);
        if (!bi) {
            bi = new BotInfo(nick, user, host, real);
        }
        bi->created = created;

        if (flags & OLD_BI_PRIVATE) {
            bi->oper_only = true;
        }

        Log(LOG_DEBUG) << "Loaded bot " << bi->nick;
    }

    close_db(f);
}

static void LoadChannels() {
    ServiceReference<ForbidService> forbid("ForbidService", "forbid");
    dbFILE *f = open_db_read("ChanServ", "chan.db", 16);
    if (f == NULL) {
        return;
    }

    for (int i = 0; i < 256; ++i)
        for (int c; (c = getc_db(f)) == 1;) {
            Anope::string buffer;
            char namebuf[64];
            READ(read_buffer(namebuf, f));
            ChannelInfo *ci = new ChannelInfo(namebuf);

            const Anope::string settings[] = { "keeptopic", "peace", "cs_private", "restricted", "cs_secure", "secureops", "securefounder",
                                               "signkick", "signkick_level", "topiclock", "persist", "noautoop", "cs_keepmodes"
                                             };
            for (unsigned j = 0; j < sizeof(settings) / sizeof(Anope::string); ++j) {
                ci->Shrink<bool>(settings[j].upper());
            }

            READ(read_string(buffer, f));
            ci->SetFounder(NickCore::Find(buffer));

            READ(read_string(buffer, f));
            ci->SetSuccessor(NickCore::Find(buffer));

            char pwbuf[32];
            READ(read_buffer(pwbuf, f));

            READ(read_string(ci->desc, f));
            READ(read_string(buffer, f));
            READ(read_string(buffer, f));

            int32_t tmp32;
            READ(read_int32(&tmp32, f));
            ci->time_registered = tmp32;

            READ(read_int32(&tmp32, f));
            ci->last_used = tmp32;

            READ(read_string(ci->last_topic, f));

            READ(read_buffer(pwbuf, f));
            ci->last_topic_setter = pwbuf;

            READ(read_int32(&tmp32, f));
            ci->last_topic_time = tmp32;

            uint32_t tmpu32;
            READ(read_uint32(&tmpu32, f));
            // Temporary flags cleanup
            tmpu32 &= ~0x80000000;
            if (tmpu32 & OLD_CI_KEEPTOPIC) {
                ci->Extend<bool>("KEEPTOPIC");
            }
            if (tmpu32 & OLD_CI_SECUREOPS) {
                ci->Extend<bool>("SECUREOPS");
            }
            if (tmpu32 & OLD_CI_PRIVATE) {
                ci->Extend<bool>("CS_PRIVATE");
            }
            if (tmpu32 & OLD_CI_TOPICLOCK) {
                ci->Extend<bool>("TOPICLOCK");
            }
            if (tmpu32 & OLD_CI_RESTRICTED) {
                ci->Extend<bool>("RESTRICTED");
            }
            if (tmpu32 & OLD_CI_PEACE) {
                ci->Extend<bool>("PEACE");
            }
            if (tmpu32 & OLD_CI_SECURE) {
                ci->Extend<bool>("CS_SECURE");
            }
            if (tmpu32 & OLD_CI_NO_EXPIRE) {
                ci->Extend<bool>("CS_NO_EXPIRE");
            }
            if (tmpu32 & OLD_CI_MEMO_HARDMAX) {
                ci->Extend<bool>("MEMO_HARDMAX");
            }
            if (tmpu32 & OLD_CI_SECUREFOUNDER) {
                ci->Extend<bool>("SECUREFOUNDER");
            }
            if (tmpu32 & OLD_CI_SIGNKICK) {
                ci->Extend<bool>("SIGNKICK");
            }
            if (tmpu32 & OLD_CI_SIGNKICK_LEVEL) {
                ci->Extend<bool>("SIGNKICK_LEVEL");
            }

            Anope::string forbidby, forbidreason;
            READ(read_string(forbidby, f));
            READ(read_string(forbidreason, f));
            if (tmpu32 & OLD_CI_SUSPENDED) {
                SuspendInfo si;
                si.what = ci->name;
                si.by = forbidby;
                si.reason = forbidreason;
                si.when = si.expires = 0;
                ci->Extend("CS_SUSPENDED", si);
            }
            bool forbid_chan = tmpu32 & OLD_CI_VERBOTEN;

            int16_t tmp16;
            READ(read_int16(&tmp16, f));
            ci->bantype = tmp16;

            READ(read_int16(&tmp16, f));
            if (tmp16 > 36) {
                tmp16 = 36;
            }
            for (int16_t j = 0; j < tmp16; ++j) {
                int16_t level;
                READ(read_int16(&level, f));

                if (level == ACCESS_INVALID) {
                    level = ACCESS_FOUNDER;
                }

                if (j == 10 && level < 0) { // NOJOIN
                    ci->Shrink<bool>("RESTRICTED");    // If CSDefRestricted was enabled this can happen
                }

                ci->SetLevel(GetLevelName(j), level);
            }

            bool xop = tmpu32 & OLD_CI_XOP;
            ServiceReference<AccessProvider> provider_access("AccessProvider",
                    "access/access"), provider_xop("AccessProvider", "access/xop");
            uint16_t tmpu16;
            READ(read_uint16(&tmpu16, f));
            for (uint16_t j = 0; j < tmpu16; ++j) {
                uint16_t in_use;
                READ(read_uint16(&in_use, f));
                if (in_use) {
                    ChanAccess *access = NULL;

                    if (xop) {
                        if (provider_xop) {
                            access = provider_xop->Create();
                        }
                    } else if (provider_access) {
                        access = provider_access->Create();
                    }

                    if (access) {
                        access->ci = ci;
                    }

                    int16_t level;
                    READ(read_int16(&level, f));
                    if (access) {
                        if (xop) {
                            switch (level) {
                            case 3:
                                access->AccessUnserialize("VOP");
                                break;
                            case 4:
                                access->AccessUnserialize("HOP");
                                break;
                            case 5:
                                access->AccessUnserialize("AOP");
                                break;
                            case 10:
                                access->AccessUnserialize("SOP");
                                break;
                            }
                        } else {
                            access->AccessUnserialize(stringify(level));
                        }
                    }

                    Anope::string mask;
                    READ(read_string(mask, f));
                    if (access) {
                        access->SetMask(mask, ci);
                    }

                    READ(read_int32(&tmp32, f));
                    if (access) {
                        access->last_seen = tmp32;
                        access->creator = "Unknown";
                        access->created = Anope::CurTime;

                        ci->AddAccess(access);
                    }
                }
            }

            READ(read_uint16(&tmpu16, f));
            for (uint16_t j = 0; j < tmpu16; ++j) {
                uint16_t flags;
                READ(read_uint16(&flags, f));
                if (flags & 0x0001) {
                    Anope::string mask, reason, creator;
                    READ(read_string(mask, f));
                    READ(read_string(reason, f));
                    READ(read_string(creator, f));
                    READ(read_int32(&tmp32, f));

                    ci->AddAkick(creator, mask, reason, tmp32);
                }
            }

            READ(read_uint32(&tmpu32, f)); // mlock on
            ci->Extend<uint32_t>("mlock_on", tmpu32);
            READ(read_uint32(&tmpu32, f)); // mlock off
            ci->Extend<uint32_t>("mlock_off", tmpu32);
            READ(read_uint32(&tmpu32, f)); // mlock limit
            ci->Extend<uint32_t>("mlock_limit", tmpu32);
            READ(read_string(buffer, f)); // key
            ci->Extend<Anope::string>("mlock_key", buffer);
            READ(read_string(buffer, f)); // +f
            READ(read_string(buffer, f)); // +L

            READ(read_int16(&tmp16, f));
            READ(read_int16(&ci->memos.memomax, f));
            for (int16_t j = 0; j < tmp16; ++j) {
                READ(read_uint32(&tmpu32, f));
                READ(read_uint16(&tmpu16, f));
                Memo *m = new Memo;
                READ(read_int32(&tmp32, f));
                m->time = tmp32;
                char sbuf[32];
                READ(read_buffer(sbuf, f));
                m->sender = sbuf;
                READ(read_string(m->text, f));
                m->owner = ci->name;
                ci->memos.memos->push_back(m);
                m->mi = &ci->memos;
            }

            READ(read_string(buffer, f));
            if (!buffer.empty()) {
                EntryMessageList *eml = ci->Require<EntryMessageList>("entrymsg");
                if (eml) {
                    EntryMsg *e = eml->Create();

                    e->chan = ci->name;
                    e->creator = "Unknown";
                    e->message = buffer;
                    e->when = Anope::CurTime;

                    (*eml)->push_back(e);
                }
            }

            READ(read_string(buffer, f));
            ci->bi = BotInfo::Find(buffer, true);

            READ(read_int32(&tmp32, f));
            if (tmp32 & OLD_BS_DONTKICKOPS) {
                ci->Extend<bool>("BS_DONTKICKOPS");
            }
            if (tmp32 & OLD_BS_DONTKICKVOICES) {
                ci->Extend<bool>("BS_DONTKICKVOICES");
            }
            if (tmp32 & OLD_BS_FANTASY) {
                ci->Extend<bool>("BS_FANTASY");
            }
            if (tmp32 & OLD_BS_GREET) {
                ci->Extend<bool>("BS_GREET");
            }
            if (tmp32 & OLD_BS_NOBOT) {
                ci->Extend<bool>("BS_NOBOT");
            }

            KickerData *kd = ci->Require<KickerData>("kickerdata");
            if (kd) {
                if (tmp32 & OLD_BS_KICK_BOLDS) {
                    kd->bolds = true;
                }
                if (tmp32 & OLD_BS_KICK_COLORS) {
                    kd->colors = true;
                }
                if (tmp32 & OLD_BS_KICK_REVERSES) {
                    kd->reverses = true;
                }
                if (tmp32 & OLD_BS_KICK_UNDERLINES) {
                    kd->underlines = true;
                }
                if (tmp32 & OLD_BS_KICK_BADWORDS) {
                    kd->badwords = true;
                }
                if (tmp32 & OLD_BS_KICK_CAPS) {
                    kd->caps = true;
                }
                if (tmp32 & OLD_BS_KICK_FLOOD) {
                    kd->flood = true;
                }
                if (tmp32 & OLD_BS_KICK_REPEAT) {
                    kd->repeat = true;
                }
            }

            READ(read_int16(&tmp16, f));
            for (int16_t j = 0; j < tmp16; ++j) {
                int16_t ttb;
                READ(read_int16(&ttb, f));
                if (j < TTB_SIZE && kd) {
                    kd->ttb[j] = ttb;
                }
            }

            READ(read_int16(&tmp16, f));
            if (kd) {
                kd->capsmin = tmp16;
            }
            READ(read_int16(&tmp16, f));
            if (kd) {
                kd->capspercent = tmp16;
            }
            READ(read_int16(&tmp16, f));
            if (kd) {
                kd->floodlines = tmp16;
            }
            READ(read_int16(&tmp16, f));
            if (kd) {
                kd->floodsecs = tmp16;
            }
            READ(read_int16(&tmp16, f));
            if (kd) {
                kd->repeattimes = tmp16;
            }

            BadWords *bw = ci->Require<BadWords>("badwords");
            READ(read_uint16(&tmpu16, f));
            for (uint16_t j = 0; j < tmpu16; ++j) {
                uint16_t in_use;
                READ(read_uint16(&in_use, f));
                if (in_use) {
                    READ(read_string(buffer, f));
                    uint16_t type;
                    READ(read_uint16(&type, f));

                    BadWordType bwtype = BW_ANY;
                    if (type == 1) {
                        bwtype = BW_SINGLE;
                    } else if (type == 2) {
                        bwtype = BW_START;
                    } else if (type == 3) {
                        bwtype = BW_END;
                    }

                    if (bw) {
                        bw->AddBadWord(buffer, bwtype);
                    }
                }
            }

            if (forbid_chan) {
                if (!forbid) {
                    delete ci;
                    continue;
                }

                if (ci->name.find_first_of("?*") != Anope::string::npos) {
                    delete ci;
                    continue;
                }

                ForbidData *d = forbid->CreateForbid();
                d->mask = ci->name;
                d->creator = forbidby;
                d->reason = forbidreason;
                d->expires = 0;
                d->created = 0;
                d->type = FT_CHAN;
                delete ci;
                forbid->AddForbid(d);
                continue;
            }

            Log(LOG_DEBUG) << "Loaded channel " << ci->name;
        }

    close_db(f);
}

static void LoadOper() {
    dbFILE *f = open_db_read("OperServ", "oper.db", 13);
    if (f == NULL) {
        return;
    }

    XLineManager *akill, *sqline, *snline, *szline;
    akill = sqline = snline = szline = NULL;

    for (std::list<XLineManager *>::iterator it =
                XLineManager::XLineManagers.begin(), it_end = XLineManager::XLineManagers.end();
            it != it_end; ++it) {
        XLineManager *xl = *it;
        if (xl->Type() == 'G') {
            akill = xl;
        } else if (xl->Type() == 'Q') {
            sqline = xl;
        } else if (xl->Type() == 'N') {
            snline = xl;
        } else if (xl->Type() == 'Z') {
            szline = xl;
        }
    }

    int32_t tmp32;
    READ(read_int32(&tmp32, f));
    READ(read_int32(&tmp32, f));

    int16_t capacity;
    read_int16(&capacity, f); // AKill count
    for (int16_t i = 0; i < capacity; ++i) {
        Anope::string user, host, by, reason;
        int32_t seton, expires;

        READ(read_string(user, f));
        READ(read_string(host, f));
        READ(read_string(by, f));
        READ(read_string(reason, f));
        READ(read_int32(&seton, f));
        READ(read_int32(&expires, f));

        if (!akill) {
            continue;
        }

        XLine *x = new XLine(user + "@" + host, by, expires, reason,
                             XLineManager::GenerateUID());
        x->created = seton;
        akill->AddXLine(x);
    }

    read_int16(&capacity, f); // SNLines
    for (int16_t i = 0; i < capacity; ++i) {
        Anope::string mask, by, reason;
        int32_t seton, expires;

        READ(read_string(mask, f));
        READ(read_string(by, f));
        READ(read_string(reason, f));
        READ(read_int32(&seton, f));
        READ(read_int32(&expires, f));

        if (!snline) {
            continue;
        }

        XLine *x = new XLine(mask, by, expires, reason, XLineManager::GenerateUID());
        x->created = seton;
        snline->AddXLine(x);
    }

    read_int16(&capacity, f); // SQLines
    for (int16_t i = 0; i < capacity; ++i) {
        Anope::string mask, by, reason;
        int32_t seton, expires;

        READ(read_string(mask, f));
        READ(read_string(by, f));
        READ(read_string(reason, f));
        READ(read_int32(&seton, f));
        READ(read_int32(&expires, f));

        if (!sqline) {
            continue;
        }

        XLine *x = new XLine(mask, by, expires, reason, XLineManager::GenerateUID());
        x->created = seton;
        sqline->AddXLine(x);
    }

    read_int16(&capacity, f); // SZLines
    for (int16_t i = 0; i < capacity; ++i) {
        Anope::string mask, by, reason;
        int32_t seton, expires;

        READ(read_string(mask, f));
        READ(read_string(by, f));
        READ(read_string(reason, f));
        READ(read_int32(&seton, f));
        READ(read_int32(&expires, f));

        if (!szline) {
            continue;
        }

        XLine *x = new XLine(mask, by, expires, reason, XLineManager::GenerateUID());
        x->created = seton;
        szline->AddXLine(x);
    }

    close_db(f);
}

static void LoadExceptions() {
    if (!session_service) {
        return;
    }

    dbFILE *f = open_db_read("OperServ", "exception.db", 9);
    if (f == NULL) {
        return;
    }

    int16_t num;
    READ(read_int16(&num, f));
    for (int i = 0; i < num; ++i) {
        Anope::string mask, reason;
        int16_t limit;
        char who[32];
        int32_t time, expires;

        READ(read_string(mask, f));
        READ(read_int16(&limit, f));
        READ(read_buffer(who, f));
        READ(read_string(reason, f));
        READ(read_int32(&time, f));
        READ(read_int32(&expires, f));

        Exception *exception = session_service->CreateException();
        exception->mask = mask;
        exception->limit = limit;
        exception->who = who;
        exception->time = time;
        exception->expires = expires;
        exception->reason = reason;
        session_service->AddException(exception);
    }

    close_db(f);
}

static void LoadNews() {
    if (!news_service) {
        return;
    }

    dbFILE *f = open_db_read("OperServ", "news.db", 9);

    if (f == NULL) {
        return;
    }

    int16_t n;
    READ(read_int16(&n, f));

    for (int16_t i = 0; i < n; i++) {
        int16_t type;
        NewsItem *ni = news_service->CreateNewsItem();

        READ(read_int16(&type, f));

        switch (type) {
        case OLD_NEWS_LOGON:
            ni->type = NEWS_LOGON;
            break;
        case OLD_NEWS_OPER:
            ni->type = NEWS_OPER;
            break;
        case OLD_NEWS_RANDOM:
            ni->type = NEWS_RANDOM;
            break;
        }

        int32_t unused;
        READ(read_int32(&unused, f));

        READ(read_string(ni->text, f));

        char who[32];
        READ(read_buffer(who, f));
        ni->who = who;

        int32_t tmp;
        READ(read_int32(&tmp, f));
        ni->time = tmp;

        news_service->AddNewsItem(ni);
    }

    close_db(f);
}

class DBOld : public Module {
    PrimitiveExtensibleItem<uint32_t> mlock_on, mlock_off, mlock_limit;
    PrimitiveExtensibleItem<Anope::string> mlock_key;

  public:
    DBOld(const Anope::string &modname,
          const Anope::string &creator) : Module(modname, creator, DATABASE | VENDOR),
        mlock_on(this, "mlock_on"), mlock_off(this, "mlock_off"), mlock_limit(this,
                "mlock_limit"), mlock_key(this, "mlock_key") {


        hashm = Config->GetModule(this)->Get<const Anope::string>("hash");

        if (hashm != "md5" && hashm != "oldmd5" && hashm != "sha1" && hashm != "plain"
                && hashm != "sha256") {
            throw ModuleException("Invalid hash method");
        }
    }

    EventReturn OnLoadDatabase() anope_override {
        LoadNicks();
        LoadVHosts();
        LoadBots();
        LoadChannels();
        LoadOper();
        LoadExceptions();
        LoadNews();

        return EVENT_STOP;
    }

    void OnUplinkSync(Server *s) anope_override {
        for (registered_channel_map::iterator it = RegisteredChannelList->begin(), it_end = RegisteredChannelList->end(); it != it_end; ++it) {
            ChannelInfo *ci = it->second;
            uint32_t *limit = mlock_limit.Get(ci);
            Anope::string *key = mlock_key.Get(ci);

            uint32_t *u = mlock_on.Get(ci);
            if (u) {
                process_mlock(ci, *u, true, limit, key);
                mlock_on.Unset(ci);
            }

            u = mlock_off.Get(ci);
            if (u) {
                process_mlock(ci, *u, false, limit, key);
                mlock_off.Unset(ci);
            }

            mlock_limit.Unset(ci);
            mlock_key.Unset(ci);

            if (ci->c) {
                ci->c->CheckModes();
            }
        }
    }
};

MODULE_INIT(DBOld)
