/*****************************************************************************
 *
 * mtview - Multitouch Viewer (GPLv3 license)
 *
 * Copyright (C) 2010-2011 Canonical Ltd.
 * Copyright (C) 2010      Henrik Rydberg <rydberg@euromail.se>
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 ****************************************************************************/

#include "config.h"
#include <X11/Xlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <utouch/frame-mtdev.h>
#if HAVE_XI
#include <utouch/frame-xi2.h>
#endif
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <active_window.h>

#define DEF_FRAC 0.15
#define DEF_WIDTH 0.05

#define DIM_TOUCH 32

static int opcode;

struct windata {
	Display *dsp;
	Window win;
	GC gc;
	int screen;
	float off_x, off_y;
	unsigned long white, black;
	unsigned long color[DIM_TOUCH];
	int id[DIM_TOUCH];
};

static inline float max(float a, float b)
{
	return b > a ? b : a;
}

static void output_touch(utouch_frame_handle fh, struct windata *w,
			 const struct utouch_contact *t)
{
	const struct utouch_surface *s = utouch_frame_get_surface(fh);
	float dx = s->mapped_max_x - s->mapped_min_x;
	float dy = s->mapped_max_y - s->mapped_min_y;
	float x = t->x - w->off_x, y = t->y - w->off_y;
	float major = 0, minor = 0, angle = 0;

	if(t->id > 32){
	}

/*	if (s->use_pressure) {
		major = DEF_FRAC * t->pressure * dy;
		minor = DEF_FRAC * t->pressure * dx;
		angle = 0;
	}
	if (s->use_touch_major) {
		major = t->touch_major;
		minor = t->touch_minor;
		angle = t->orientation;
	}
	if (major == 0 && minor == 0) {
		major = DEF_WIDTH * dy;
		minor = DEF_WIDTH * dx;
	}  */

/*	float ac = fabs(cos(angle));
	float as = fabs(sin(angle));
	float mx = max(minor * ac, major * as);
	float my = max(major * ac, minor * as); */

/*	XSetForeground(w->dsp, w->gc, w->color[t->slot]);

	XFillArc(w->dsp, w->win, w->gc, x - mx / 2, y - my / 2,
		 mx, my, 0, 360 * 64);
	XFlush(w->dsp); */
	/* update touchhandler */
	update(x, y, t->id);
}

static void report_frame(utouch_frame_handle fh,
			 const struct utouch_frame *frame,
			 struct windata *w)
{
	int i;

	for (i = 0; i < frame->num_active; i++)
		output_touch(fh, w, frame->active[i]);
}

static int init_window(struct windata *w)
{
	int event, err;
	int i;

	memset(w, 0, sizeof(w));
	for (i = 0; i < DIM_TOUCH; i++)
		w->id[i] = -1;

	w->dsp = XOpenDisplay(NULL);
	if (!w->dsp)
		return -1;
	if (!XQueryExtension(w->dsp, "XInputExtension", &opcode, &event, &err))
		return -1;

	w->screen = DefaultScreen(w->dsp);
	w->white = WhitePixel(w->dsp, w->screen);
	w->black = BlackPixel(w->dsp, w->screen);

	/* w->win = XCreateSimpleWindow(w->dsp, XDefaultRootWindow(w->dsp), 0, 0, 200, 200, 0, w->black, w->white); */
	w->win = get_focus_window(w->dsp);
	w->gc = DefaultGC(w->dsp, w->screen);

	XMapWindow(w->dsp, w->win);
	XFlush(w->dsp);

	return 0;
}

static void term_window(struct windata *w)
{
	XDestroyWindow(w->dsp, w->win);
	XCloseDisplay(w->dsp);
}

static void set_screen_size_mtdev(utouch_frame_handle fh,
				  struct windata *w,
				  XEvent *xev)
{
	struct utouch_surface *s = utouch_frame_get_surface(fh);
	XConfigureEvent *cev = (XConfigureEvent *)xev;

	s->mapped_min_x = 0;
	s->mapped_min_y = 0;
	s->mapped_max_x = DisplayWidth(w->dsp, w->screen);
	s->mapped_max_y = DisplayHeight(w->dsp, w->screen);
	s->mapped_max_pressure = 1;

	if (cev) {
		w->off_x = cev->x;
		w->off_y = cev->y;
	}

	fprintf(stderr, "map: %f %f %f %f %f %f\n",
		w->off_x, w->off_y,
		s->mapped_min_x, s->mapped_min_y,
		s->mapped_max_x, s->mapped_max_y);
}

static void run_window_mtdev(utouch_frame_handle fh, struct mtdev *dev, int fd)
{
	const struct utouch_frame *frame;
	struct input_event iev;
	struct windata w;
	XEvent xev;

	if (init_window(&w))
		return;

	set_screen_size_mtdev(fh, &w, 0);
	XSelectInput(w.dsp, w.win, StructureNotifyMask);

	while (1) {
		while (!mtdev_idle(dev, fd, 100)) {
			while (mtdev_get(dev, fd, &iev, 1) > 0) {
				frame = utouch_frame_pump_mtdev(fh, &iev);
				if (frame)
					report_frame(fh, frame, &w);
			}
		}
		while (XPending(w.dsp)) {
			XNextEvent(w.dsp, &xev);
			set_screen_size_mtdev(fh, &w, &xev);
		}
	}

	term_window(&w);
}

