#include <sys/eventfd.h>

int main() {
	int fd = eventfd(0, 0);
	return (fd < 0);
}
