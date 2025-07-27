/* mibines - A small NES emulator.
 * by Mibi88
 *
 * This software is licensed under the BSD-3-Clause license:
 *
 * Copyright 2025 Mibi88
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from this
 * software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <gui.h>

#include <emu.h>

#define _XOPEN_SOURCE 600
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

#define W 256
#define H 240

static Display *display;
static Window root;
static Window window;
static XEvent event;
static Atom wm_delete;
static GC gc;

static MNEmu emu;

static int x, y;
static int w, h;

int mn_gui_init(char *file) {
    XVisualInfo info;
    XSetWindowAttributes attr;

    if(mn_emu_init(&emu, mn_gui_pixel)){
        return 1;
    }

    memset(&info, 0, sizeof(XVisualInfo));

    display = XOpenDisplay(NULL);
    if(display == NULL){
        return 2;
    }

    root = DefaultRootWindow(display);

    if(!XMatchVisualInfo(display, DefaultScreen(display), 24, TrueColor,
                         &info)){
        return 3;
    }

    attr.background_pixel = 0;
    attr.colormap = XCreateColormap(display, root, info.visual, AllocNone);

    window = XCreateWindow(display, root, 0, 0, W, H, 0, info.depth,
                           InputOutput, info.visual, CWColormap,
                           &attr);

    /* TODO: Error handling */

    XSelectInput(display, window, ExposureMask | ButtonPressMask |
                 ButtonReleaseMask | PointerMotionMask | KeyPressMask |
                 KeyReleaseMask | KeymapStateMask);

    /* Make the window appear */
    XMapWindow(display, window);

    /* Set the text in the title bar */
    XStoreName(display, window, "MibiNES");

    wm_delete = XInternAtom(display, "WM_DELETE_WINDOW", True);
    XSetWMProtocols(display, window, &wm_delete, 1);

    gc = DefaultGC(display, DefaultScreen(display));

    x = 0;
    y = 0;

    return 0;
}

static int mn_gui_get_next_event(void) {
    if(XPending(display)){
        XNextEvent(display, &event);
        if(event.type == MappingNotify){
            XRefreshKeyboardMapping(&(event.xmapping));
        }
        return 1;
    }
    return 0;
}

void mn_gui_pixel(long int color) {
    XSetForeground(display, gc, color);
    /* HACK: I had to add 1 to the width and height to avoid having a black
     * grid */
    XFillRectangle(display, window, gc, x*w/W, y*h/H, (w/W > 0 ? w/W : 1)+1,
                   (h/H > 0 ? h/H : 1)+1);
    x++;
    if(x >= W){
        x = 0;
        y++;
        if(y >= H){
            y = 0;

            XFlush(display);
        }
    }
}

void mn_gui_run(void) {
    XWindowAttributes win_attr;

    while(1){
        if(mn_gui_get_next_event()){
            if((Atom)event.xclient.data.l[0] == wm_delete){
                break;
            }
            if(event.type == Expose){
                XGetWindowAttributes(display, window, &win_attr);
                w = win_attr.width;
                h = win_attr.height;
            }
        }

        mn_gui_pixel(rand()&0xFFFFFF);
    }
}

void mn_gui_free(void) {
    XDestroyWindow(display, window);
    XCloseDisplay(display);
}
