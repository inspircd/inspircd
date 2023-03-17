/*
 *
 * (C) 2012-2023 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 */

#ifndef ANOPE_HTTPD_H
#define ANOPE_HTTPD_H

enum HTTPError {
    HTTP_ERROR_OK = 200,
    HTTP_FOUND = 302,
    HTTP_BAD_REQUEST = 400,
    HTTP_PAGE_NOT_FOUND = 404,
    HTTP_NOT_SUPPORTED = 505
};

/* A message to someone */
struct HTTPReply {
    HTTPError error;
    Anope::string content_type;
    std::map<Anope::string, Anope::string, ci::less> headers;
    typedef std::list<std::pair<Anope::string, Anope::string> > cookie;
    std::vector<cookie> cookies;

    HTTPReply() : error(HTTP_ERROR_OK), length(0) { }

    HTTPReply(const HTTPReply& other) : error(other.error), length(other.length) {
        content_type = other.content_type;
        headers = other.headers;
        cookies = other.cookies;

        for (unsigned i = 0; i < other.out.size(); ++i) {
            out.push_back(new Data(other.out[i]->buf, other.out[i]->len));
        }
    }

    ~HTTPReply() {
        for (unsigned i = 0; i < out.size(); ++i) {
            delete out[i];
        }
        out.clear();
    }

    struct Data {
        char *buf;
        size_t len;

        Data(const char *b, size_t l) {
            this->buf = new char[l];
            memcpy(this->buf, b, l);
            this->len = l;
        }

        ~Data() {
            delete [] buf;
        }
    };

    std::deque<Data *> out;
    size_t length;

    void Write(const Anope::string &message) {
        this->out.push_back(new Data(message.c_str(), message.length()));
        this->length += message.length();
    }

    void Write(const char *b, size_t l) {
        this->out.push_back(new Data(b, l));
        this->length += l;
    }
};

/* A message from someone */
struct HTTPMessage {
    std::map<Anope::string, Anope::string> headers;
    std::map<Anope::string, Anope::string> cookies;
    std::map<Anope::string, Anope::string> get_data;
    std::map<Anope::string, Anope::string> post_data;
    Anope::string content;
};

class HTTPClient;
class HTTPProvider;

class HTTPPage : public Base {
    Anope::string url;
    Anope::string content_type;

  public:
    HTTPPage(const Anope::string &u,
             const Anope::string &ct = "text/html") : url(u), content_type(ct) { }

    const Anope::string &GetURL() const {
        return this->url;
    }

    const Anope::string &GetContentType() const {
        return this->content_type;
    }

    /** Called when this page is requested
     * @param The server this page is on
     * @param The page name
     * @param The client requesting the page
     * @param The HTTP header sent from the client to request the page
     * @param The HTTP header that will be sent back to the client
     */
    virtual bool OnRequest(HTTPProvider *, const Anope::string &, HTTPClient *,
                           HTTPMessage &, HTTPReply &) = 0;
};

class HTTPClient : public ClientSocket, public BinarySocket, public Base {
  protected:
    void WriteClient(const Anope::string &message) {
        BinarySocket::Write(message + "\r\n");
    }

  public:
    HTTPClient(ListenSocket *l, int f, const sockaddrs &a) : ClientSocket(l, a),
        BinarySocket() { }

    virtual const Anope::string GetIP() {
        return this->clientaddr.addr();
    }

    virtual void SendError(HTTPError err, const Anope::string &msg) = 0;
    virtual void SendReply(HTTPReply *) = 0;
};

class HTTPProvider : public ListenSocket, public Service {
    Anope::string ip;
    unsigned short port;
    bool ssl;
  public:
    std::vector<Anope::string> ext_ips;
    std::vector<Anope::string> ext_headers;

    HTTPProvider(Module *c, const Anope::string &n, const Anope::string &i,
                 const unsigned short p, bool s) : ListenSocket(i, p,
                             i.find(':') != Anope::string::npos), Service(c, "HTTPProvider", n), ip(i),
        port(p), ssl(s) { }

    const Anope::string &GetIP() const {
        return this->ip;
    }

    unsigned short GetPort() const {
        return this->port;
    }

    bool IsSSL() const {
        return this->ssl;
    }

    virtual bool RegisterPage(HTTPPage *page) = 0;
    virtual void UnregisterPage(HTTPPage *page) = 0;
    virtual HTTPPage* FindPage(const Anope::string &name) = 0;
};

namespace HTTPUtils {
inline Anope::string URLDecode(const Anope::string &url) {
    Anope::string decoded;

    for (unsigned i = 0; i < url.length(); ++i) {
        const char& c = url[i];

        if (c == '%' && i + 2 < url.length()) {
            Anope::string dest;
            Anope::Unhex(url.substr(i + 1, 2), dest);
            decoded += dest;
            i += 2;
        } else if (c == '+') {
            decoded += ' ';
        } else {
            decoded += c;
        }
    }

    return decoded;
}

inline Anope::string URLEncode(const Anope::string &url) {
    Anope::string encoded;

    for (unsigned i = 0; i < url.length(); ++i) {
        const char& c = url[i];

        if (isalnum(c) || c == '.' || c == '-' || c == '*' || c == '_') {
            encoded += c;
        } else if (c == ' ') {
            encoded += '+';
        } else {
            encoded += "%" + Anope::Hex(c);
        }
    }

    return encoded;
}

inline Anope::string Escape(const Anope::string &src) {
    Anope::string dst;

    for (unsigned i = 0; i < src.length(); ++i) {
        switch (src[i]) {
        case '<':
            dst += "&lt;";
            break;
        case '>':
            dst += "&gt;";
            break;
        case '"':
            dst += "&quot;";
            break;
        case '&':
            dst += "&amp;";
            break;
        default:
            dst += src[i];
        }
    }

    return dst;
}
}

#endif // ANOPE_HTTPD_H
