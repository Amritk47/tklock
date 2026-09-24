/*
 * config.c - Configuration parsing
 */

#include <string.h>
#include "config.h"

#ifndef VERSION
#define VERSION "dev"
#endif

static gchar *opt_auth_cmd = NULL;
static gchar *opt_message_text = NULL;
static gchar *opt_verif_text = NULL;
static gchar *opt_failed_text = NULL;
static gchar *opt_hint_text = NULL;
static gboolean opt_nofork = FALSE;
static gboolean opt_ignore_empty = FALSE;
static gboolean opt_show_attempts = FALSE;
static gboolean opt_pass_media = FALSE;
static gboolean opt_pass_screen = FALSE;
static gint opt_timeout = 0;

static gchar *opt_position = NULL;
static gint opt_width = 0;
static gint opt_height = 0;
static gint opt_offset_x = 0;
static gint opt_offset_y = 0;

static gchar *opt_border = NULL;
static gint opt_border_width = 2;

static gchar *opt_color_bg = NULL;
static gchar *opt_color_fg = NULL;
static gchar *opt_color_frame = NULL;
static gchar *opt_color_border = NULL;
static gchar *opt_color_accent = NULL;
static gchar *opt_color_error = NULL;
static gchar *opt_color_clock = NULL;
static gchar *opt_color_info = NULL;

static gchar *opt_font_family = NULL;
static gint opt_font_size = 0;
static gint opt_frame_padding = 0;
static gint opt_frame_min_width = 0;
static gboolean opt_no_frame = FALSE;

static gint opt_input_fields = -1;
static gchar *opt_field_text[MAX_FIELDS] = {NULL};
static gchar *opt_field_mode[MAX_FIELDS] = {NULL};

static gint opt_max_attempts = 0;
static gint opt_input_timeout = 0;
static gboolean opt_clear_on_escape = FALSE;

static gboolean opt_show_clock = FALSE;
static gboolean opt_show_username = FALSE;
static gboolean opt_show_hostname = FALSE;
static gboolean opt_no_animate = FALSE;
static gboolean opt_blink_on_error = FALSE;

static gint opt_lockout_time = 0;
static gboolean opt_log_attempts = FALSE;
static gboolean opt_no_grab = FALSE;

static gboolean opt_beep_on_error = FALSE;
static gboolean opt_beep_on_keypress = FALSE;

static gint opt_entry_width = 0;
static gboolean opt_center_input = FALSE;
static gchar *opt_text_alignment = NULL;
static gboolean opt_hide_hint = FALSE;
static gboolean opt_hide_message = FALSE;
static gboolean opt_message_markup = FALSE;
static gboolean opt_show_submit_button = FALSE;
static gboolean opt_force_submit_button = FALSE;
static gchar *opt_accept_key = NULL;

