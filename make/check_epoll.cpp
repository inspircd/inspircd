#include <sys/epoll.h>

int main() {
	int fd = epoll_create(1);
	return (fd < 0);
}
