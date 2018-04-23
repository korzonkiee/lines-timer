#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include<pthread.h>
#include <signal.h>
#include <poll.h>

#define IN  0
#define OUT 1
 
#define LOW  0
#define HIGH 1
 
#define SW_START 10
#define SW_PAUSE 22
#define SW_STOP 27

#define RED_LED_OUT 24
#define GREEN_LED_OUT 23

int start_fd;
int pause_fd;
int stop_fd;
 
static int
GPIOExport(int pin)
{
    #define BUFFER_MAX 3
	char buffer[BUFFER_MAX];
	ssize_t bytes_written;
	int fd;
 
	fd = open("/sys/class/gpio/export", O_WRONLY);
	if (-1 == fd) {
		fprintf(stderr, "Failed to open export for writing!\n");
		return(-1);
	}
 
	bytes_written = snprintf(buffer, BUFFER_MAX, "%d", pin);
	write(fd, buffer, bytes_written);
	close(fd);
	return(0);
}
 
static int
GPIOUnexport(int pin)
{
	char buffer[BUFFER_MAX];
	ssize_t bytes_written;
	int fd;
 
	fd = open("/sys/class/gpio/unexport", O_WRONLY);
	if (-1 == fd) {
		fprintf(stderr, "Failed to open unexport for writing!\n");
		return(-1);
	}
 
	bytes_written = snprintf(buffer, BUFFER_MAX, "%d", pin);
	write(fd, buffer, bytes_written);
	close(fd);
	return(0);
}
 
static int
GPIODirection(int pin, int dir)
{
	static const char s_directions_str[]  = "in\0out";
 
    #define DIRECTION_MAX 35
	char path[DIRECTION_MAX];
	int fd;
 
	snprintf(path, DIRECTION_MAX, "/sys/class/gpio/gpio%d/direction", pin);
	fd = open(path, O_WRONLY);
	if (-1 == fd) {
		fprintf(stderr, "Failed to open gpio direction for writing!\n");
		return(-1);
	}
 
	if (-1 == write(fd, &s_directions_str[IN == dir ? 0 : 3], IN == dir ? 2 : 3)) {
		fprintf(stderr, "Failed to set direction!\n");
		return(-1);
	}
 
	close(fd);
	return(0);
}

static int
GPIOInterrupt(int pin)
{
	static const char s_directions_str[]  = "in\0out";
 
    #define DIRECTION_MAX 35
	char path[DIRECTION_MAX];
	int fd;
 
	snprintf(path, DIRECTION_MAX, "/sys/class/gpio/gpio%d/edge", pin);
	fd = open(path, O_WRONLY);
	if (-1 == fd) {
		fprintf(stderr, "Failed to open gpio edge for writing!\n");
		return(-1);
	}
 
	if (-1 == write(fd, "falling", 7)) {
		fprintf(stderr, "Failed to set direction!\n");
		return(-1);
	}
 
	close(fd);
	return(0);
}


static int
GPIOWrite(int pin, int value)
{
    #define VALUE_MAX 30
	static const char s_values_str[] = "01";
 
	char path[VALUE_MAX];
	int fd;
 
	snprintf(path, VALUE_MAX, "/sys/class/gpio/gpio%d/value", pin);
	fd = open(path, O_WRONLY);
	if (-1 == fd) {
		fprintf(stderr, "Failed to open gpio value for writing!\n");
		return(-1);
	}
 
	if (1 != write(fd, &s_values_str[LOW == value ? 0 : 1], 1)) {
		fprintf(stderr, "Failed to write value!\n");
		return(-1);
	}
 
	close(fd);
	return(0);
}

static void on_start_alarm(int signal) {
    printf("Start.");
    char value_str[3];
    GPIOWrite(GREEN_LED_OUT, 1);
    GPIOWrite(RED_LED_OUT, 0);
    if (-1 == read(start_fd, value_str, 3)) {
        fprintf(stderr, "Failed to read value!\n");
        return(-1);
    }
}

static void on_pause_alarm(int signal) {
    printf("Pause.");
    GPIOWrite(RED_LED_OUT, 1);
    GPIOWrite(GREEN_LED_OUT, 0);
    char value_str[3];
    if (-1 == read(pause_fd, value_str, 3)) {
        fprintf(stderr, "Failed to read value!\n");
        return(-1);
    }
}

