/*
 *
 * (C) 2012-2023 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 */

/* RequiredLibraries: tre */

#include "module.h"
#include <tre/regex.h>

class TRERegex : public Regex {
    regex_t regbuf;

  public:
    TRERegex(const Anope::string &expr) : Regex(expr) {
        int err = regcomp(&this->regbuf, expr.c_str(), REG_EXTENDED | REG_NOSUB);
        if (err) {
            char buf[BUFSIZE];
            regerror(err, &this->regbuf, buf, sizeof(buf));
            regfree(&this->regbuf);
            throw RegexException("Error in regex " + expr + ": " + buf);
        }
    }

    ~TRERegex() {
        regfree(&this->regbuf);
    }

    bool Matches(const Anope::string &str) {
        return regexec(&this->regbuf, str.c_str(), 0, NULL, 0) == 0;
    }
};

class TRERegexProvider : public RegexProvider {
  public:
    TRERegexProvider(Module *creator) : RegexProvider(creator, "regex/tre") { }

    Regex *Compile(const Anope::string &expression) anope_override {
        return new TRERegex(expression);
    }
};

class ModuleRegexTRE : public Module {
    TRERegexProvider tre_regex_provider;

  public:
    ModuleRegexTRE(const Anope::string &modname,
                   const Anope::string &creator) : Module(modname, creator, EXTRA | VENDOR),
        tre_regex_provider(this) {
        this->SetPermanent(true);
    }

    ~ModuleRegexTRE() {
        for (std::list<XLineManager *>::iterator it =
                    XLineManager::XLineManagers.begin(); it != XLineManager::XLineManagers.end();
                ++it) {
            XLineManager *xlm = *it;
            const std::vector<XLine *> &xlines = xlm->GetList();

            for (unsigned int i = 0; i < xlines.size(); ++i) {
                XLine *x = xlines[i];

                if (x->regex && dynamic_cast<TRERegex *>(x->regex)) {
                    delete x->regex;
                    x->regex = NULL;
                }
            }
        }
    }
};

MODULE_INIT(ModuleRegexTRE)
