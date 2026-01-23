#include "platform.h"
#include "log.h"

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <GL/gl.h>
#include <GL/glx.h>

#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct KC_X11 {
    Display* dpy;
    int screen;
    Window win;
    GLXContext ctx;
    Atom wm_delete;

    int center_x;
    int center_y;

    Cursor invisible_cursor;

    int last_mx, last_my;
    bool have_last_mouse;
    bool warp_pending;
} KC_X11;

static double time_now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

double kc_platform_time_sec(void) { return time_now_sec(); }

static void warp_to_center(KC_Platform* p, KC_X11* x11) {
    x11->center_x = p->width / 2;
    x11->center_y = p->height / 2;
    XWarpPointer(x11->dpy, None, x11->win, 0, 0, 0, 0, x11->center_x, x11->center_y);
    XFlush(x11->dpy);
    x11->warp_pending = true;
    x11->have_last_mouse = false;
}

static KC_Key keysym_to_key(KeySym ks) {
    switch (ks) {
        case XK_w: case XK_W: return KC_KEY_W;
        case XK_a: case XK_A: return KC_KEY_A;
        case XK_s: case XK_S: return KC_KEY_S;
        case XK_d: case XK_D: return KC_KEY_D;
        case XK_space:        return KC_KEY_SPACE;
        case XK_Shift_L:      return KC_KEY_LSHIFT;
        case XK_Escape:       return KC_KEY_ESCAPE;
        case XK_q: case XK_Q: return KC_KEY_Q;
        case XK_e: case XK_E: return KC_KEY_E;
        case XK_f: case XK_F: return KC_KEY_F;
        default:              return KC_KEY_UNKNOWN;
    }
}

static Cursor create_invisible_cursor(Display* dpy, Window win) {
    Pixmap bm_no;
    XColor black;
    static char no_data[] = { 0,0,0,0,0,0,0,0 };
    black.red = black.green = black.blue = 0;

    bm_no = XCreateBitmapFromData(dpy, win, no_data, 8, 8);
    Cursor c = XCreatePixmapCursor(dpy, bm_no, bm_no, &black, &black, 0, 0);
    XFreePixmap(dpy, bm_no);
    return c;
}

