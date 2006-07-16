#include "inspircd.h"
#include "users.h"

bool lookup_dns(const std::string &nick);
void dns_poll(int fdcheck);
void ZapThisDns(int fd);
