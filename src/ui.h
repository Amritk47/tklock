/*
 * ui.h - GTK UI components
 */

#ifndef TKLOCK_UI_H
#define TKLOCK_UI_H

#include <gtk/gtk.h>
#include "main.h"

/* Create and configure the main window */
GtkWidget *ui_create_window(GtkApplication *app);

/* Apply CSS styling */
void ui_apply_css(void);

/* Setup input fields (handles 0, 1, or N fields automatically) */
void ui_setup_fields(void);

/* State updates */
void ui_set_state_idle(void);
void ui_set_state_verifying(void);
void ui_set_state_wrong(const char *message);

/* Input handling */
gboolean ui_on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer data);

/* Clear input fields */
void ui_clear_inputs(void);

/* Get field values (caller must secure_free each non-NULL value) */
void ui_get_field_values(char **values, int *count);

/* Focus first input field */
void ui_focus_first_field(void);

/* Input timeout management */
void ui_reset_input_timeout(void);

/* Visual feedback */
void ui_blink_error(void);

/* Lockout display */
void ui_show_lockout(int seconds_remaining);
void ui_hide_lockout(void);

#endif /* TKLOCK_UI_H */
