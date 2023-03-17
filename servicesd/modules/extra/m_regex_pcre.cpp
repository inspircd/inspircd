/*
 *
 * (C) 2012-2023 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 */

/* RequiredLibraries: pcre */
/* RequiredWindowsLibraries: libpcre */

#include "module.h"
#include <pcre.h>

class PCRERegex : public Regex {
    pcre *regex;

  public:
    PCRERegex(const Anope::string &expr) : Regex(expr) {
        const char *error;
        int erroffset;
        this->regex = pcre_compile(expr.c_str(), PCRE_CASELESS, &error, &erroffset,
                                   NULL);
        if (!this->regex) {
            throw RegexException("Error in regex " + expr + " at offset " + stringify(
                                     erroffset) + ": " + error);
        }
    }

    ~PCRERegex() {
        pcre_free(this->regex);
    }

    bool Matches(const Anope::string &str) {
        return pcre_exec(this->regex, NULL, str.c_str(), str.length(), 0, 0, NULL,
                         0) > -1;
    }
};

class PCRERegexProvider : public RegexProvider {
  public:
    PCRERegexProvider(Module *creator) : RegexProvider(creator, "regex/pcre") { }

    Regex *Compile(const Anope::string &expression) anope_override {
        return new PCRERegex(expression);
    }
};

class ModuleRegexPCRE : public Module {
    PCRERegexProvider pcre_regex_provider;

  public:
    ModuleRegexPCRE(const Anope::string &modname,
                    const Anope::string &creator) : Module(modname, creator, EXTRA | VENDOR),
        pcre_regex_provider(this) {
        this->SetPermanent(true);
    }

    ~ModuleRegexPCRE() {
        for (std::list<XLineManager *>::iterator it =
                    XLineManager::XLineManagers.begin(); it != XLineManager::XLineManagers.end();
                ++it) {
            XLineManager *xlm = *it;
            const std::vector<XLine *> &xlines = xlm->GetList();

            for (unsigned int i = 0; i < xlines.size(); ++i) {
                XLine *x = xlines[i];

                if (x->regex && dynamic_cast<PCRERegex *>(x->regex)) {
                    delete x->regex;
                    x->regex = NULL;
                }
            }
        }
    }
};

MODULE_INIT(ModuleRegexPCRE)