/* GOption entries */
static GOptionEntry option_entries[] = {
    /* Authentication */
    {"auth-cmd", 0, 0, G_OPTION_ARG_STRING, &opt_auth_cmd,
     "External authentication command", "COMMAND"},

    /* Input fields */
    {"input-fields", 'i', 0, G_OPTION_ARG_INT, &opt_input_fields,
     "Number of input fields (0-8, default 0)", "N"},
    {"field1-text", 0, 0, G_OPTION_ARG_STRING, &opt_field_text[0],
     "Label for field 1", "LABEL"},
    {"field2-text", 0, 0, G_OPTION_ARG_STRING, &opt_field_text[1],
     "Label for field 2", "LABEL"},
    {"field3-text", 0, 0, G_OPTION_ARG_STRING, &opt_field_text[2],
     "Label for field 3", "LABEL"},
    {"field4-text", 0, 0, G_OPTION_ARG_STRING, &opt_field_text[3],
     "Label for field 4", "LABEL"},
    {"field5-text", 0, 0, G_OPTION_ARG_STRING, &opt_field_text[4],
     "Label for field 5", "LABEL"},
    {"field6-text", 0, 0, G_OPTION_ARG_STRING, &opt_field_text[5],
     "Label for field 6", "LABEL"},
    {"field7-text", 0, 0, G_OPTION_ARG_STRING, &opt_field_text[6],
     "Label for field 7", "LABEL"},
    {"field8-text", 0, 0, G_OPTION_ARG_STRING, &opt_field_text[7],
     "Label for field 8", "LABEL"},
    {"field1", 0, 0, G_OPTION_ARG_STRING, &opt_field_mode[0],
     "Field 1 visibility: visible or hidden (default: visible)", "MODE"},
    {"field2", 0, 0, G_OPTION_ARG_STRING, &opt_field_mode[1],
     "Field 2 visibility: visible or hidden (default: visible)", "MODE"},
    {"field3", 0, 0, G_OPTION_ARG_STRING, &opt_field_mode[2],
     "Field 3 visibility: visible or hidden (default: visible)", "MODE"},
    {"field4", 0, 0, G_OPTION_ARG_STRING, &opt_field_mode[3],
     "Field 4 visibility: visible or hidden (default: visible)", "MODE"},
    {"field5", 0, 0, G_OPTION_ARG_STRING, &opt_field_mode[4],
     "Field 5 visibility: visible or hidden (default: visible)", "MODE"},
    {"field6", 0, 0, G_OPTION_ARG_STRING, &opt_field_mode[5],
     "Field 6 visibility: visible or hidden (default: visible)", "MODE"},
    {"field7", 0, 0, G_OPTION_ARG_STRING, &opt_field_mode[6],
     "Field 7 visibility: visible or hidden (default: visible)", "MODE"},
    {"field8", 0, 0, G_OPTION_ARG_STRING, &opt_field_mode[7],
     "Field 8 visibility: visible or hidden (default: visible)", "MODE"},

    /* Display text */
    {"message-text", 0, 0, G_OPTION_ARG_STRING, &opt_message_text,
     "Main message displayed", "TEXT"},
    {"verif-text", 0, 0, G_OPTION_ARG_STRING, &opt_verif_text,
     "Text during verification", "TEXT"},
    {"failed-text", 0, 0, G_OPTION_ARG_STRING, &opt_failed_text,
     "Text on verification failure", "TEXT"},
    {"hint-text", 0, 0, G_OPTION_ARG_STRING, &opt_hint_text,
     "Hint text when no input fields", "TEXT"},

    /* Behavior flags */
    {"nofork", 'n', 0, G_OPTION_ARG_NONE, &opt_nofork,
     "Run in foreground (don't fork)", NULL},
    {"ignore-empty-fields", 'e', 0, G_OPTION_ARG_NONE, &opt_ignore_empty,
     "Don't submit when all fields are empty", NULL},
    {"show-failed-attempts", 'f', 0, G_OPTION_ARG_NONE, &opt_show_attempts,
     "Display failed attempt counter", NULL},
    {"pass-media-keys", 0, 0, G_OPTION_ARG_NONE, &opt_pass_media,
     "Pass media keys through (formless mode)", NULL},
    {"pass-screen-keys", 0, 0, G_OPTION_ARG_NONE, &opt_pass_screen,
     "Pass screen brightness keys through", NULL},

    /* Safety/testing */
    {"timeout", 0, 0, G_OPTION_ARG_INT, &opt_timeout,
     "Auto-unlock after N seconds (for testing)", "SECONDS"},

    /* Input handling */
    {"max-attempts", 0, 0, G_OPTION_ARG_INT, &opt_max_attempts,
     "Max auth attempts before lockout (0 = unlimited)", "N"},
    {"input-timeout", 0, 0, G_OPTION_ARG_INT, &opt_input_timeout,
     "Clear input after N seconds of inactivity", "SECONDS"},
    {"clear-on-escape", 0, 0, G_OPTION_ARG_NONE, &opt_clear_on_escape,
     "Clear input fields when Escape pressed", NULL},

    /* Visual feedback */
    {"show-clock", 0, 0, G_OPTION_ARG_NONE, &opt_show_clock,
     "Display current time in UI", NULL},
    {"show-username", 0, 0, G_OPTION_ARG_NONE, &opt_show_username,
     "Display logged-in username", NULL},
    {"show-hostname", 0, 0, G_OPTION_ARG_NONE, &opt_show_hostname,
     "Display system hostname", NULL},
    {"no-animate", 0, 0, G_OPTION_ARG_NONE, &opt_no_animate,
     "Disable animated ellipsis during verification", NULL},
    {"blink-on-error", 0, 0, G_OPTION_ARG_NONE, &opt_blink_on_error,
     "Flash screen on auth failure", NULL},

    /* Security */
    {"lockout-time", 0, 0, G_OPTION_ARG_INT, &opt_lockout_time,
     "Lockout duration after max-attempts (default: 300)", "SECONDS"},
    {"log-attempts", 0, 0, G_OPTION_ARG_NONE, &opt_log_attempts,
     "Log auth attempts to syslog", NULL},
    {"no-grab", 0, 0, G_OPTION_ARG_NONE, &opt_no_grab,
     "Skip keyboard/pointer grab (testing only)", NULL},

    /* Accessibility */
    {"beep-on-error", 0, 0, G_OPTION_ARG_NONE, &opt_beep_on_error,
     "Beep on auth failure", NULL},
    {"beep-on-keypress", 0, 0, G_OPTION_ARG_NONE, &opt_beep_on_keypress,
     "Beep on each keypress", NULL},

    /* Layout/UX */
    {"entry-width", 0, 0, G_OPTION_ARG_INT, &opt_entry_width,
     "Entry field width in characters (default: 25)", "CHARS"},
    {"center-input", 0, 0, G_OPTION_ARG_NONE, &opt_center_input,
     "Center-align typed text inside input entries", NULL},
    {"text-alignment", 0, 0, G_OPTION_ARG_STRING, &opt_text_alignment,
     "Alignment for message/hint/status text and field groups: center (default), left, right", "ALIGN"},
    {"hide-hint", 0, 0, G_OPTION_ARG_NONE, &opt_hide_hint,
     "Hide hint text in formless mode", NULL},
    {"hide-message", 0, 0, G_OPTION_ARG_NONE, &opt_hide_message,
     "Hide message text", NULL},
    {"message-markup", 0, 0, G_OPTION_ARG_NONE, &opt_message_markup,
     "Render --message-text as Pango markup (enables color spans, disables auto-wrap)", NULL},
    {"show-submit-button", 0, 0, G_OPTION_ARG_NONE, &opt_show_submit_button,
     "Show a submit button", NULL},
    {"force-submit-button", 0, 0, G_OPTION_ARG_NONE, &opt_force_submit_button,
     "Disable Enter-to-submit; only the Submit button submits (implies --show-submit-button)", NULL},
    {"accept-key", 0, 0, G_OPTION_ARG_STRING, &opt_accept_key,
     "Formless mode: only this key unlocks (e.g. Return, space, F1)", "KEYNAME"},

    /* Theme colors */
    {"color-bg", 0, 0, G_OPTION_ARG_STRING, &opt_color_bg,
     "Background color (default: 171421)", "RRGGBB"},
    {"color-fg", 0, 0, G_OPTION_ARG_STRING, &opt_color_fg,
     "Text/foreground color (default: ffffff)", "RRGGBB"},
    {"color-frame", 0, 0, G_OPTION_ARG_STRING, &opt_color_frame,
     "Frame background color (default: 2d2d2d)", "RRGGBB"},
    {"color-border", 0, 0, G_OPTION_ARG_STRING, &opt_color_border,
     "Border/outline color (default: 4a4a4a)", "RRGGBB"},
    {"color-accent", 0, 0, G_OPTION_ARG_STRING, &opt_color_accent,
     "Accent/focus color (default: 5294e2)", "RRGGBB"},
    {"color-error", 0, 0, G_OPTION_ARG_STRING, &opt_color_error,
     "Error text color (default: ff5555)", "RRGGBB"},
    {"color-clock", 0, 0, G_OPTION_ARG_STRING, &opt_color_clock,
     "Clock text color (default: 55ff55)", "RRGGBB"},
    {"color-info", 0, 0, G_OPTION_ARG_STRING, &opt_color_info,
     "Username/hostname text color (default: 55ff55)", "RRGGBB"},

    /* Typography */
    {"font-family", 0, 0, G_OPTION_ARG_STRING, &opt_font_family,
     "Font family (CSS format, default: JetBrains Mono with monospace fallbacks)", "FONT"},
    {"font-size", 0, 0, G_OPTION_ARG_INT, &opt_font_size,
     "Base font size in pixels (default: 14)", "PIXELS"},

    /* Frame styling */
    {"frame-padding", 0, 0, G_OPTION_ARG_INT, &opt_frame_padding,
     "Frame padding in pixels (default: 32)", "PIXELS"},
    {"frame-min-width", 0, 0, G_OPTION_ARG_INT, &opt_frame_min_width,
     "Frame minimum width in pixels (default: 420)", "PIXELS"},
    {"no-frame", 0, 0, G_OPTION_ARG_NONE, &opt_no_frame,
     "Hide the frame border entirely", NULL},

    /* Window geometry */
    {"position", 0, 0, G_OPTION_ARG_STRING, &opt_position,
     "Window position: center, top-left, top-right, bottom-left, bottom-right", "POS"},
    {"width", 0, 0, G_OPTION_ARG_INT, &opt_width,
     "Window width in pixels (0 = fullscreen)", "PIXELS"},
    {"height", 0, 0, G_OPTION_ARG_INT, &opt_height,
     "Window height in pixels (0 = fullscreen)", "PIXELS"},
    {"offset-x", 0, 0, G_OPTION_ARG_INT, &opt_offset_x,
     "X offset percentage from position (-100 to 100)", "PERCENT"},
    {"offset-y", 0, 0, G_OPTION_ARG_INT, &opt_offset_y,
     "Y offset percentage from position (-100 to 100)", "PERCENT"},

    /* Border styling */
    {"border", 0, 0, G_OPTION_ARG_STRING, &opt_border,
     "Border color (RRGGBB hex)", "COLOR"},
    {"border-width", 0, 0, G_OPTION_ARG_INT, &opt_border_width,
     "Border thickness in pixels (default: 2)", "PIXELS"},

    {NULL}
};

