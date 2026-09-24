/*
 * security.h - Grabs, visibility monitoring, and secure memory
 */

#ifndef TKLOCK_SECURITY_H
#define TKLOCK_SECURITY_H

#include <stdbool.h>
#include <stddef.h>
#include <gtk/gtk.h>
#include <xcb/xcb.h>

typedef enum {
    SECURITY_EVENT_KEY_PRESS,
    SECURITY_EVENT_KEY_RELEASE,
    SECURITY_EVENT_BUTTON_PRESS,
    SECURITY_EVENT_BUTTON_RELEASE
} SecurityEventType;

typedef struct {
    SecurityEventType type;
    uint32_t detail;      /* keycode or button */
    uint16_t state;       /* modifier mask */
    int16_t x, y;
    int16_t root_x, root_y;
} SecurityEvent;

typedef void (*SecurityEventCallback)(const SecurityEvent *event, gpointer data);

/* XCB connection lifecycle */
bool security_init(void);
void security_cleanup(void);

/* Root-window grabs (keyboard + pointer) */
bool security_grab_input(void);
void security_ungrab_input(void);
bool security_is_grabbed(void);

/* XCB event polling with callback */
void security_set_event_callback(SecurityEventCallback callback, gpointer data);

/* Fork a child that re-raises the window if obscured */
void security_setup_visibility_monitor(GtkWidget *window);

/* Locked memory: mlock'd alloc, explicit_bzero clear */
void *secure_alloc(size_t size);
void secure_free(void *ptr, size_t size);
void secure_clear(void *ptr, size_t size);

#endif /* TKLOCK_SECURITY_H */
