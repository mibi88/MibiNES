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
#include <time.h>
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
static XVisualInfo info;

static char *back_buffer;
static XImage *back_buffer_image;

static MNEmu emu;

static int x, y;
static int w, h;

static int needs_resize;
static int nw, nh;

static unsigned long int last_time;

static unsigned long mn_gui_get_time(void) {
    struct timespec time;
    clock_gettime(CLOCK_REALTIME, &time);
    return time.tv_nsec/(1e6)+time.tv_sec*1000;
}

int mn_gui_init(unsigned char *rom, size_t size) {
    XSetWindowAttributes attr;

    w = W;
    h = H;
    nw = w;
    nh = h;
    needs_resize = 0;

    last_time = mn_gui_get_time();

    if(mn_emu_init(&emu, mn_gui_pixel, rom, size, 0)){
        return 1;
    }

    back_buffer = malloc(W*H*4);
    if(back_buffer == NULL){
        mn_emu_free(&emu);

        return 2;
    }

    memset(&info, 0, sizeof(XVisualInfo));

    display = XOpenDisplay(NULL);
    if(display == NULL){
        mn_emu_free(&emu);
        free(back_buffer);

        return 3;
    }

    root = DefaultRootWindow(display);

    if(!XMatchVisualInfo(display, DefaultScreen(display), 24, TrueColor,
                         &info)){
        mn_emu_free(&emu);
        free(back_buffer);
        XCloseDisplay(display);

        return 4;
    }

    attr.background_pixel = 0;
    attr.colormap = XCreateColormap(display, root, info.visual, AllocNone);

    window = XCreateWindow(display, root, 0, 0, W, H, 0, info.depth,
                           InputOutput, info.visual, CWBackPixel | CWColormap,
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

    back_buffer_image = XCreateImage(display, info.visual, info.depth, ZPixmap,
                                     0, back_buffer, W, H, 4*8, 0);

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

static void mn_gui_update(void) {
    unsigned long int new_time;
    unsigned long int ms;

    if(back_buffer != NULL){
        XPutImage(display, window, gc, back_buffer_image, 0, 0, 0, 0, w, h);
    }

    if(needs_resize){
        /* NOTE: XDestroyImage also frees back_buffer. */
        if(back_buffer != NULL) XDestroyImage(back_buffer_image);
        back_buffer = malloc(nw*nh*4);
        if(back_buffer != NULL){
            back_buffer_image = XCreateImage(display, info.visual, info.depth,
                                             ZPixmap, 0, back_buffer, nw, nh,
                                             4*8, 0);
        }

        w = nw;
        h = nh;

        needs_resize = 0;
    }

    /* Cap it at 16 ms */
    do{
        new_time = mn_gui_get_time();
        ms = new_time-last_time;
        if(new_time < last_time) ms = 1;
    }while(ms < 16);

    last_time = new_time;

    printf("\033[2Kms: %lu\r", ms);
    fflush(stdout);

    XFlush(display);
}

static void mn_gui_rect(int x, int y, int rw, int rh, long int color) {
    int px, py;
    char *p;
    size_t d;

    char r = *((unsigned char*)&color);
    char g = ((unsigned char*)&color)[1];
    char b = ((unsigned char*)&color)[2];

    if(back_buffer == NULL){
        /* Fallback on error */
        XSetForeground(display, gc, color);
        XFillRectangle(display, window, gc, x, y, rw, rh);
    }else{
        if(x < 0){
            rw += x;
            x = 0;
        }
        if(y < 0){
            rh += y;
            y = 0;
        }
        if(x+rw > w){
            rw = w-x-1;
        }
        if(rw < 0 || x >= w){
            return;
        }
        if(y+rh > h){
            rh = h-y-1;
        }
        if(rh < 0 || y >= h){
            return;
        }

        p = back_buffer+(y*w+x)*4;
        d = (w-rw)*4;
        for(py=y;py<y+rh;py++){
            for(px=x;px<x+rw;px++){
                *(p++) = r;
                *(p++) = g;
                *(p++) = b;
                *(p++) = 0;
            }
            p += d;
        }
    }
}

void mn_gui_pixel(long int color) {
    /* HACK: I had to add 1 to the width and height to avoid having a black
     * grid */
    mn_gui_rect(x*w/W, y*h/H, (w/W > 0 ? w/W : 1)+1, (h/H > 0 ? h/H : 1)+1,
                color);

    x++;
    if(x >= W){
        x = 0;
        y++;
        if(y >= H){
            y = 0;

            mn_gui_update();
        }
    }
}

void mn_gui_run(void) {
    size_t i;

    int message = 0;

    XWindowAttributes win_attr;

    while(1){
        if(mn_gui_get_next_event()){
            if((Atom)event.xclient.data.l[0] == wm_delete){
                break;
            }
            if(event.type == Expose){
                XGetWindowAttributes(display, window, &win_attr);
                nw = win_attr.width;
                nh = win_attr.height;
                needs_resize = 1;
            }
        }else{
            for(i=0;i<W*H;i++){
                mn_emu_pixel(&emu);
                if(emu.cpu.jammed && !message){
                    fputs("CPU jammed!\n", stderr);
                    message = 1;
                }
            }
        }
    }
}

void mn_gui_free(void) {
    XDestroyWindow(display, window);
    XCloseDisplay(display);
}