static WindowPosition parse_position(const char *str) {
    if (!str) return WIN_POS_FULLSCREEN;

    if (g_ascii_strcasecmp(str, "center") == 0) return WIN_POS_CENTER;
    if (g_ascii_strcasecmp(str, "top-left") == 0) return WIN_POS_TOP_LEFT;
    if (g_ascii_strcasecmp(str, "top-right") == 0) return WIN_POS_TOP_RIGHT;
    if (g_ascii_strcasecmp(str, "bottom-left") == 0) return WIN_POS_BOTTOM_LEFT;
    if (g_ascii_strcasecmp(str, "bottom-right") == 0) return WIN_POS_BOTTOM_RIGHT;

    return WIN_POS_FULLSCREEN;
}

static TextAlignment parse_text_alignment(const char *str) {
    if (!str) return TEXT_ALIGN_CENTER;

    if (g_ascii_strcasecmp(str, "left") == 0) return TEXT_ALIGN_LEFT;
    if (g_ascii_strcasecmp(str, "right") == 0) return TEXT_ALIGN_RIGHT;

    return TEXT_ALIGN_CENTER;
}

void config_init(LockerConfig *config) {
    memset(config, 0, sizeof(LockerConfig));

    /* Fields default to visible */
    for (int i = 0; i < MAX_FIELDS; i++)
        config->field_visible[i] = TRUE;

    /* Non-zero defaults */
    config->animate_verifying = TRUE;
    config->lockout_time_secs = 300;
    config->entry_width = 25;
    config->border_width = 2;
    config->font_size = 14;
    config->frame_padding = 32;
    config->frame_min_width = 420;

    /* Theme colors */
    config->color_bg = g_strdup("171421");
    config->color_fg = g_strdup("ffffff");
    config->color_frame = g_strdup("2d2d2d");
    config->color_border = g_strdup("4a4a4a");
    config->color_accent = g_strdup("5294e2");
    config->color_error = g_strdup("ff5555");
    config->color_clock = g_strdup("55ff55");
    config->color_info = g_strdup("55ff55");

    /* Text strings */
    config->message_text = g_strdup("Press any key to continue");
    config->verif_text = g_strdup("verifying");
    config->failed_text = g_strdup("verification failed");
    config->hint_text = g_strdup("Press any key to continue");

    /* Typography */
    config->font_family = g_strdup("\"JetBrains Mono\", \"Fira Code\", \"Source Code Pro\", \"DejaVu Sans Mono\", \"Liberation Mono\", monospace");
}

