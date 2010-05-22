#include <sys/eventfd.h>

int main() {
	int fd = eventfd(0, EFD_NONBLOCK);
	return (fd < 0);
}
