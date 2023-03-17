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

struct SuspendInfo {
    Anope::string what, by, reason;
    time_t when, expires;

    SuspendInfo() { }
    virtual ~SuspendInfo() { }
};
