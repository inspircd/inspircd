/*
 *
 * (C) 2003-2023 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 */

struct MiscData {
    Anope::string object;
    Anope::string name;
    Anope::string data;

    MiscData() { }
    virtual ~MiscData() { }
};
