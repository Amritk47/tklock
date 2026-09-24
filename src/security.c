/*
 * security.c - XCB grabs, visibility monitoring, and secure memory
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <xcb/xcb.h>
#include <gdk/gdkx.h>

#include "security.h"
#include "main.h"

static pid_t visibility_monitor_pid = 0;
static xcb_connection_t *xcb_conn = NULL;
static xcb_screen_t *xcb_screen = NULL;
static gboolean grab_active = FALSE;
static guint xcb_event_source_id = 0;
static gboolean cleaning_up = FALSE;
static SecurityEventCallback event_callback = NULL;
static gpointer event_callback_data = NULL;

bool security_init(void) {
    int screen_num;

    xcb_conn = xcb_connect(NULL, &screen_num);
    if (xcb_connection_has_error(xcb_conn)) {
        fprintf(stderr, "security: Failed to connect to X server\n");
        xcb_conn = NULL;
        return false;
    }

    xcb_screen_iterator_t iter = xcb_setup_roots_iterator(xcb_get_setup(xcb_conn));
    for (int i = 0; i < screen_num; i++) {
        xcb_screen_next(&iter);
    }
    xcb_screen = iter.data;

    if (!xcb_screen) {
        fprintf(stderr, "security: Failed to get screen\n");
        xcb_disconnect(xcb_conn);
        xcb_conn = NULL;
        return false;
    }

    return true;
}

static gboolean xcb_event_poll(gpointer data) {
    (void)data;

    if (cleaning_up) {
        xcb_event_source_id = 0;
        return G_SOURCE_REMOVE;
    }

    if (!xcb_conn || !grab_active) {
        return G_SOURCE_CONTINUE;
    }

    xcb_generic_event_t *event;
    while ((event = xcb_poll_for_event(xcb_conn)) != NULL) {
        uint8_t type = event->response_type & ~0x80;

        switch (type) {
            case XCB_KEY_PRESS: {
                xcb_key_press_event_t *kp = (xcb_key_press_event_t *)event;
                if (event_callback && !cleaning_up) {
                    SecurityEvent ev = {
                        .type = SECURITY_EVENT_KEY_PRESS,
                        .detail = kp->detail,
                        .state = kp->state,
                        .x = kp->event_x,
                        .y = kp->event_y,
                        .root_x = kp->root_x,
                        .root_y = kp->root_y
                    };
                    event_callback(&ev, event_callback_data);
                }
                break;
            }
            case XCB_KEY_RELEASE:
                break;
            case XCB_BUTTON_PRESS: {
                xcb_button_press_event_t *bp = (xcb_button_press_event_t *)event;
                if (event_callback && !cleaning_up) {
                    SecurityEvent ev = {
                        .type = SECURITY_EVENT_BUTTON_PRESS,
                        .detail = bp->detail,
                        .state = bp->state,
                        .x = bp->event_x,
                        .y = bp->event_y,
                        .root_x = bp->root_x,
                        .root_y = bp->root_y
                    };
                    event_callback(&ev, event_callback_data);
                }
                break;
            }
        }
        free(event);

        if (cleaning_up) {
            break;
        }
    }

    return cleaning_up ? G_SOURCE_REMOVE : G_SOURCE_CONTINUE;
}

void security_cleanup(void) {
    cleaning_up = TRUE;

    if (xcb_event_source_id > 0) {
        g_source_remove(xcb_event_source_id);
        xcb_event_source_id = 0;
    }

    if (visibility_monitor_pid > 0) {
        kill(visibility_monitor_pid, SIGTERM);
        visibility_monitor_pid = 0;
    }

    if (xcb_conn) {
        xcb_disconnect(xcb_conn);
        xcb_conn = NULL;
    }

}

/* Grab keyboard and pointer on root window via XCB */
bool security_grab_input(void) {
    if (!xcb_conn || !xcb_screen) {
        fprintf(stderr, "security: XCB not initialized\n");
        return false;
    }

    xcb_grab_pointer_cookie_t pcookie;
    xcb_grab_pointer_reply_t *preply;
    xcb_grab_keyboard_cookie_t kcookie;
    xcb_grab_keyboard_reply_t *kreply;

    const int max_tries = 10000;
    int tries;

    for (tries = max_tries; tries > 0; tries--) {
        pcookie = xcb_grab_pointer(
            xcb_conn, false, xcb_screen->root,
            XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE |
            XCB_EVENT_MASK_POINTER_MOTION,
            XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC,
            XCB_NONE, XCB_NONE, XCB_CURRENT_TIME);

        preply = xcb_grab_pointer_reply(xcb_conn, pcookie, NULL);
        if (preply && preply->status == XCB_GRAB_STATUS_SUCCESS) {
            free(preply);
            break;
        }
        free(preply);
        usleep(50);
    }

    if (tries <= 0) {
        fprintf(stderr, "security: Failed to grab pointer after %d tries\n", max_tries);
        return false;
    }

    for (tries = max_tries; tries > 0; tries--) {
        kcookie = xcb_grab_keyboard(
            xcb_conn, true, xcb_screen->root,
            XCB_CURRENT_TIME, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);

        kreply = xcb_grab_keyboard_reply(xcb_conn, kcookie, NULL);
        if (kreply && kreply->status == XCB_GRAB_STATUS_SUCCESS) {
            free(kreply);
            break;
        }
        free(kreply);
        usleep(50);
    }

    if (tries <= 0) {
        fprintf(stderr, "security: Failed to grab keyboard after %d tries\n", max_tries);
        xcb_ungrab_pointer(xcb_conn, XCB_CURRENT_TIME);
        xcb_flush(xcb_conn);
        return false;
    }

    xcb_flush(xcb_conn);
    grab_active = TRUE;
    return true;
}

