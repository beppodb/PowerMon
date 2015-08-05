/*
 ============================================================================
 Name        : application.c
 Author      : Daniel Bedard, updated by Jeff Young, 2015
 Version     : 1.0
 Copyright   : RENCI, 2011
 Description : This application reads data measured by the PowerMon sensors 
               via a USB interface, typically /dev/ttyUSB0 or /dev/ttyUSB1 under Linux 
 ============================================================================
 */

#include <sys/termios.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>

#define VERSION_H 0
#define VERSION_L 1
#define OVF_FLAG 7
#define TIME_FLAG 6 
#define DONE_FLAG 5

#define BAUDRATE B1000000
#define V_FULLSCALE 26.52
#define R_SENSE 0.00422 /* found empirically */
#define I_FULLSCALE .10584

int g_serial_fd;
FILE *g_serial_fp;
struct termios g_oldtio, g_newtio;

int powermon_init();

void cleanup() {
	/*		fprintf (stderr, "cleanup\n");*/
	powermon_init();
	tcflush(g_serial_fd, TCIFLUSH);
	tcsetattr(g_serial_fd, TCSANOW, &g_oldtio);
	close(g_serial_fd);
}

int powermon_init() {
	char buffer[32];
	fprintf(g_serial_fp, "d\n");
	usleep(100000);
	fflush(g_serial_fp);
	usleep(100000);
	tcflush(g_serial_fd, TCIFLUSH);
	fprintf(g_serial_fp, "\n");
	fgets(buffer, sizeof(buffer), g_serial_fp);
	return strcmp(buffer, "OK\r\n");
}

int powermon_set_mask(uint16_t mask) {
	char buffer[32];
	unsigned int length;
	unsigned int setval;
	length = sprintf(buffer, "m %u\n", mask);
	fwrite(buffer, 1, length, g_serial_fp);
	fgets(buffer, sizeof(buffer), g_serial_fp);
	sscanf(buffer, "M=%u\r", &setval);
	printf("Mask set to %u.\n", setval);
	fgets(buffer, sizeof(buffer), g_serial_fp);
	return strcmp(buffer, "OK\r\n");
}

int powermon_set_samples(uint16_t interval, uint32_t num_samples) {
	char buffer[32];
	unsigned int length;
	unsigned int set_interval, set_num_samples;
	length = sprintf(buffer, "s %u %u\n", interval, num_samples);
	fwrite(buffer, 1, length, g_serial_fp);
	fgets(buffer, sizeof(buffer), g_serial_fp);
	sscanf(buffer, "S=%u,%u\r", &set_interval, &set_num_samples);
	printf("Sample interval set to %u.  %u sample", set_interval,
			set_num_samples);
	if (set_num_samples != 1)
		printf("s");
	printf(" to be collected.\n");
	fgets(buffer, sizeof(buffer), g_serial_fp);
	return strcmp(buffer, "OK\r\n");
}

int powermon_set_time(uint32_t now) {
	char buffer[32];
	unsigned int length;
	unsigned int setval;
	length = sprintf(buffer, "t %u\n", now);
	fwrite(buffer, 1, length, g_serial_fp);
	fgets(buffer, sizeof(buffer), g_serial_fp);
	sscanf(buffer, "T=%u\r", &setval);
	printf("Time set to %u.\n", setval);
	fgets(buffer, sizeof(buffer), g_serial_fp);
	return strcmp(buffer, "OK\r\n");
}

int powermon_get_samples() {
	unsigned char buffer[4];
	unsigned int time; /* , reading; */
	uint16_t voltage, current;
	uint8_t flags, sensor;
	double v_double, i_double;
	fprintf(g_serial_fp, "e\n");
	for (;;) {
		fread(buffer, 1, 4, g_serial_fp);
		flags = buffer[0];
		/*reading = (((uint32_t)buffer[0] << 24) | ((uint32_t)buffer[1] << 16)
				| ((uint32_t)buffer[2] << 8) | ((uint32_t)buffer[3]));
		printf("bytes: %#x\n", reading);*/
		if (!(flags&(1<<TIME_FLAG)) && (flags&(1<<DONE_FLAG))) {
			break;
		}
		if(flags& (1 << OVF_FLAG)) {
			printf("*");
		}
		if (flags & (1 << TIME_FLAG)) {
			time = (((uint32_t)buffer[0] << 24) | ((uint32_t)buffer[1] << 16)
					| ((uint32_t)buffer[2] << 8) | ((uint32_t)buffer[3]))
					& 0x3FFFFFFF;
			printf("timestamp: %u\n", time);
		} else {
			sensor = (uint8_t)buffer[0] & 0x0F;
			voltage = ((uint16_t)buffer[1] << 4) | ((buffer[3] >> 4) & 0x0F);
			current = ((uint16_t)buffer[2] << 4) | (buffer[3] & 0x0F);
			v_double = (double)voltage / 4096 * V_FULLSCALE;
			i_double = (double)current / 4096 * I_FULLSCALE / R_SENSE;
			printf("sensor = %d, voltage = %fV, current = %fA\n", sensor, v_double, i_double);
		}
	}
	return 0;
}

int configure_tty(const char *dev) {
	g_serial_fd = open(dev, O_RDWR | O_NOCTTY);
	if (g_serial_fd < 0) {
		perror(dev);
		return 1;
	}
	if ((g_serial_fp = fdopen(g_serial_fd, "r+")) == NULL) {
		perror("fdopen()");
		return 2;
	}
	tcgetattr(g_serial_fd, &g_oldtio); /* save current port settings */
	bzero(&g_newtio, sizeof (g_newtio));

	/* 
	 CRTSCTS == hardware flow control
	 CS8 = 8 bits
	 CLOCAL = ignore modem control lines
	 CREAD = enable receiver
	 B38400 = baud rate
	 */
	g_newtio.c_iflag = IGNPAR;
	g_newtio.c_cflag = BAUDRATE | CS8 | CSTOPB | CLOCAL | CREAD;
	g_newtio.c_oflag = 0;

	/* set input mode (non-canonical, no echo,...) */
	g_newtio.c_lflag = 0;

	g_newtio.c_cc[VTIME] = 0; /* inter-character timer unused */
	g_newtio.c_cc[VMIN] = 1; /* blocking read until 1 char received */

	tcflush(g_serial_fd, TCIFLUSH);
	tcsetattr(g_serial_fd, TCSANOW, &g_newtio);

	return 0;
}

int main(int argc, char **argv) {
	char dev[256];
	uint16_t sensor_mask;
	uint16_t sample_period;
	uint32_t num_samples = 0;
	if ((argc < 4) || (argc > 5)) {
		fprintf (stderr,
			 "Usage:\n %s <port_dev> <mask> <sample_pd> [<num_samples>]\n\n",
			 argv[0]);
		fprintf (stderr, "Example:\n%s /dev/ttyUSB1 32 1 200\n", argv[0]);
		fprintf (stderr, "(Read from sensor 5, or 0b10000, at 1 Hz for 200 samples)\n");
		return 1;
	}
	strcpy (dev, argv[1]);
	sensor_mask = atoi(argv[2]);
	sample_period = atoi(argv[3]);
	if(argc == 5) {
		num_samples = atoi(argv[4]);
	}
	if (configure_tty(dev))
		exit(1);
	if (powermon_init())
		exit(1);
	if (powermon_set_mask(sensor_mask))
		exit(1);
	if (powermon_set_samples(sample_period, num_samples))
		exit(1);
	if (powermon_set_time(1))
		exit(1);
	powermon_get_samples();
	atexit(cleanup);
	return 0;

}
