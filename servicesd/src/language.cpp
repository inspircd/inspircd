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

#include "services.h"
#include "modules.h"
#include "commands.h"
#include "config.h"
#include "language.h"

#if GETTEXT_FOUND
# include <libintl.h>
#endif

std::vector<Anope::string> Language::Languages;
std::vector<Anope::string> Language::Domains;

void Language::InitLanguages() {
#if GETTEXT_FOUND
    Log(LOG_DEBUG) << "Initializing Languages...";

    Languages.clear();

    if (!bindtextdomain("anope", Anope::LocaleDir.c_str())) {
        Log() << "Error calling bindtextdomain, " << Anope::LastError();
    } else {
        Log(LOG_DEBUG) << "Successfully bound anope to " << Anope::LocaleDir;
    }

    setlocale(LC_ALL, "");

    spacesepstream sep(
        Config->GetBlock("options")->Get<const Anope::string>("languages"));
    Anope::string language;
    while (sep.GetToken(language)) {
        const Anope::string &lang_name = Translate(language.c_str(), _("English"));
        if (lang_name == "English") {
            Log() << "Unable to use language " << language;
            continue;
        }

        Log(LOG_DEBUG) << "Found language " << language;
        Languages.push_back(language);
    }
#else
    Log() << "Unable to initialize languages, gettext is not installed";
#endif
}

const char *Language::Translate(const char *string) {
    return Translate("", string);
}

const char *Language::Translate(User *u, const char *string) {
    if (u && u->Account()) {
        return Translate(u->Account(), string);
    } else {
        return Translate("", string);
    }
}

const char *Language::Translate(const NickCore *nc, const char *string) {
    return Translate(nc ? nc->language.c_str() : "", string);
}

#if GETTEXT_FOUND

#if defined(__GLIBC__) && defined(__USE_GNU_GETTEXT)
extern "C" int _nl_msg_cat_cntr;
#endif

const char *Language::Translate(const char *lang, const char *string) {
    if (!string || !*string) {
        return "";
    }

    if (!lang || !*lang) {
        lang = Config->DefLanguage.c_str();
    }

#if defined(__GLIBC__) && defined(__USE_GNU_GETTEXT)
    ++_nl_msg_cat_cntr;
#endif

#ifdef _WIN32
    SetThreadLocale(MAKELCID(MAKELANGID(WindowsGetLanguage(lang), SUBLANG_DEFAULT),
                             SORT_DEFAULT));
#else
    /* First, set LANG and LANGUAGE env variables.
     * Some systems (Debian) don't care about this, so we must setlocale LC_ALL as well.
     * BUT if this call fails because the LANGUAGE env variable is set, setlocale resets
     * the locale to "C", which short circuits gettext and causes it to fail on systems that
     * use the LANGUAGE env variable. We must reset the locale to en_US (or, anything not
     * C or POSIX) then.
     */
    setenv("LANG", lang, 1);
    setenv("LANGUAGE", lang, 1);
    if (setlocale(LC_ALL, lang) == NULL) {
        setlocale(LC_ALL, "en_US");
    }
#endif
    const char *translated_string = dgettext("anope", string);
    for (unsigned i = 0; translated_string == string && i < Domains.size(); ++i) {
        translated_string = dgettext(Domains[i].c_str(), string);
    }
#ifdef _WIN32
    SetThreadLocale(MAKELCID(MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                             SORT_DEFAULT));
#else
    unsetenv("LANGUAGE");
    unsetenv("LANG");
    setlocale(LC_ALL, "");
#endif

    return translated_string;
}
#else
const char *Language::Translate(const char *lang, const char *string) {
    return string != NULL ? string : "";
}
#endif
