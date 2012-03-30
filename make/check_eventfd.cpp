#include <sys/eventfd.h>

int main() {
	eventfd_t efd_data;
	int fd;

	fd = eventfd(0, EFD_NONBLOCK);
	eventfd_read(fd, &efd_data);

	return (fd < 0);
}
