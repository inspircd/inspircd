/*
 *
 * (C) 2011-2023 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 */

#ifndef OS_NEWS
#define OS_NEWS

enum NewsType {
    NEWS_LOGON,
    NEWS_RANDOM,
    NEWS_OPER
};

struct NewsMessages {
    NewsType type;
    Anope::string name;
    const char *msgs[10];
};

struct NewsItem : Serializable {
    NewsType type;
    Anope::string text;
    Anope::string who;
    time_t time;

    NewsItem() : Serializable("NewsItem") { }
};

class NewsService : public Service {
  public:
    NewsService(Module *m) : Service(m, "NewsService", "news") { }

    virtual NewsItem *CreateNewsItem() = 0;

    virtual void AddNewsItem(NewsItem *n) = 0;

    virtual void DelNewsItem(NewsItem *n) = 0;

    virtual std::vector<NewsItem *> &GetNewsList(NewsType t) = 0;
};

static ServiceReference<NewsService> news_service("NewsService", "news");

#endif // OS_NEWS
