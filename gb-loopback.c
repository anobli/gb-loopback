/* Copyright 2015 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <libudev.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>

struct loopback
{
	int cport;
	int type;
	int size;
	int ms;

	int monitor;
};

#define	GB_LOOPBACK_TYPE_PING				0x02
#define	GB_LOOPBACK_TYPE_TRANSFER			0x03
#define	GB_LOOPBACK_TYPE_SINK				0x04

volatile int run = 1;

void sig_handler(int signo)
{
	if (signo == SIGINT)
		run = 2;
}

int get_cportid(const char *path)
{
	int cport;
	char *c = strrchr(path, ':');
	if (c == NULL)
		return -1;
	if (sscanf(c + 1, "%d", &cport) != 1)
		return -1;
	return cport;
}

void loopback_monitor(struct loopback *loopback, struct udev_device *dev,
		      int cportid)
{
	int type = loopback->type;

	printf("cport %d\n", cportid);
	printf("\trequests per seconds = %s\n",
		udev_device_get_sysattr_value(dev, "requests_per_second_avg"));
	printf("\tlatency = %s\n",
		udev_device_get_sysattr_value(dev, "latency_avg"));
	printf("\tbw = %s\n",
		udev_device_get_sysattr_value(dev, "throughput_avg"));
	printf("\terrors = %s\n",
		udev_device_get_sysattr_value(dev, "error"));
	if (run == 2 && type)
		udev_device_set_sysattr_value(dev, "type", "0");

}

void loopback_configure(struct loopback *loopback, struct udev_device *dev,
			int cportid)
{
	char value[32];
	int type = loopback->type;
	int size = loopback->size;
	int monitor = loopback->monitor;
	int ms = loopback->ms;


	switch(type) {
	case GB_LOOPBACK_TYPE_PING:
		break;
	case GB_LOOPBACK_TYPE_TRANSFER:
	case GB_LOOPBACK_TYPE_SINK:
		sprintf(value, "%d", size);
		udev_device_set_sysattr_value(dev, "size", value);
	default:
		type = 0;
		if (monitor)
			return;
	}

	if (type == 0)
		printf("Disable loopback on cport %d\n", cportid);
	else
		printf("Enable loopback on cport %d\n", cportid);

	if (ms >= 0) {
		sprintf(value, "%d", ms);
		udev_device_set_sysattr_value(dev, "ms_wait", value);
	}

	sprintf(value, "%d", type);
	udev_device_set_sysattr_value(dev, "type", value);
}

void loopback_foreach(struct udev *udev,
		      struct udev_list_entry *devices,
		      void (*cb)(struct loopback *, struct udev_device *, int),
		      struct loopback *loopback)
{
	struct udev_device *dev;
	struct udev_list_entry *dev_list_entry;

	const char *path;
	const char *devtype;
	const char *protocol_id;

	int cport = loopback->cport;

	udev_list_entry_foreach(dev_list_entry, devices) {
		path = udev_list_entry_get_name(dev_list_entry);
		dev = udev_device_new_from_syspath(udev, path);
		devtype = udev_device_get_devtype(dev);

		if (strcmp(devtype, "greybus_connection"))
			continue;

		protocol_id = udev_device_get_sysattr_value(dev,
							    "protocol_id");
		if (strcmp(protocol_id, "17"))
			continue;

		if (cport != -1 && cport != get_cportid(path))
			continue;

		cb(loopback, dev, get_cportid(path));
	}
}

int main (int argc, char *argv[])
{
	struct loopback loopback;

	struct udev *udev;
	struct udev_enumerate *enumerate;
	struct udev_list_entry *devices, *dev_list_entry;
	struct udev_device *dev;

	int c;

	memset(&loopback, 0, sizeof(loopback));
	loopback.cport = -1;

	while ((c = getopt (argc, argv, "t:s:c:mw:")) != -1) {
		switch (c) {
		case 'c':
			if (sscanf(optarg, "%d", &loopback.cport) != 1)
				return -EINVAL;			
			break;		
		case 's':
			if (sscanf(optarg, "%d", &loopback.size) != 1)
				return -EINVAL;			
			break;
		case 't':
			if (sscanf(optarg, "%d", &loopback.type) != 1)
				return -EINVAL;
			break;
		case 'm':
			loopback.monitor = 1;
			break;
		case 'w':
			if (sscanf(optarg, "%d", &loopback.ms) != 1)
				return -EINVAL;
			break;
		default:
			return -EINVAL;
		}
	}

	if (signal(SIGINT, sig_handler) == SIG_ERR)
		return -1;

	udev = udev_new();
	if (!udev) {
		printf("Can't create udev\n");
		exit(1);
	}
	
	enumerate = udev_enumerate_new(udev);
	udev_enumerate_add_match_subsystem(enumerate, "greybus");
	udev_enumerate_scan_devices(enumerate);
	devices = udev_enumerate_get_list_entry(enumerate);

	if (!devices) {
		printf("No greybus devices!\n");
		return -1;
	}

	loopback_foreach(udev, devices, loopback_configure, &loopback);

	if (loopback.monitor) {
		struct timeval ts, te, td;
		struct timeval tr = {0, 400000 };
		while (run) {
			gettimeofday(&ts, NULL);
			printf("\033[2J");
			printf("\033[H");
			loopback_foreach(udev, devices, loopback_monitor,
					 &loopback);
			if (run == 2)
				run = 0;
			gettimeofday(&te, NULL);
			timersub(&te, &ts, &td);
			if (timercmp(&td, &tr, <)) {
				timersub(&tr, &td, &ts);
				usleep(ts.tv_usec);
			}
		}
	}

	udev_enumerate_unref(enumerate);
	udev_unref(udev);

	return 0;
}
