/*
 *
 * (C) 2010-2023 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 */

#include "module.h"
#include "modules/xmlrpc.h"
#include "modules/httpd.h"

static struct special_chars {
    Anope::string character;
    Anope::string replace;

    special_chars(const Anope::string &c, const Anope::string &r) : character(c),
        replace(r) { }
}
special[] = {
    special_chars("&", "&amp;"),
    special_chars("\"", "&quot;"),
    special_chars("<", "&lt;"),
    special_chars(">", "&qt;"),
    special_chars("'", "&#39;"),
    special_chars("\n", "&#xA;"),
    special_chars("\002", ""), // bold
    special_chars("\003", ""), // color
    special_chars("\035", ""), // italics
    special_chars("\037", ""), // underline
    special_chars("\026", ""), // reverses
    special_chars("", "")
};

class MyXMLRPCServiceInterface : public XMLRPCServiceInterface,
    public HTTPPage {
    std::deque<XMLRPCEvent *> events;

  public:
    MyXMLRPCServiceInterface(Module *creator,
                             const Anope::string &sname) : XMLRPCServiceInterface(creator, sname),
        HTTPPage("/xmlrpc", "text/xml") { }

    void Register(XMLRPCEvent *event) anope_override {
        this->events.push_back(event);
    }

    void Unregister(XMLRPCEvent *event) anope_override {
        std::deque<XMLRPCEvent *>::iterator it = std::find(this->events.begin(), this->events.end(), event);

        if (it != this->events.end()) {
            this->events.erase(it);
        }
    }

    Anope::string Sanitize(const Anope::string &string) anope_override {
        Anope::string ret = string;
        for (int i = 0; special[i].character.empty() == false; ++i) {
            ret = ret.replace_all_cs(special[i].character, special[i].replace);
        }
        return ret;
    }

    static Anope::string Unescape(const Anope::string &string) {
        Anope::string ret = string;
        for (int i = 0; special[i].character.empty() == false; ++i)
            if (!special[i].replace.empty()) {
                ret = ret.replace_all_cs(special[i].replace, special[i].character);
            }

        for (size_t i, last = 0; (i = string.find("&#", last)) != Anope::string::npos;
            ) {
            last = i + 1;

            size_t end = string.find(';', i);
            if (end == Anope::string::npos) {
                break;
            }

            Anope::string ch = string.substr(i + 2, end - (i + 2));

            if (ch.empty()) {
                continue;
            }

            long l;
            if (!ch.empty() && ch[0] == 'x') {
                l = strtol(ch.substr(1).c_str(), NULL, 16);
            } else {
                l = strtol(ch.c_str(), NULL, 10);
            }

            if (l > 0 && l < 256) {
                ret = ret.replace_all_cs("&#" + ch + ";", Anope::string(l));
            }
        }

        return ret;
    }

  private:
    static bool GetData(Anope::string &content, Anope::string &tag,
                        Anope::string &data) {
        if (content.empty()) {
            return false;
        }

        Anope::string prev, cur;
        bool istag;

        do {
            prev = cur;
            cur.clear();

            size_t len = 0;
            istag = false;

            if (content[0] == '<') {
                len = content.find_first_of('>');
                istag = true;
            } else if (content[0] != '>') {
                len = content.find_first_of('<');
            }

            // len must advance
            if (len == Anope::string::npos || len == 0) {
                break;
            }

            if (istag) {
                cur = content.substr(1, len - 1);
                content.erase(0, len + 1);
                while (!content.empty() && content[0] == ' ') {
                    content.erase(content.begin());
                }
            } else {
                cur = content.substr(0, len);
                content.erase(0, len);
            }
        } while (istag && !content.empty());

        tag = Unescape(prev);
        data = Unescape(cur);
        return !istag && !data.empty();
    }

  public:
    bool OnRequest(HTTPProvider *provider, const Anope::string &page_name,
                   HTTPClient *client, HTTPMessage &message, HTTPReply &reply) anope_override {
        Anope::string content = message.content, tname, data;
        XMLRPCRequest request(reply);

        while (GetData(content, tname, data)) {
            Log(LOG_DEBUG) << "m_xmlrpc: Tag name: " << tname << ", data: " << data;
            if (tname == "methodName") {
                request.name = data;
            } else if (tname == "name" && data == "id") {
                GetData(content, tname, data);
                request.id = data;
            } else if (tname == "string") {
                request.data.push_back(data);
            }
        }

        for (unsigned i = 0; i < this->events.size(); ++i) {
            XMLRPCEvent *e = this->events[i];

            if (!e->Run(this, client, request)) {
                return false;
            } else if (!request.get_replies().empty()) {
                this->Reply(request);
                return true;
            }
        }

        reply.error = HTTP_PAGE_NOT_FOUND;
        reply.Write("Unrecognized query");
        return true;
    }

    void Reply(XMLRPCRequest &request) anope_override {
        if (!request.id.empty()) {
            request.reply("id", request.id);
        }

        Anope::string r = "<?xml version=\"1.0\" encoding=\"iso-8859-1\"?>\n<methodResponse>\n<params>\n<param>\n<value>\n<struct>\n";
        for (std::map<Anope::string, Anope::string>::const_iterator it = request.get_replies().begin(); it != request.get_replies().end(); ++it) {
            r += "<member>\n<name>" + it->first + "</name>\n<value>\n<string>" +
            this->Sanitize(it->second) + "</string>\n</value>\n</member>\n";
        }
        r += "</struct>\n</value>\n</param>\n</params>\n</methodResponse>";

        request.r.Write(r);
    }
};

class ModuleXMLRPC : public Module {
    ServiceReference<HTTPProvider> httpref;
  public:
    MyXMLRPCServiceInterface xmlrpcinterface;

    ModuleXMLRPC(const Anope::string &modname,
                 const Anope::string &creator) : Module(modname, creator, EXTRA | VENDOR),
        xmlrpcinterface(this, "xmlrpc") {

    }

    ~ModuleXMLRPC() {
        if (httpref) {
            httpref->UnregisterPage(&xmlrpcinterface);
        }
    }

    void OnReload(Configuration::Conf *conf) anope_override {
        if (httpref) {
            httpref->UnregisterPage(&xmlrpcinterface);
        }
        this->httpref = ServiceReference<HTTPProvider>("HTTPProvider", conf->GetModule(this)->Get<const Anope::string>("server", "httpd/main"));
        if (!httpref) {
            throw ConfigException("Unable to find http reference, is m_httpd loaded?");
        }
        httpref->RegisterPage(&xmlrpcinterface);
    }
};

MODULE_INIT(ModuleXMLRPC)
