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

int main (int argc, char *argv[])
{
	int type = 0;
	int size = 0;
	int cport = -1;
	int monitor = 0;
	int ms = -1;
	struct udev *udev;
	struct udev_enumerate *enumerate;
	struct udev_list_entry *devices, *dev_list_entry;
	struct udev_device *dev;
	char value[32];
	const char *path;
	const char *devtype;
	const char *protocol_id;

	int c;
	while ((c = getopt (argc, argv, "t:s:c:mw:")) != -1) {
		switch (c) {
		case 'c':
			if (sscanf(optarg, "%d", &cport) != 1)
				return -EINVAL;			
			break;		
		case 's':
			if (sscanf(optarg, "%d", &size) != 1)
				return -EINVAL;			
			break;
		case 't':
			if (sscanf(optarg, "%d", &type) != 1)
				return -EINVAL;
			break;
		case 'm':
			monitor = 1;
			break;
		case 'w':
			if (sscanf(optarg, "%d", &ms) != 1)
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
	udev_list_entry_foreach(dev_list_entry, devices) {		
		path = udev_list_entry_get_name(dev_list_entry);
		dev = udev_device_new_from_syspath(udev, path);
		devtype = udev_device_get_devtype(dev);

		if (strcmp(devtype, "greybus_connection"))
			continue;

		protocol_id = udev_device_get_sysattr_value(dev, "protocol_id");
		if (strcmp(protocol_id, "17"))
			continue;

		if (cport != -1 && cport != get_cportid(path))
			continue;

		if (!type && monitor)
			continue;

		if (type == 2) {
			sprintf(value, "%d", size);
			udev_device_set_sysattr_value(dev, "size", value);
		}

		if (type == 0)
			printf("Disable loopback on cport %d\n", 
				get_cportid(path));
		else if (type > 2)
			return -EINVAL;
		else
			printf("Enable loopback on cport %d\n",
				get_cportid(path));
		sprintf(value, "%d", type);
		udev_device_set_sysattr_value(dev, "type", value);
		if (ms >= 0) {
			sprintf(value, "%d", ms);
			udev_device_set_sysattr_value(dev, "ms_wait", value);
		}
	}
	if (monitor) {
		struct timeval ts, te, td;
		struct timeval tr = {0, 400000 };
		while (run) {
			gettimeofday(&ts, NULL);
			printf("\033[2J");
			printf("\033[H");
			udev_list_entry_foreach(dev_list_entry, devices) {
				path = udev_list_entry_get_name(dev_list_entry);
				dev = udev_device_new_from_syspath(udev, path);
				devtype = udev_device_get_devtype(dev);

				if (strcmp(devtype, "greybus_connection"))
					continue;
				protocol_id = udev_device_get_sysattr_value(dev, "protocol_id");
				if (strcmp(protocol_id, "17"))
					continue;
				if (cport != -1 && cport != get_cportid(path))
					continue;
				printf("cport %d\n", get_cportid(path));
				printf("\tfreq = %s\n", 					udev_device_get_sysattr_value(dev, "frequency_avg"));
				printf("\tlatency = %s\n", 
					udev_device_get_sysattr_value(dev, "latency_avg"));
				printf("\tbw = %s\n", 
					udev_device_get_sysattr_value(dev, "throughput_avg"));
				if (run == 2 && type)
					udev_device_set_sysattr_value(dev, "type", "0");
			}
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
