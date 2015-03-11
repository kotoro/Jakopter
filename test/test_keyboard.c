#include <linux/input.h>
#include <stdio.h>
#include <fcntl.h>
#define EV_RELEASED 0

int main(void) {
	char *device = "/dev/input/event3";
	int fd = open(device, O_RDONLY );
	if (fd < 0) {
		perror("error\t");
		return -1;
	}
	printf("test before\n");
	struct input_event event;
	unsigned int char_code = 0;
	while (1) {
		printf("test before read\n");
		int num_bytes = read(fd, &event, sizeof(struct input_event));
		printf("test after read\n");
		if (event.type != EV_KEY) {
			continue;
		}

		printf("Value : %d\nCode : %d", event.value, event.code);
	}

	return 0;
}