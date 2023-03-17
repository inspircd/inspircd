/*
 *
 * (C) 2010-2023 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 */

#include "httpd.h"

class XMLRPCRequest {
    std::map<Anope::string, Anope::string> replies;

  public:
    Anope::string name;
    Anope::string id;
    std::deque<Anope::string> data;
    HTTPReply& r;

    XMLRPCRequest(HTTPReply &_r) : r(_r) { }
    inline void reply(const Anope::string &dname, const Anope::string &ddata) {
        this->replies.insert(std::make_pair(dname, ddata));
    }
    inline const std::map<Anope::string, Anope::string> &get_replies() {
        return this->replies;
    }
};

class XMLRPCServiceInterface;

class XMLRPCEvent {
  public:
    virtual ~XMLRPCEvent() { }
    virtual bool Run(XMLRPCServiceInterface *iface, HTTPClient *client,
                     XMLRPCRequest &request) = 0;
};

class XMLRPCServiceInterface : public Service {
  public:
    XMLRPCServiceInterface(Module *creator,
                           const Anope::string &sname) : Service(creator, "XMLRPCServiceInterface",
                                       sname) { }

    virtual void Register(XMLRPCEvent *event) = 0;

    virtual void Unregister(XMLRPCEvent *event) = 0;

    virtual Anope::string Sanitize(const Anope::string &string) = 0;

    virtual void Reply(XMLRPCRequest &request) = 0;
};
