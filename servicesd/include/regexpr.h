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

#ifndef REGEXPR_H
#define REGEXPR_H

#include "services.h"
#include "anope.h"
#include "service.h"

class RegexException : public CoreException {
  public:
    RegexException(const Anope::string &reason = "") : CoreException(reason) { }

    virtual ~RegexException() throw() { }
};

class CoreExport Regex {
    Anope::string expression;
  protected:
    Regex(const Anope::string &expr) : expression(expr) { }
  public:
    virtual ~Regex() { }
    const Anope::string &GetExpression() {
        return expression;
    }
    virtual bool Matches(const Anope::string &str) = 0;
};

class CoreExport RegexProvider : public Service {
  public:
    RegexProvider(Module *o, const Anope::string &n) : Service(o, "Regex", n) { }
    virtual Regex *Compile(const Anope::string &) = 0;
};

#endif // REGEXPR_H
