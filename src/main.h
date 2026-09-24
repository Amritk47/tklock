/*
 * main.h - Shared types and declarations
 */

#ifndef TKLOCK_MAIN_H
#define TKLOCK_MAIN_H

#include <gtk/gtk.h>
#include <stdbool.h>
#include <time.h>

/* Mode macros based on input_fields count */
#define MODE_IS_FORMLESS(cfg)     ((cfg)->input_fields == 0)
#define MODE_IS_SINGLE_FIELD(cfg) ((cfg)->input_fields == 1)
#define MODE_IS_MULTI_FIELD(cfg)  ((cfg)->input_fields > 1)
#define MODE_HAS_FIELDS(cfg)      ((cfg)->input_fields > 0)

/* Max supported input fields */
#define MAX_FIELDS 8

/* Authentication result codes (match auth-cmd exit codes) */
typedef enum {
    AUTH_SUCCESS = 0,
    AUTH_WRONG_CREDS = 1,
    AUTH_INSUFFICIENT_PRIV = 2,
    AUTH_API_ERROR = 3
} AuthResult;

/* Application state */
typedef enum {
    STATE_IDLE,
    STATE_INPUT,
    STATE_VERIFYING,
    STATE_WRONG,
    STATE_ERROR
} LockerState;

/* Window position presets */
typedef enum {
    WIN_POS_FULLSCREEN,
    WIN_POS_CENTER,
    WIN_POS_TOP_LEFT,
    WIN_POS_TOP_RIGHT,
    WIN_POS_BOTTOM_LEFT,
    WIN_POS_BOTTOM_RIGHT
} WindowPosition;

/* Text alignment for message/hint/status labels and field groups */
typedef enum {
    TEXT_ALIGN_CENTER,
    TEXT_ALIGN_LEFT,
    TEXT_ALIGN_RIGHT
} TextAlignment;

/* Configuration from command line */
typedef struct {
    /* Input fields */
    int input_fields;                    /* Number of input fields (0-8), default 0 */
    char *field_labels[MAX_FIELDS];      /* Labels for each field */
    bool field_visible[MAX_FIELDS];      /* visible (default) or hidden (masked) */

    /* Behavior flags */
    bool nofork;
    bool ignore_empty_fields;
    bool show_failed_attempts;
    bool pass_media_keys;
    bool pass_screen_keys;

    /* Safety timeout in seconds (0 = disabled) */
    int timeout_secs;

    /* Input handling */
    int max_attempts;                    /* 0 = unlimited */
    int input_timeout_secs;              /* 0 = disabled */
    bool clear_on_escape;

    /* Visual feedback */
    bool show_clock;
    bool show_username;
    bool show_hostname;
    bool animate_verifying;              /* default: true */
    bool blink_on_error;

    /* Security */
    int lockout_time_secs;               /* Duration of lockout (default: 300) */
    bool log_attempts;
    bool no_grab;

    /* Accessibility */
    bool beep_on_error;
    bool beep_on_keypress;

    /* Layout/UX */
    int entry_width;                     /* default: 25 chars */
    bool center_text;
    TextAlignment text_alignment;        /* message/hint/fields alignment (default: center) */
    bool hide_hint;
    bool hide_message;
    bool message_markup;                 /* render --message-text as Pango markup (color spans, no auto-wrap) */
    bool show_submit_button;
    bool force_submit_button;            /* disable Enter-to-submit; only the Submit button submits (implies show_submit_button) */

    /* Window geometry */
    WindowPosition win_position;
    int win_width;
    int win_height;
    int win_offset_x;
    int win_offset_y;

    /* Border styling */
    char *border_color;
    int border_width;

    /* Theme colors (RRGGBB format) */
    char *color_bg;
    char *color_fg;
    char *color_frame;
    char *color_border;
    char *color_accent;
    char *color_error;
    char *color_clock;
    char *color_info;

    /* Text strings */
    char *message_text;
    char *verif_text;
    char *failed_text;
    char *hint_text;

    /* Typography */
    char *font_family;
    int font_size;

    /* Frame styling */
    int frame_padding;
    int frame_min_width;
    bool no_frame;

    /* Authentication */
    char *auth_cmd;

    /* Formless mode: restrict unlock to a specific key (0 = any key) */
    guint accept_key;
} LockerConfig;

/* Global application context */
typedef struct {
    LockerConfig config;
    LockerState state;
    GtkApplication *app;
    GtkWidget *window;

    /* Statistics */
    int failed_attempts;

    /* Custom error message from auth */
    char *error_message;

    /* Animation timer ID */
    guint verifying_timer_id;
    int ellipsis_state;

    /* Lockout state */
    time_t lockout_until;

    /* Timers */
    guint input_timeout_id;
    guint clock_timer_id;
} LockerContext;

/* Global context - defined in main.c */
extern LockerContext *ctx;

/* Main functions */
void locker_quit(void);
void locker_set_state(LockerState state);
void locker_attempt_auth(void);   /* run the auth-cmd against current field values (Submit button) */

#endif /* TKLOCK_MAIN_H */