void config_cleanup(LockerConfig *config) {
    for (int i = 0; i < MAX_FIELDS; i++) {
        g_free(config->field_labels[i]);
        config->field_labels[i] = NULL;
    }

    g_free(config->message_text);
    g_free(config->verif_text);
    g_free(config->failed_text);
    g_free(config->hint_text);
    g_free(config->auth_cmd);
    g_free(config->border_color);
    g_free(config->color_bg);
    g_free(config->color_fg);
    g_free(config->color_frame);
    g_free(config->color_border);
    g_free(config->color_accent);
    g_free(config->color_error);
    g_free(config->color_clock);
    g_free(config->color_info);
    g_free(config->font_family);

    config->message_text = NULL;
    config->verif_text = NULL;
    config->failed_text = NULL;
    config->hint_text = NULL;
    config->auth_cmd = NULL;
    config->border_color = NULL;
    config->color_bg = NULL;
    config->color_fg = NULL;
    config->color_frame = NULL;
    config->color_border = NULL;
    config->color_accent = NULL;
    config->color_error = NULL;
    config->color_clock = NULL;
    config->color_info = NULL;
    config->font_family = NULL;
}

static void apply_string_opt(char **dest, const gchar *opt) {
    if (opt) {
        g_free(*dest);
        *dest = g_strdup(opt);
    }
}

