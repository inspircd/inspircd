/* ChanServ core functions
 *
 * (C) 2003-2023 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 *
 * Based on the original code of Epona by Lara.
 * Based on the original code of Services by Andy Church.
 */

struct LogSetting {
    Anope::string chan;
    /* Our service name of the command */
    Anope::string service_name;
    /* The name of the client the command is on */
    Anope::string command_service;
    /* Name of the command to the user, can have spaces */
    Anope::string command_name;
    Anope::string method, extra;
    Anope::string creator;
    time_t created;

    virtual ~LogSetting() { }
  protected:
    LogSetting() { }
};

struct LogSettings : Serialize::Checker<std::vector<LogSetting *> > {
    typedef std::vector<LogSetting *>::iterator iterator;

  protected:
    LogSettings() : Serialize::Checker<std::vector<LogSetting *> >("LogSetting") {
    }

  public:
    virtual ~LogSettings() { }
    virtual LogSetting *Create() = 0;
};