bool security_is_grabbed(void) {
    return grab_active;
}

void security_ungrab_input(void) {
    if (xcb_conn && grab_active) {
        xcb_ungrab_keyboard(xcb_conn, XCB_CURRENT_TIME);
        xcb_ungrab_pointer(xcb_conn, XCB_CURRENT_TIME);
        xcb_flush(xcb_conn);
        grab_active = FALSE;
    }
}

void security_set_event_callback(SecurityEventCallback callback, gpointer data) {
    event_callback = callback;
    event_callback_data = data;

    if (xcb_event_source_id == 0) {
        xcb_event_source_id = g_timeout_add(10, xcb_event_poll, NULL);
    }
}

/* Child process: re-raise window if another window covers it */
static void visibility_monitor_loop(xcb_window_t xwin) {
    xcb_connection_t *conn;
    int screen_num;
    xcb_generic_event_t *event;

    conn = xcb_connect(NULL, &screen_num);
    if (xcb_connection_has_error(conn)) {
        _exit(1);
    }

    uint32_t mask = XCB_EVENT_MASK_VISIBILITY_CHANGE | XCB_EVENT_MASK_STRUCTURE_NOTIFY;
    xcb_change_window_attributes(conn, xwin, XCB_CW_EVENT_MASK, &mask);
    xcb_flush(conn);

    while ((event = xcb_wait_for_event(conn)) != NULL) {
        uint8_t type = event->response_type & ~0x80;

        switch (type) {
            case XCB_VISIBILITY_NOTIFY: {
                xcb_visibility_notify_event_t *vis = (xcb_visibility_notify_event_t *)event;
                if (vis->state != XCB_VISIBILITY_UNOBSCURED) {
                    uint32_t values[] = {XCB_STACK_MODE_ABOVE};
                    xcb_configure_window(conn, xwin, XCB_CONFIG_WINDOW_STACK_MODE, values);
                    xcb_flush(conn);
                }
                break;
            }
            case XCB_UNMAP_NOTIFY:
            case XCB_DESTROY_NOTIFY:
                free(event);
                xcb_disconnect(conn);
                _exit(0);
                break;
        }
        free(event);
    }

    xcb_disconnect(conn);
    _exit(0);
}

void security_setup_visibility_monitor(GtkWidget *window) {
    if (!window) return;

    GdkWindow *gdk_window = gtk_widget_get_window(window);
    if (!gdk_window) return;

    xcb_window_t xwin = gdk_x11_window_get_xid(gdk_window);
    if (!xwin) return;

    pid_t pid = fork();
    if (pid == 0) {
        visibility_monitor_loop(xwin);
        _exit(0);
    } else if (pid > 0) {
        visibility_monitor_pid = pid;
    }
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
void *secure_alloc(size_t size) {
    void *ptr = malloc(size);
    if (ptr) {
        mlock(ptr, size);
        memset(ptr, 0, size);
    }
    return ptr;
}
#pragma GCC diagnostic pop

void secure_free(void *ptr, size_t size) {
    if (ptr) {
        secure_clear(ptr, size);
        munlock(ptr, size);
        free(ptr);
    }
}

void secure_clear(void *ptr, size_t size) {
    if (!ptr || size == 0) return;

#ifdef HAVE_EXPLICIT_BZERO
    explicit_bzero(ptr, size);
#else
    volatile char *vptr = (volatile char *)ptr;
    for (size_t i = 0; i < size; i++) {
        vptr[i] = 0;
    }
#endif
}