static int run_mtdev(const char *name)
{
	struct evemu_device *evemu;
	struct mtdev *mtdev;
	utouch_frame_handle fh;
	int fd;

	fd = open(name, O_RDONLY | O_NONBLOCK);
	if (fd < 0) {
		fprintf(stderr, "error: could not open device\n");
		return -1;
	}
	if (ioctl(fd, EVIOCGRAB, 1)) {
		fprintf(stderr, "error: could not grab the device\n");
		return -1;
	}

	evemu = evemu_new(0);
	if (!evemu || evemu_extract(evemu, fd)) {
		fprintf(stderr, "error: could not describe device\n");
		return -1;
	}
	if (!utouch_frame_is_supported_mtdev(evemu)) {
		fprintf(stderr, "error: unsupported device\n");
		return -1;
	}
	mtdev = mtdev_new_open(fd);
	if (!mtdev) {
		fprintf(stderr, "error: could not open mtdev\n");
		return -1;
	}
	fh = utouch_frame_new_engine(100, 32, 100);
	if (!fh || utouch_frame_init_mtdev(fh, evemu)) {
		fprintf(stderr, "error: could not init frame\n");
		return -1;
	}

	run_window_mtdev(fh, mtdev, fd);

	utouch_frame_delete_engine(fh);
	mtdev_close_delete(mtdev);
	evemu_delete(evemu);

	ioctl(fd, EVIOCGRAB, 0);
	close(fd);

	return 0;
}

#if HAVE_XI
static void handle_event_xi2(struct windata *w,
			     utouch_frame_handle fh,
			     XEvent *ev)
{
	XConfigureEvent *cev = (XConfigureEvent *)ev;
	XGenericEventCookie *gev = &ev->xcookie;
	const struct utouch_frame *frame;

	switch(ev->type) {
	case ConfigureNotify:
		if (cev->window == XDefaultRootWindow(cev->display)) {
			utouch_frame_configure_xi2(fh, cev);
		} else {
			w->off_x = cev->x;
			w->off_y = cev->y;
		}
		break;
	case GenericEvent:
		if (!XGetEventData(w->dsp, gev))
			break;
		if (gev->type == GenericEvent && gev->extension == opcode) {
			frame = utouch_frame_pump_xi2(fh, gev->data);
			if (frame)
				report_frame(fh, frame, w);
		}
		XFreeEventData(w->dsp, gev);
		break;
	}
}

static void run_window_xi2(struct windata *w,
			   utouch_frame_handle fh,
			   XIDeviceInfo *dev)
{
	const struct utouch_frame *frame;
	XIEventMask mask;

	fprintf(stderr, "xi2 running\n");

	XSelectInput(w->dsp, w->win, StructureNotifyMask);
	XSelectInput(w->dsp, XDefaultRootWindow(w->dsp), StructureNotifyMask);

	mask.deviceid = dev->deviceid;
	mask.mask_len = XIMaskLen(XI_LASTEVENT);
	mask.mask = calloc(mask.mask_len, sizeof(char));

	XISetMask(mask.mask, XI_PropertyEvent);
	XISetMask(mask.mask, XI_TouchBegin);
	XISetMask(mask.mask, XI_TouchUpdate);
	XISetMask(mask.mask, XI_TouchEnd);
	XISelectEvents(w->dsp, w->win, &mask, 1);

	while (1) {
		XEvent ev;
		XNextEvent(w->dsp, &ev);
		handle_event_xi2(w, fh, &ev);
	}
}

static int run_xi2(int id)
{
	struct windata w;
	XIDeviceInfo *info, *dev;
	utouch_frame_handle fh;
	int ndevice;
	int i;
	fprintf(stdout, "Using xi2\n");

	if (init_window(&w)) {
		fprintf(stderr, "error: could not init window\n");
		return -1;
	}

	info = XIQueryDevice(w.dsp, XIAllDevices, &ndevice);
	dev = 0;
	for (i = 0; i < ndevice; i++)
		if (info[i].deviceid == id)
			dev = &info[i];
	if (!dev)
		return -1;

	if (!utouch_frame_is_supported_xi2(w.dsp, dev)) {
		fprintf(stderr, "error: unsupported device\n");
		return -1;
	}

	fh = utouch_frame_new_engine(100, 32, 100);
	if (!fh || utouch_frame_init_xi2(fh, w.dsp, dev)) {
		fprintf(stderr, "error: could not init frame\n");
		return -1;
	}

	run_window_xi2(&w, fh, dev);

	utouch_frame_delete_engine(fh);
	XIFreeDeviceInfo(info);
	term_window(&w);

	return 0;
}
#else
static int run_xi2(int id)
{
	fprintf(stderr, "XI2.1 not supported\n");
	return 0;
}
#endif

int run(int devNum)
{
	int id, ret;

	char argv[18] = "/dev/input/event";
        append(argv, '0' + devNum);

	printf("ARGV: %s", argv);

	id = atoi(argv);
	ret = run_mtdev(argv);

	return ret;
}

void append(char* s, char c){
        int len = strlen(s);
        s[len] = c;
        s[len + 1] = '\0';
}
