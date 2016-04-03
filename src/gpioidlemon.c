/*
 * Copyright (c) 2016 Boudewijn Dijkstra <boudewijn@ndva.nl>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <sys/gpio.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/sched.h>
#include <sys/sysctl.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEBUG 0

static int 
get_busy_ticks(void) 
{
	static const int cp_time_mib[] = {CTL_KERN, KERN_CPTIME};
	static int64_t prev_busy;
	int64_t cur_busy, diff_busy;
	long cp_time_cur[CPUSTATES];
	size_t size;
	int i;
	int busy_ticks;

	size = sizeof(cp_time_cur);
	if (sysctl(cp_time_mib, 2, cp_time_cur, &size, NULL, 0) < 0)
		warn("sysctl kern.cp_time failed");
	if (size != sizeof(cp_time_cur))
		warnx(1, "sysctl kern.cp_time failed");
	cur_busy = 0;
	for (i = 0; i < CP_IDLE; i++)
		cur_busy += cp_time_cur[i];

	diff_busy = cur_busy - prev_busy;
	prev_busy = cur_busy;
	if (diff_busy > 0 && diff_busy < LONG_MAX)
		busy_ticks = diff_busy;
	else
		busy_ticks = 0;
	return busy_ticks;
}

static void 
feedback(const char *dev, const struct gpio_pin_op *op) 
{
	int fd, r;

	fd = open(dev, O_RDWR);
	if (fd == -1)
		err(1, "could not open device '%s'", dev);
	r = ioctl(fd, GPIOPINTOGGLE, op);
        if (r == -1)
                err(1, "could not toggle '%s'", op->gp_name);
	close(fd);
}

int 
main(int argc, char** argv) 
{
	const int 		 clockrate[] = {CTL_KERN, KERN_CLOCKRATE};
	struct clockinfo 	 ci;
	size_t 			 size;
	const char		*dev;
	const char		*pin;
	struct gpio_pin_op 	 feedback_op;

	if (argc != 3)
		errx(1, "need 2 arguments (device, pin)");

	dev = argv[1];
	pin = argv[2];

	/* try sysctl before detaching */
	size = sizeof(struct clockinfo);
	if (sysctl(clockrate, 2, &ci, &size, NULL, 0) < 0)
		err(1, "sysctl kern.clockrate failed");

	memset(&feedback_op, 0, sizeof(struct gpio_pin_op));
	strlcpy(feedback_op.gp_name, pin, GPIOPINMAXNAME);

	/* try gpio ioctl before detaching */
	feedback(dev, &feedback_op);
	feedback(dev, &feedback_op);
#if !DEBUG
	daemon(0, 0);
#endif

	useconds_t sleep_time = (useconds_t)(1000000 / (ci.stathz * 2));
	unsigned int busy_ticks = 0;
	for (;;) {
#if DEBUG
		printf("%u ", busy_ticks);
		fflush(stdout);
#endif
		busy_ticks += get_busy_ticks();
		if (busy_ticks == 0) {
			usleep(2 * sleep_time);
		} else {
			feedback(dev, &feedback_op);
			usleep(sleep_time);
			feedback(dev, &feedback_op);
			usleep(sleep_time);
			if (busy_ticks < ci.stathz)
				busy_ticks--;
			else
				busy_ticks /= 2;
		}
	}

	return 0;
}