gboolean config_parse_args(int *argc, char ***argv, LockerConfig *config, GError **error) {
    GOptionContext *context;
    gboolean success;

    opt_auth_cmd = NULL;
    opt_message_text = NULL;
    opt_verif_text = NULL;
    opt_failed_text = NULL;
    opt_hint_text = NULL;
    opt_nofork = FALSE;
    opt_ignore_empty = FALSE;
    opt_show_attempts = FALSE;
    opt_pass_media = FALSE;
    opt_pass_screen = FALSE;
    opt_timeout = 0;

    opt_position = NULL;
    opt_width = 0;
    opt_height = 0;
    opt_offset_x = 0;
    opt_offset_y = 0;

    opt_border = NULL;
    opt_border_width = 2;

    opt_color_bg = NULL;
    opt_color_fg = NULL;
    opt_color_frame = NULL;
    opt_color_border = NULL;
    opt_color_accent = NULL;
    opt_color_error = NULL;
    opt_color_clock = NULL;
    opt_color_info = NULL;

    opt_font_family = NULL;
    opt_font_size = 0;
    opt_frame_padding = 0;
    opt_frame_min_width = 0;
    opt_no_frame = FALSE;

    opt_input_fields = -1;
    for (int i = 0; i < MAX_FIELDS; i++) {
        opt_field_text[i] = NULL;
        opt_field_mode[i] = NULL;
    }

    opt_max_attempts = 0;
    opt_input_timeout = 0;
    opt_clear_on_escape = FALSE;
    opt_show_clock = FALSE;
    opt_show_username = FALSE;
    opt_show_hostname = FALSE;
    opt_no_animate = FALSE;
    opt_blink_on_error = FALSE;
    opt_lockout_time = 0;
    opt_log_attempts = FALSE;
    opt_no_grab = FALSE;
    opt_beep_on_error = FALSE;
    opt_beep_on_keypress = FALSE;
    opt_entry_width = 0;
    opt_center_input = FALSE;
    opt_text_alignment = NULL;
    opt_hide_hint = FALSE;
    opt_hide_message = FALSE;
    opt_message_markup = FALSE;
    opt_show_submit_button = FALSE;
    opt_force_submit_button = FALSE;
    opt_accept_key = NULL;

    /* Handle --version before GOption parsing */
    for (int i = 1; i < *argc; i++) {
        if (strcmp((*argv)[i], "-v") == 0 || strcmp((*argv)[i], "--version") == 0) {
            printf("tklock %s\n", VERSION);
            exit(EXIT_SUCCESS);
        }
    }

    context = g_option_context_new("- tklock screen locker");
    g_option_context_add_main_entries(context, option_entries, NULL);
    g_option_context_add_group(context, gtk_get_option_group(TRUE));

    success = g_option_context_parse(context, argc, argv, error);
    g_option_context_free(context);

    if (!success) {
        return FALSE;
    }

    apply_string_opt(&config->auth_cmd, opt_auth_cmd);
    apply_string_opt(&config->message_text, opt_message_text);
    apply_string_opt(&config->verif_text, opt_verif_text);
    apply_string_opt(&config->failed_text, opt_failed_text);
    apply_string_opt(&config->hint_text, opt_hint_text);

    config->nofork = opt_nofork;
    config->ignore_empty_fields = opt_ignore_empty;
    config->show_failed_attempts = opt_show_attempts;
    config->pass_media_keys = opt_pass_media;
    config->pass_screen_keys = opt_pass_screen;
    config->timeout_secs = opt_timeout;

    /* Input fields */
    if (opt_input_fields >= 0) {
        config->input_fields = CLAMP(opt_input_fields, 0, MAX_FIELDS);
    }
    for (int i = 0; i < MAX_FIELDS; i++) {
        apply_string_opt(&config->field_labels[i], opt_field_text[i]);
        if (opt_field_mode[i]) {
            if (g_ascii_strcasecmp(opt_field_mode[i], "hidden") == 0) {
                config->field_visible[i] = FALSE;
            } else {
                config->field_visible[i] = TRUE;
            }
        }
    }

    /* Input handling */
    if (opt_max_attempts > 0) config->max_attempts = opt_max_attempts;
    if (opt_input_timeout > 0) config->input_timeout_secs = opt_input_timeout;
    config->clear_on_escape = opt_clear_on_escape;

    /* Visual feedback */
    config->show_clock = opt_show_clock;
    config->show_username = opt_show_username;
    config->show_hostname = opt_show_hostname;
    if (opt_no_animate) config->animate_verifying = FALSE;
    config->blink_on_error = opt_blink_on_error;

    /* Security */
    if (opt_lockout_time > 0) config->lockout_time_secs = opt_lockout_time;
    config->log_attempts = opt_log_attempts;
    config->no_grab = opt_no_grab;

    /* Accessibility */
    config->beep_on_error = opt_beep_on_error;
    config->beep_on_keypress = opt_beep_on_keypress;

    /* Layout/UX */
    if (opt_entry_width > 0) config->entry_width = opt_entry_width;
    config->center_text = opt_center_input;
    config->text_alignment = parse_text_alignment(opt_text_alignment);
    config->hide_hint = opt_hide_hint;
    config->hide_message = opt_hide_message;
    config->message_markup = opt_message_markup;
    config->force_submit_button = opt_force_submit_button;
    config->show_submit_button = opt_show_submit_button || opt_force_submit_button;

    /* Window geometry */
    config->win_position = parse_position(opt_position);
    config->win_width = opt_width;
    config->win_height = opt_height;
    config->win_offset_x = CLAMP(opt_offset_x, -100, 100);
    config->win_offset_y = CLAMP(opt_offset_y, -100, 100);

    /* Border */
    apply_string_opt(&config->border_color, opt_border);
    config->border_width = opt_border_width > 0 ? opt_border_width : 2;

    /* Theme colors */
    apply_string_opt(&config->color_bg, opt_color_bg);
    apply_string_opt(&config->color_fg, opt_color_fg);
    apply_string_opt(&config->color_frame, opt_color_frame);
    apply_string_opt(&config->color_border, opt_color_border);
    apply_string_opt(&config->color_accent, opt_color_accent);
    apply_string_opt(&config->color_error, opt_color_error);
    apply_string_opt(&config->color_clock, opt_color_clock);
    apply_string_opt(&config->color_info, opt_color_info);

    /* Typography */
    apply_string_opt(&config->font_family, opt_font_family);
    if (opt_font_size > 0) config->font_size = opt_font_size;

    /* Frame styling */
    if (opt_frame_padding > 0) config->frame_padding = opt_frame_padding;
    if (opt_frame_min_width > 0) config->frame_min_width = opt_frame_min_width;
    config->no_frame = opt_no_frame;

    /* Formless accept-key: parse GDK key name to keyval */
    if (opt_accept_key) {
        guint kv = gdk_keyval_from_name(opt_accept_key);
        if (kv == GDK_KEY_VoidSymbol) {
            g_set_error(error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                        "Unknown key name for --accept-key: '%s'", opt_accept_key);
            return FALSE;
        }
        config->accept_key = kv;
    }

    return TRUE;
}