bool kc_platform_init(KC_Platform* p, int w, int h, const char* title) {
    memset(p, 0, sizeof(*p));
    p->width = w;
    p->height = h;

    KC_X11* x11 = (KC_X11*)calloc(1, sizeof(KC_X11));
    if (!x11) return false;

    x11->dpy = XOpenDisplay(NULL);
    if (!x11->dpy) {
        KC_ERR("XOpenDisplay failed");
        free(x11);
        return false;
    }

    x11->screen = DefaultScreen(x11->dpy);

    static int visual_attribs[] = {
        GLX_RGBA,
        GLX_DOUBLEBUFFER,
        GLX_DEPTH_SIZE, 24,
        GLX_STENCIL_SIZE, 8,
        GLX_RED_SIZE, 8,
        GLX_GREEN_SIZE, 8,
        GLX_BLUE_SIZE, 8,
        None
    };

    XVisualInfo* vi = glXChooseVisual(x11->dpy, x11->screen, visual_attribs);
    if (!vi) {
        KC_ERR("glXChooseVisual failed (no matching visual)");
        XCloseDisplay(x11->dpy);
        free(x11);
        return false;
    }

    Colormap cmap = XCreateColormap(x11->dpy, RootWindow(x11->dpy, x11->screen), vi->visual, AllocNone);

    XSetWindowAttributes swa;
    swa.colormap = cmap;
    swa.event_mask =
        ExposureMask | KeyPressMask | KeyReleaseMask |
        ButtonPressMask | ButtonReleaseMask |
        PointerMotionMask | StructureNotifyMask | FocusChangeMask;

    x11->win = XCreateWindow(
        x11->dpy,
        RootWindow(x11->dpy, x11->screen),
        0, 0, (unsigned)w, (unsigned)h, 0,
        vi->depth,
        InputOutput,
        vi->visual,
        CWColormap | CWEventMask,
        &swa
    );

    if (!x11->win) {
        KC_ERR("XCreateWindow failed");
        XFree(vi);
        XCloseDisplay(x11->dpy);
        free(x11);
        return false;
    }

    XStoreName(x11->dpy, x11->win, title);

    x11->wm_delete = XInternAtom(x11->dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(x11->dpy, x11->win, &x11->wm_delete, 1);

    XMapWindow(x11->dpy, x11->win);

    x11->ctx = glXCreateContext(x11->dpy, vi, NULL, True);
    XFree(vi);

    if (!x11->ctx) {
        KC_ERR("glXCreateContext failed");
        XDestroyWindow(x11->dpy, x11->win);
        XCloseDisplay(x11->dpy);
        free(x11);
        return false;
    }

    if (!glXMakeCurrent(x11->dpy, x11->win, x11->ctx)) {
        KC_ERR("glXMakeCurrent failed");
        glXDestroyContext(x11->dpy, x11->ctx);
        XDestroyWindow(x11->dpy, x11->win);
        XCloseDisplay(x11->dpy);
        free(x11);
        return false;
    }

    x11->invisible_cursor = create_invisible_cursor(x11->dpy, x11->win);

    p->native = x11;

    KC_INFO("Platform init OK (X11/GLX)");

    return true;
}

void kc_platform_shutdown(KC_Platform* p) {
    if (!p || !p->native) return;
    KC_X11* x11 = (KC_X11*)p->native;

    glXMakeCurrent(x11->dpy, None, NULL);
    glXDestroyContext(x11->dpy, x11->ctx);

    XFreeCursor(x11->dpy, x11->invisible_cursor);
    XDestroyWindow(x11->dpy, x11->win);
    XCloseDisplay(x11->dpy);

    free(x11);
    p->native = NULL;
}

static void input_frame_reset(KC_Input* in) {
    memset(in->keys_pressed, 0, sizeof(in->keys_pressed));
    memset(in->keys_released, 0, sizeof(in->keys_released));
    in->mouse_dx = 0;
    in->mouse_dy = 0;
}

void kc_platform_set_mouse_capture(KC_Platform* p, bool enabled) {
    KC_X11* x11 = (KC_X11*)p->native;
    if (!x11) return;

    p->mouse_captured = enabled;

    if (enabled) {
        int rc = XGrabPointer(
            x11->dpy, x11->win, True,
            PointerMotionMask | ButtonPressMask | ButtonReleaseMask,
            GrabModeAsync, GrabModeAsync,
            x11->win, x11->invisible_cursor,
            CurrentTime
        );
        if (rc != GrabSuccess) {
            KC_WARN("XGrabPointer failed (%d) - mouse may feel bad on WSL", rc);
        }

        XDefineCursor(x11->dpy, x11->win, x11->invisible_cursor);
        warp_to_center(p, x11);
    } else {
        XUngrabPointer(x11->dpy, CurrentTime);
        XUndefineCursor(x11->dpy, x11->win);
        XFlush(x11->dpy);

        x11->have_last_mouse = false;
        x11->warp_pending = false;
    }
}

void kc_platform_poll(KC_Platform* p) {
    KC_X11* x11 = (KC_X11*)p->native;
    KC_ASSERT(x11);

    input_frame_reset(&p->input);

    while (XPending(x11->dpy)) {
        XEvent e;
        XNextEvent(x11->dpy, &e);

        switch (e.type) {
            case ClientMessage:
                if ((Atom)e.xclient.data.l[0] == x11->wm_delete) {
                    p->should_close = true;
                }
                break;

            case ConfigureNotify:
                p->width = e.xconfigure.width;
                p->height = e.xconfigure.height;
                glViewport(0, 0, p->width, p->height);
                break;

            case KeyPress: {
                KeySym ks = XLookupKeysym(&e.xkey, 0);
                KC_Key k = keysym_to_key(ks);
                if (k != KC_KEY_UNKNOWN) {
                    if (!p->input.keys[k]) p->input.keys_pressed[k] = true;
                    p->input.keys[k] = true;
                }
            } break;

            case KeyRelease: {
                /* Filter auto-repeat: if next event is KeyPress with same key/time, ignore */
                if (XEventsQueued(x11->dpy, QueuedAfterReading)) {
                    XEvent ne;
                    XPeekEvent(x11->dpy, &ne);
                    if (ne.type == KeyPress && ne.xkey.time == e.xkey.time && ne.xkey.keycode == e.xkey.keycode) {
                        /* consume the auto-repeat press */
                        XNextEvent(x11->dpy, &ne);
                        break;
                    }
                }

                KeySym ks = XLookupKeysym(&e.xkey, 0);
                KC_Key k = keysym_to_key(ks);
                if (k != KC_KEY_UNKNOWN) {
                    p->input.keys[k] = false;
                    p->input.keys_released[k] = true;
                }
            } break;

            case MotionNotify:
                if (p->mouse_captured) {
                    const int mx = e.xmotion.x;
                    const int my = e.xmotion.y;

                    if (x11->warp_pending) {
                        x11->warp_pending = false;
                        x11->last_mx = mx;
                        x11->last_my = my;
                        x11->have_last_mouse = true;
                        break;
                    }

                    if (!x11->have_last_mouse) {
                        x11->last_mx = mx;
                        x11->last_my = my;
                        x11->have_last_mouse = true;
                        break;
                    }

                    p->input.mouse_dx += (mx - x11->last_mx);
                    p->input.mouse_dy += (my - x11->last_my);

                    x11->last_mx = mx;
                    x11->last_my = my;

                    const int margin = 8;
                    if (mx < margin || mx > (p->width - margin) ||
                        my < margin || my > (p->height - margin)) {
                        warp_to_center(p, x11);
                    }
                }
                break;

            case FocusOut:
                /* drop input when we lose focus */
                memset(p->input.keys, 0, sizeof(p->input.keys));
                break;

            default:
                break;
        }
    }
}

void kc_platform_swap(KC_Platform* p) {
    KC_X11* x11 = (KC_X11*)p->native;
    glXSwapBuffers(x11->dpy, x11->win);
}