static void on_stop_alarm(int signal) {
    GPIOWrite(RED_LED_OUT, 0);
    GPIOWrite(GREEN_LED_OUT, 0);
}

static int
GPIOOpen(int pin) {
	char path[VALUE_MAX];
	int fd;

	snprintf(path, VALUE_MAX, "/sys/class/gpio/gpio%d/value", pin);
	fd = open(path, O_RDONLY);
	if (-1 == fd) {
		fprintf(stderr, "Failed to open gpio value for reading!\n");
		return(-1);
	}

    return fd;
}

static void
GPIOClose(int fd) {
	close(fd);
}

static void
GPIOListenForButton(int start_fd, int pause_fd, int stop_fd) {
    #define VALUE_MAX 30
	char value_str[3];
    int timeout_msecs = -1;
    struct pollfd fds[3];

    lseek(start_fd, 0, SEEK_SET);
    lseek(pause_fd, 0, SEEK_SET);
    lseek(stop_fd, 0, SEEK_SET);

    fds[0].fd = start_fd;
    fds[0].events = POLLPRI;

    fds[1].fd = pause_fd;
    fds[1].events = POLLPRI;

    fds[2].fd = stop_fd;
    fds[2].events = POLLPRI;

    int ret = poll(fds, 3, timeout_msecs);
    if (ret > 0) {
        if (fds[0].revents & POLLPRI) {
            // Prevent `contact bounce` effect.
            signal(SIGALRM, on_start_alarm);
            ualarm(1000 * 100, 0);

            if (-1 == read(start_fd, value_str, 3)) {
                fprintf(stderr, "Failed to read value!\n");
                return(-1);
            }
        }

        if (fds[1].revents & POLLPRI) {
            // Prevent `contact bounce` effect.
            signal(SIGALRM, on_pause_alarm);
            ualarm(1000 * 100, 0);

            if (-1 == read(pause_fd, value_str, 3)) {
                fprintf(stderr, "Failed to read value!\n");
                return(-1);
            }
        }

        if (fds[2].revents & POLLPRI) {
            // Prevent `contact bounce` effect.
            signal(SIGALRM, on_stop_alarm);
            ualarm(1000 * 100, 0);

            if (-1 == read(stop_fd, value_str, 3)) {
                fprintf(stderr, "Failed to read value!\n");
                return(-1);
            }
        }
    }
 
	return(atoi(value_str));
}

int main() { 
	/*
	 * Enable GPIO pins
	 */
	if (GPIOExport(SW_START) || GPIOExport(SW_PAUSE) || GPIOExport(SW_STOP) ||
        GPIOExport(RED_LED_OUT) || GPIOExport(GREEN_LED_OUT))
		return(1);

	/*
	 * Set GPIO directions
	 */
	if (GPIODirection(SW_START, IN) || GPIODirection(SW_PAUSE, IN) || GPIODirection(SW_STOP, IN) ||
        GPIODirection(RED_LED_OUT, OUT) || GPIODirection(GREEN_LED_OUT, OUT))
		return(2);

    /*
     * Set GPIO edge
     */
    if (GPIOInterrupt(SW_START) || GPIOInterrupt(SW_PAUSE) || GPIOInterrupt(SW_STOP))
        return(3);

    start_fd = GPIOOpen(SW_START);
    pause_fd = GPIOOpen(SW_PAUSE);
    stop_fd = GPIOOpen(SW_STOP);

    if (start_fd == -1 || pause_fd == -1 || stop_fd == -1)
        return(4);

    while (1) {
        GPIOListenForButton(start_fd, pause_fd, stop_fd);
    }

    /*
     * Close GPIO files
     */
    GPIOClose(start_fd);
    GPIOClose(pause_fd);
    GPIOClose(stop_fd);
    
	/*
	 * Disable GPIO pins
	 */
	if (-1 == GPIOUnexport(SW_START) || -1 == GPIOUnexport(SW_PAUSE) || -1 == GPIOUnexport(SW_STOP))
		return(4);
 
	return(0);
}

void start_timer() {

}