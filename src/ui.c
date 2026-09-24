/*
 * ui.c - GTK UI layout and state management
 */

#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <gdk/gdkx.h>

#include "ui.h"
#include "main.h"
#include "security.h"

/* Widget references */
static GtkWidget *main_container = NULL;
static GtkWidget *frame_widget = NULL;
static GtkWidget *message_label = NULL;
static GtkWidget *status_label = NULL;
static GtkWidget *clock_label = NULL;
static GtkWidget *info_label = NULL;
static GtkWidget *field_entries[MAX_FIELDS] = {0};
static GtkWidget *submit_button = NULL;
static GtkWidget *attempts_label = NULL;
static GtkWidget *lockout_label = NULL;
static char *verif_text_base = NULL;

/* Animated "verifying..." ellipsis */
static gboolean animate_ellipsis(gpointer data) {
    (void)data;

    if (!ctx || ctx->state != STATE_VERIFYING || !status_label) {
        if (ctx) ctx->verifying_timer_id = 0;
        return G_SOURCE_REMOVE;
    }

    ctx->ellipsis_state = (ctx->ellipsis_state + 1) % 4;

    const char *dots[] = {"", ".", "..", "..."};
    char *text = g_strdup_printf("%s%s", verif_text_base ? verif_text_base : "verifying",
                                  dots[ctx->ellipsis_state]);
    gtk_label_set_text(GTK_LABEL(status_label), text);
    g_free(text);

    return G_SOURCE_CONTINUE;
}

/* Set background color on a widget via per-widget CssProvider */
static void set_widget_bg_color(GtkWidget *widget, const GdkRGBA *color) {
    GtkCssProvider *provider = g_object_get_data(G_OBJECT(widget), "bg-css-provider");

    if (!provider) {
        provider = gtk_css_provider_new();
        gtk_style_context_add_provider(
            gtk_widget_get_style_context(widget),
            GTK_STYLE_PROVIDER(provider),
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION + 1);
        g_object_set_data_full(G_OBJECT(widget), "bg-css-provider",
                               provider, g_object_unref);
    }

    char *css = g_strdup_printf(
        "* { background-color: rgba(%d,%d,%d,%g); }",
        (int)(color->red * 255), (int)(color->green * 255),
        (int)(color->blue * 255), color->alpha);
    gtk_css_provider_load_from_data(provider, css, -1, NULL);
    g_free(css);
}

/* Parse "RRGGBB" or "#RRGGBB" to GdkRGBA; falls back to dark gray */
static void parse_hex_color(const char *hex, GdkRGBA *color) {
    unsigned int r, g, b;

    if (hex) {
        if (hex[0] == '#') hex++;
        if (strlen(hex) >= 6 && sscanf(hex, "%02x%02x%02x", &r, &g, &b) == 3) {
            color->red = r / 255.0;
            color->green = g / 255.0;
            color->blue = b / 255.0;
            color->alpha = 1.0;
            return;
        }
    }

    /* Fallback */
    color->red = 0.133;
    color->green = 0.133;
    color->blue = 0.133;
    color->alpha = 1.0;
}

/* Generate CSS from config (colors, typography, frame) */
static char *generate_css(void) {
    int base_size = ctx->config.font_size > 0 ? ctx->config.font_size : 14;
    int message_size = base_size + 2;
    int small_size = base_size - 1;
    int tiny_size = base_size - 2;
    int h_padding = (int)(ctx->config.frame_padding * 1.33);

    /* Frame border */
    char frame_border[64];
    if (ctx->config.no_frame) {
        snprintf(frame_border, sizeof(frame_border), "none");
    } else {
        snprintf(frame_border, sizeof(frame_border), "2px solid #%s", ctx->config.color_border);
    }

    return g_strdup_printf(
        "/* Generated theme */\n"
        "\n"
        "window {\n"
        "    background-color: #%s;\n"
        "    font-family: %s;\n"
        "}\n"
        "\n"
        "#lock-frame {\n"
        "    background-color: #%s;\n"
        "    border-radius: 0;\n"
        "    border: %s;\n"
        "    padding: %dpx %dpx;\n"
        "}\n"
        "\n"
        "#lock-container {\n"
        "    padding: 12px 20px;\n"
        "    background-color: transparent;\n"
        "}\n"
        "\n"
        "scrolledwindow {\n"
        "    background-color: transparent;\n"
        "}\n"
        "\n"
        "scrolledwindow viewport {\n"
        "    background-color: transparent;\n"
        "}\n"
        "\n"
        "#message-label {\n"
        "    font-size: %dpx;\n"
        "    font-weight: bold;\n"
        "    color: #%s;\n"
        "    margin-bottom: 10px;\n"
        "    letter-spacing: 0.5px;\n"
        "}\n"
        "\n"
        "#hint-label {\n"
        "    font-size: %dpx;\n"
        "    color: #%s;\n"
        "    font-style: normal;\n"
        "}\n"
        "\n"
        "#field-label {\n"
        "    font-size: %dpx;\n"
        "    font-weight: bold;\n"
        "    color: #%s;\n"
        "    margin-bottom: 2px;\n"
        "}\n"
        "\n"
        "entry {\n"
        "    font-size: %dpx;\n"
        "    font-family: %s;\n"
        "    padding: 8px 12px;\n"
        "    border-radius: 0;\n"
        "    border: 2px solid #%s;\n"
        "    background-color: #%s;\n"
        "    color: #%s;\n"
        "    min-width: 200px;\n"
        "    caret-color: #%s;\n"
        "    box-shadow: none;\n"
        "}\n"
        "\n"
        "entry:focus {\n"
        "    border-color: #%s;\n"
        "    box-shadow: none;\n"
        "}\n"
        "\n"
        "entry:disabled {\n"
        "    background-color: #%s;\n"
        "    color: #%s;\n"
        "    border-color: #%s;\n"
        "}\n"
        "\n"
        "entry selection {\n"
        "    background-color: #%s;\n"
        "    color: #%s;\n"
        "}\n"
        "\n"
        "#status-label {\n"
        "    font-size: %dpx;\n"
        "    color: #%s;\n"
        "    min-height: 18px;\n"
        "    margin-top: 8px;\n"
        "}\n"
        "\n"
        "#status-verifying {\n"
        "    font-size: %dpx;\n"
        "    color: #%s;\n"
        "    font-style: normal;\n"
        "    min-height: 18px;\n"
        "    margin-top: 8px;\n"
        "}\n"
        "\n"
        "#status-wrong {\n"
        "    font-size: %dpx;\n"
        "    color: #%s;\n"
        "    font-weight: bold;\n"
        "    min-height: 18px;\n"
        "    margin-top: 8px;\n"
        "}\n"
        "\n"
        "#attempts-label {\n"
        "    font-size: %dpx;\n"
        "    color: #%s;\n"
        "    margin-top: 10px;\n"
        "}\n"
        "\n"
        "#clock-label {\n"
        "    font-size: %dpx;\n"
        "    font-family: %s;\n"
        "    color: #%s;\n"
        "    margin-bottom: 8px;\n"
        "}\n"
        "\n"
        "#info-label {\n"
        "    font-size: %dpx;\n"
        "    font-family: %s;\n"
        "    font-weight: bold;\n"
        "    color: #%s;\n"
        "    margin-bottom: 6px;\n"
        "}\n"
        "\n"
        "#submit-button {\n"
        "    font-size: %dpx;\n"
        "    font-family: %s;\n"
        "    padding: 8px 24px;\n"
        "    margin-top: 8px;\n"
        "    background: #%s;\n"
        "    color: #%s;\n"
        "    border: 2px solid #%s;\n"
        "    border-radius: 0;\n"
        "    box-shadow: none;\n"
        "    outline: none;\n"
        "    -gtk-outline-radius: 0;\n"
        "}\n"
        "\n"
        "#submit-button:focus, #submit-button:checked {\n"
        "    box-shadow: none;\n"
        "    outline: none;\n"
        "    -gtk-outline-radius: 0;\n"
        "    background-image: none;\n"
        "}\n"
        "\n"
        "#submit-button:hover {\n"
        "    opacity: 0.85;\n"
        "}\n"
        "\n"
        "#submit-button:active {\n"
        "    opacity: 0.65;\n"
        "    box-shadow: none;\n"
        "    outline: none;\n"
        "    -gtk-outline-radius: 0;\n"
        "    background-image: none;\n"
        "}\n"
        "\n"
        "#lockout-label {\n"
        "    font-size: %dpx;\n"
        "    color: #%s;\n"
        "    font-weight: bold;\n"
        "    margin-top: 8px;\n"
        "}\n",
        /* window */          ctx->config.color_bg, ctx->config.font_family,
        /* lock-frame */      ctx->config.color_frame, frame_border,
                              ctx->config.frame_padding, h_padding,
        /* message */         message_size, ctx->config.color_fg,
        /* hint */            small_size, ctx->config.color_accent,
        /* field-label */     small_size, ctx->config.color_accent,
        /* entry */           base_size, ctx->config.font_family,
                              ctx->config.color_border, ctx->config.color_frame,
                              ctx->config.color_fg, ctx->config.color_fg,
        /* entry:focus */     ctx->config.color_accent,
        /* entry:disabled */  ctx->config.color_frame, ctx->config.color_border,
                              ctx->config.color_border,
        /* selection */       ctx->config.color_accent, ctx->config.color_bg,
        /* status */          small_size, ctx->config.color_border,
        /* verifying */       small_size, ctx->config.color_accent,
        /* wrong */           small_size, ctx->config.color_error,
        /* attempts */        tiny_size, ctx->config.color_border,
        /* clock */           message_size, ctx->config.font_family, ctx->config.color_clock,
        /* info */            small_size, ctx->config.font_family, ctx->config.color_info,
        /* submit */          base_size, ctx->config.font_family,
                              ctx->config.color_accent, ctx->config.color_frame,
                              ctx->config.color_accent,
        /* lockout */         small_size, ctx->config.color_error
    );
}

/* Load generated CSS into the default screen */
void ui_apply_css(void) {
    GtkCssProvider *provider = gtk_css_provider_new();
    GError *error = NULL;

    char *css = generate_css();
    gtk_css_provider_load_from_data(provider, css, -1, &error);
    g_free(css);

    if (error) {
        fprintf(stderr, "tklock: Failed to load CSS: %s\n", error->message);
        g_error_free(error);
    }

    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    g_object_unref(provider);
}

/* Resolve position preset + offset into x, y, w, h */
static void calculate_window_geometry(int screen_w, int screen_h,
                                       int *out_x, int *out_y,
                                       int *out_w, int *out_h) {
    int win_w = ctx->config.win_width > 0 ? ctx->config.win_width : screen_w;
    int win_h = ctx->config.win_height > 0 ? ctx->config.win_height : screen_h;

    if (win_w > screen_w) win_w = screen_w;
    if (win_h > screen_h) win_h = screen_h;

    int base_x = 0, base_y = 0;

    switch (ctx->config.win_position) {
        case WIN_POS_CENTER:
            base_x = (screen_w - win_w) / 2;
            base_y = (screen_h - win_h) / 2;
            break;
        case WIN_POS_TOP_LEFT:
            base_x = 0;
            base_y = 0;
            break;
        case WIN_POS_TOP_RIGHT:
            base_x = screen_w - win_w;
            base_y = 0;
            break;
        case WIN_POS_BOTTOM_LEFT:
            base_x = 0;
            base_y = screen_h - win_h;
            break;
        case WIN_POS_BOTTOM_RIGHT:
            base_x = screen_w - win_w;
            base_y = screen_h - win_h;
            break;
        case WIN_POS_FULLSCREEN:
        default:
            base_x = 0;
            base_y = 0;
            win_w = screen_w;
            win_h = screen_h;
            break;
    }

    int offset_x = (screen_w * ctx->config.win_offset_x) / 100;
    int offset_y = (screen_h * ctx->config.win_offset_y) / 100;

    *out_x = base_x + offset_x;
    *out_y = base_y + offset_y;
    *out_w = win_w;
    *out_h = win_h;

    if (*out_x < 0) *out_x = 0;
    if (*out_y < 0) *out_y = 0;
    if (*out_x + *out_w > screen_w) *out_x = screen_w - *out_w;
    if (*out_y + *out_h > screen_h) *out_y = screen_h - *out_h;
}

/* On realize: set override_redirect and apply window geometry */
static void on_realize(GtkWidget *widget, gpointer data) {
    (void)data;
    GdkWindow *gdk_win = gtk_widget_get_window(widget);
    if (gdk_win) {
        if (!ctx->config.no_grab)
            gdk_window_set_override_redirect(gdk_win, TRUE);

        GdkDisplay *display = gdk_window_get_display(gdk_win);
        GdkMonitor *monitor = gdk_display_get_monitor_at_window(display, gdk_win);
        GdkRectangle geom;
        gdk_monitor_get_geometry(monitor, &geom);
        int screen_w = geom.width;
        int screen_h = geom.height;

        int win_x, win_y, win_w, win_h;
        calculate_window_geometry(screen_w, screen_h, &win_x, &win_y, &win_w, &win_h);

        gdk_window_move_resize(gdk_win, win_x, win_y, win_w, win_h);
    }
}

/* Dynamically size frame to ~50% of window width (clamped to min/max) */
static void on_outer_size_allocate(GtkWidget *widget, GdkRectangle *alloc, gpointer data) {
    (void)widget;
    (void)data;

    if (!frame_widget) return;

    int avail_w = alloc->width;
    int avail_h = alloc->height;
    int min_w = ctx->config.frame_min_width;
    int margin = 20;

    int target_w = avail_w / 2;
    if (target_w < min_w) target_w = min_w;
    if (target_w > avail_w - margin * 2) target_w = avail_w - margin * 2;
    if (target_w < 200) target_w = 200;

    gtk_widget_set_size_request(frame_widget, target_w, -1);

    /* Cap scrolled window height (frame -> viewport -> scrolled) */
    GtkWidget *viewport = gtk_widget_get_parent(frame_widget);
    GtkWidget *scrolled = viewport ? gtk_widget_get_parent(viewport) : NULL;
    if (scrolled && GTK_IS_SCROLLED_WINDOW(scrolled)) {
        int max_h = avail_h - margin * 2;
        if (max_h < 100) max_h = 100;
        gtk_scrolled_window_set_max_content_height(
            GTK_SCROLLED_WINDOW(scrolled), max_h);
    }
}

/* Create and configure the main application window */
GtkWidget *ui_create_window(GtkApplication *app) {
    GtkWidget *window;
    GdkRGBA bg_color;

    window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "tklock");
    gtk_window_set_decorated(GTK_WINDOW(window), FALSE);

    if (!ctx->config.no_grab) {
        gtk_window_set_skip_taskbar_hint(GTK_WINDOW(window), TRUE);
        gtk_window_set_skip_pager_hint(GTK_WINDOW(window), TRUE);
        gtk_window_set_keep_above(GTK_WINDOW(window), TRUE);
        gtk_window_set_type_hint(GTK_WINDOW(window), GDK_WINDOW_TYPE_HINT_SPLASHSCREEN);
    }

    g_signal_connect(window, "realize", G_CALLBACK(on_realize), NULL);

    if (ctx->config.win_position == WIN_POS_FULLSCREEN &&
        ctx->config.win_width == 0 && ctx->config.win_height == 0) {
        gtk_window_fullscreen(GTK_WINDOW(window));
    }

    parse_hex_color(ctx->config.color_bg, &bg_color);
    set_widget_bg_color(window, &bg_color);

    /* Optional border overlay */
    GtkWidget *outer_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    if (ctx->config.border_color) {
        GdkRGBA border_color;
        parse_hex_color(ctx->config.border_color, &border_color);

        GtkWidget *border_box = gtk_event_box_new();
        set_widget_bg_color(border_box, &border_color);

        int bw = ctx->config.border_width;
        gtk_widget_set_margin_start(outer_box, bw);
        gtk_widget_set_margin_end(outer_box, bw);
        gtk_widget_set_margin_top(outer_box, bw);
        gtk_widget_set_margin_bottom(outer_box, bw);

        set_widget_bg_color(outer_box, &bg_color);

        gtk_container_add(GTK_CONTAINER(border_box), outer_box);
        gtk_container_add(GTK_CONTAINER(window), border_box);
    } else {
        gtk_container_add(GTK_CONTAINER(window), outer_box);
    }

    /* Content container */
    main_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
    gtk_widget_set_halign(main_container, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(main_container, GTK_ALIGN_CENTER);
    gtk_widget_set_name(main_container, "lock-container");

    /* Frame (border via CSS) */
    frame_widget = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type(GTK_FRAME(frame_widget), GTK_SHADOW_NONE);
    gtk_widget_set_name(frame_widget, "lock-frame");
    gtk_widget_set_halign(frame_widget, GTK_ALIGN_FILL);
    gtk_widget_set_valign(frame_widget, GTK_ALIGN_CENTER);
    gtk_container_add(GTK_CONTAINER(frame_widget), main_container);

    /* Scrollable wrapper for overflow */
    GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                    GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_propagate_natural_height(
        GTK_SCROLLED_WINDOW(scrolled), TRUE);
    gtk_widget_set_halign(scrolled, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(scrolled, GTK_ALIGN_CENTER);
    gtk_container_add(GTK_CONTAINER(scrolled), frame_widget);

    /* Centering wrapper */
    GtkWidget *center_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_halign(center_box, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(center_box, GTK_ALIGN_CENTER);
    gtk_widget_set_hexpand(center_box, TRUE);
    gtk_widget_set_vexpand(center_box, TRUE);
    gtk_box_pack_start(GTK_BOX(center_box), scrolled, FALSE, FALSE, 0);

    /* Dynamic frame sizing on resize */
    g_signal_connect(outer_box, "size-allocate",
                     G_CALLBACK(on_outer_size_allocate), NULL);

    gtk_box_pack_start(GTK_BOX(outer_box), center_box, TRUE, TRUE, 0);

    return window;
}

/* ---- Helpers ---- */

/* Map config text_alignment to GTK alignment/justification */
static GtkAlign text_halign(void) {
    switch (ctx->config.text_alignment) {
        case TEXT_ALIGN_LEFT:  return GTK_ALIGN_START;
        case TEXT_ALIGN_RIGHT: return GTK_ALIGN_END;
        default:               return GTK_ALIGN_CENTER;
    }
}

static GtkJustification text_justify(void) {
    switch (ctx->config.text_alignment) {
        case TEXT_ALIGN_LEFT:  return GTK_JUSTIFY_LEFT;
        case TEXT_ALIGN_RIGHT: return GTK_JUSTIFY_RIGHT;
        default:               return GTK_JUSTIFY_CENTER;
    }
}

static GtkWidget *create_wrapped_label(const char *text, const char *name,
                                         int max_width_chars) {
    GtkWidget *label = gtk_label_new(text);
    gtk_widget_set_name(label, name);
    gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
    gtk_label_set_line_wrap_mode(GTK_LABEL(label), PANGO_WRAP_WORD_CHAR);
    gtk_label_set_max_width_chars(GTK_LABEL(label), max_width_chars);
    gtk_label_set_justify(GTK_LABEL(label), text_justify());
    gtk_widget_set_halign(label, text_halign());
    return label;
}

/* ---- Clock ---- */

static gboolean update_clock(gpointer data) {
    (void)data;
    if (!clock_label) return G_SOURCE_REMOVE;

    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char buf[32];
    strftime(buf, sizeof(buf), "%a %b %d %H:%M:%S", tm);
    gtk_label_set_text(GTK_LABEL(clock_label), buf);
    return G_SOURCE_CONTINUE;
}

/* Build the info string (username@hostname) */
static char *build_info_string(void) {
    GString *info = g_string_new(NULL);

    if (ctx->config.show_username) {
        struct passwd *pw = getpwuid(getuid());
        if (pw) g_string_append(info, pw->pw_name);
    }

    if (ctx->config.show_username && ctx->config.show_hostname)
        g_string_append_c(info, '@');

    if (ctx->config.show_hostname) {
        char hostname[256];
        if (gethostname(hostname, sizeof(hostname)) == 0) {
            hostname[sizeof(hostname) - 1] = '\0';
            g_string_append(info, hostname);
        }
    }

    char *result = (info->len > 0) ? g_strdup(info->str) : NULL;
    g_string_free(info, TRUE);
    return result;
}

/* ---- Header row (clock + info on same line, or standalone) ---- */

static void setup_header(void) {
    gboolean has_clock = ctx->config.show_clock;
    gboolean has_info = ctx->config.show_username || ctx->config.show_hostname;
    char *info_str = has_info ? build_info_string() : NULL;

    if (has_clock && info_str) {
        /* Both: horizontal box with clock on left, info on right, fixed gap */
        GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 20);
        gtk_widget_set_halign(hbox, text_halign());

        clock_label = gtk_label_new("");
        gtk_widget_set_name(clock_label, "clock-label");
        gtk_box_pack_start(GTK_BOX(hbox), clock_label, FALSE, FALSE, 0);

        info_label = gtk_label_new(info_str);
        gtk_widget_set_name(info_label, "info-label");
        gtk_box_pack_start(GTK_BOX(hbox), info_label, FALSE, FALSE, 0);

        gtk_box_pack_start(GTK_BOX(main_container), hbox, FALSE, FALSE, 0);

        update_clock(NULL);
        ctx->clock_timer_id = g_timeout_add(1000, update_clock, NULL);
    } else if (has_clock) {
        /* Clock only: centered */
        clock_label = create_wrapped_label("", "clock-label", 30);
        gtk_box_pack_start(GTK_BOX(main_container), clock_label, FALSE, FALSE, 0);
        update_clock(NULL);
        ctx->clock_timer_id = g_timeout_add(1000, update_clock, NULL);
    } else if (info_str) {
        /* Info only: centered */
        info_label = create_wrapped_label(info_str, "info-label", 40);
        gtk_box_pack_start(GTK_BOX(main_container), info_label, FALSE, FALSE, 0);
    }

    g_free(info_str);
}

/* ---- Message label ---- */

static void create_message_label(void) {
    if (ctx->config.message_markup) {
        /* Pango-markup message: caller controls coloring and line breaks;
         * no wrap/width cap so its own newlines render verbatim */
        message_label = gtk_label_new(NULL);
        gtk_widget_set_name(message_label, "message-label");
        gtk_label_set_markup(GTK_LABEL(message_label), ctx->config.message_text);
        gtk_label_set_line_wrap(GTK_LABEL(message_label), FALSE);
        gtk_label_set_justify(GTK_LABEL(message_label), text_justify());
        gtk_widget_set_halign(message_label, text_halign());
    } else {
        message_label = create_wrapped_label(ctx->config.message_text,
                                              "message-label", 40);
    }
    gtk_box_pack_start(GTK_BOX(main_container), message_label, FALSE, FALSE, 6);
}

/* ---- Status label ---- */

static void create_status_label(void) {
    status_label = create_wrapped_label("", "status-label", 40);
    gtk_box_pack_start(GTK_BOX(main_container), status_label, FALSE, FALSE, 5);
}

/* ---- Attempts label ---- */

static void create_attempts_label(void) {
    if (ctx->config.show_failed_attempts) {
        attempts_label = create_wrapped_label("", "attempts-label", 40);
        gtk_box_pack_end(GTK_BOX(main_container), attempts_label, FALSE, FALSE, 10);
    }
}

/* ---- Input timeout ---- */

static gboolean on_input_timeout(gpointer data) {
    (void)data;
    ctx->input_timeout_id = 0;
    ui_clear_inputs();
    return G_SOURCE_REMOVE;
}

void ui_reset_input_timeout(void) {
    if (ctx->config.input_timeout_secs <= 0) return;

    /* Cancel existing timer */
    if (ctx->input_timeout_id > 0) {
        g_source_remove(ctx->input_timeout_id);
    }

    ctx->input_timeout_id = g_timeout_add_seconds(
        ctx->config.input_timeout_secs, on_input_timeout, NULL);
}

static void on_entry_changed(GtkEditable *editable, gpointer data) {
    (void)editable;
    (void)data;
    ui_reset_input_timeout();
}

/* Submit button: trigger auth directly */
static void on_submit_clicked(GtkButton *button, gpointer data) {
    (void)button;
    (void)data;
    locker_attempt_auth();
}

/* ---- Field setup ---- */

void ui_setup_fields(void) {
    int n = ctx->config.input_fields;

    /* Optional header elements */
    if (ctx->config.show_clock || ctx->config.show_username || ctx->config.show_hostname)
        setup_header();

    if (!ctx->config.hide_message)
        create_message_label();

    /* Formless: no input fields */
    if (n == 0) {
        if (!ctx->config.hide_hint) {
            GtkWidget *hint = create_wrapped_label(ctx->config.hint_text,
                                                    "hint-label", 40);
            gtk_box_pack_start(GTK_BOX(main_container), hint, FALSE, FALSE, 6);
        }
        return;
    }

    /* Build field groups: label above entry, aligned per text_alignment */
    for (int i = 0; i < n && i < MAX_FIELDS; i++) {
        GtkWidget *field_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
        gtk_widget_set_halign(field_box, text_halign());

        if (ctx->config.field_labels[i]) {
            GtkWidget *label = gtk_label_new(ctx->config.field_labels[i]);
            gtk_widget_set_name(label, "field-label");
            gtk_widget_set_halign(label, GTK_ALIGN_START);
            gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
            gtk_label_set_line_wrap_mode(GTK_LABEL(label), PANGO_WRAP_WORD_CHAR);
            gtk_label_set_max_width_chars(GTK_LABEL(label), 30);
            gtk_box_pack_start(GTK_BOX(field_box), label, FALSE, FALSE, 0);
        }

        field_entries[i] = gtk_entry_new();
        gtk_entry_set_width_chars(GTK_ENTRY(field_entries[i]), ctx->config.entry_width);
        gtk_widget_set_halign(field_entries[i], GTK_ALIGN_FILL);
        gtk_widget_set_can_focus(field_entries[i], TRUE);

        if (!ctx->config.field_visible[i]) {
            gtk_entry_set_visibility(GTK_ENTRY(field_entries[i]), FALSE);
            gtk_entry_set_invisible_char(GTK_ENTRY(field_entries[i]), '*');
        }
        if (ctx->config.center_text) {
            gtk_entry_set_alignment(GTK_ENTRY(field_entries[i]), 0.5);
        }

        g_signal_connect(field_entries[i], "changed", G_CALLBACK(on_entry_changed), NULL);
        gtk_box_pack_start(GTK_BOX(field_box), field_entries[i], FALSE, FALSE, 0);

        int spacing = (i < n - 1) ? 8 : 4;
        gtk_box_pack_start(GTK_BOX(main_container), field_box, FALSE, FALSE, spacing);
    }

    if (ctx->config.show_submit_button) {
        submit_button = gtk_button_new_with_label("Submit");
        gtk_widget_set_name(submit_button, "submit-button");
        gtk_widget_set_halign(submit_button, text_halign());
        g_signal_connect(submit_button, "clicked", G_CALLBACK(on_submit_clicked), NULL);
        gtk_box_pack_start(GTK_BOX(main_container), submit_button, FALSE, FALSE, 6);
    }

    create_status_label();
    create_attempts_label();
}

/* ---- State management ---- */

void ui_set_state_idle(void) {
    if (ctx->verifying_timer_id > 0) {
        g_source_remove(ctx->verifying_timer_id);
        ctx->verifying_timer_id = 0;
    }

    g_free(verif_text_base);
    verif_text_base = NULL;

    if (status_label) {
        gtk_label_set_text(GTK_LABEL(status_label), "");
        gtk_widget_set_name(status_label, "status-label");
    }

    if (attempts_label && ctx->config.show_failed_attempts && ctx->failed_attempts > 0) {
        char *text = g_strdup_printf("%d failed attempt%s",
                                      ctx->failed_attempts,
                                      ctx->failed_attempts == 1 ? "" : "s");
        gtk_label_set_text(GTK_LABEL(attempts_label), text);
        g_free(text);
    }

    int n = ctx->config.input_fields;
    for (int i = 0; i < n && i < MAX_FIELDS; i++) {
        if (field_entries[i])
            gtk_widget_set_sensitive(field_entries[i], TRUE);
    }
    if (submit_button)
        gtk_widget_set_sensitive(submit_button, TRUE);
}

void ui_set_state_verifying(void) {
    if (status_label) {
        g_free(verif_text_base);
        verif_text_base = g_strdup(ctx->config.verif_text);
        ctx->ellipsis_state = 0;
        gtk_label_set_text(GTK_LABEL(status_label), verif_text_base);
        gtk_widget_set_name(status_label, "status-verifying");

        if (ctx->verifying_timer_id > 0)
            g_source_remove(ctx->verifying_timer_id);
        if (ctx->config.animate_verifying)
            ctx->verifying_timer_id = g_timeout_add(300, animate_ellipsis, NULL);
    }

    int n = ctx->config.input_fields;
    for (int i = 0; i < n && i < MAX_FIELDS; i++) {
        if (field_entries[i])
            gtk_widget_set_sensitive(field_entries[i], FALSE);
    }
    if (submit_button)
        gtk_widget_set_sensitive(submit_button, FALSE);
}

void ui_set_state_wrong(const char *message) {
    if (ctx->verifying_timer_id > 0) {
        g_source_remove(ctx->verifying_timer_id);
        ctx->verifying_timer_id = 0;
    }

    if (status_label) {
        const char *display_msg = message;
        if (!display_msg || strlen(display_msg) == 0) {
            display_msg = ctx->config.failed_text;
        }
        gtk_label_set_text(GTK_LABEL(status_label), display_msg);
        gtk_widget_set_name(status_label, "status-wrong");
    }

    int n = ctx->config.input_fields;
    for (int i = 0; i < n && i < MAX_FIELDS; i++) {
        if (field_entries[i])
            gtk_widget_set_sensitive(field_entries[i], TRUE);
    }
    if (submit_button)
        gtk_widget_set_sensitive(submit_button, TRUE);
}

/* ---- Key press handler ---- */

gboolean ui_on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer data) {
    (void)widget;
    (void)data;

    int n = ctx->config.input_fields;

    if (ctx->config.beep_on_keypress && n > 0) {
        gdk_display_beep(gdk_display_get_default());
    }

    if (event->keyval == GDK_KEY_Escape && ctx->config.clear_on_escape) {
        ui_clear_inputs();
        if (n > 0 && field_entries[0])
            gtk_widget_grab_focus(field_entries[0]);
        return TRUE;
    }

    if (n > 1 && (event->keyval == GDK_KEY_Tab || event->keyval == GDK_KEY_ISO_Left_Tab)) {
        GtkWidget *focused = gtk_window_get_focus(GTK_WINDOW(ctx->window));
        int current = -1;
        for (int i = 0; i < n && i < MAX_FIELDS; i++) {
            if (field_entries[i] == focused) { current = i; break; }
        }

        if (current >= 0) {
            int next;
            if (event->keyval == GDK_KEY_ISO_Left_Tab) {
                next = (current - 1 + n) % n;
            } else {
                next = (current + 1) % n;
            }
            if (field_entries[next])
                gtk_widget_grab_focus(field_entries[next]);
            return TRUE;
        }
    }

    return FALSE;
}

/* ---- Input management ---- */

void ui_clear_inputs(void) {
    int n = ctx->config.input_fields;
    for (int i = 0; i < n && i < MAX_FIELDS; i++) {
        if (field_entries[i])
            gtk_entry_set_text(GTK_ENTRY(field_entries[i]), "");
    }
}

void ui_get_field_values(char **values, int *count) {
    int n = ctx->config.input_fields;
    if (n > MAX_FIELDS) n = MAX_FIELDS;
    *count = n;

    for (int i = 0; i < n; i++) {
        values[i] = NULL;
        if (field_entries[i]) {
            const char *text = gtk_entry_get_text(GTK_ENTRY(field_entries[i]));
            if (text) {
                size_t len = strlen(text);
                values[i] = secure_alloc(len + 1);
                if (values[i]) memcpy(values[i], text, len + 1);
            }
        }
    }
}

void ui_focus_first_field(void) {
    int n = ctx->config.input_fields;
    if (n > 0 && field_entries[0]) {
        gtk_widget_grab_focus(field_entries[0]);
    }
}

/* ---- Error blink ---- */

static gboolean blink_restore(gpointer data) {
    (void)data;
    if (ctx && ctx->window) {
        GdkRGBA bg;
        parse_hex_color(ctx->config.color_bg, &bg);
        set_widget_bg_color(ctx->window, &bg);
    }
    return G_SOURCE_REMOVE;
}

void ui_blink_error(void) {
    if (!ctx || !ctx->window) return;

    GdkRGBA error_color;
    parse_hex_color(ctx->config.color_error, &error_color);
    set_widget_bg_color(ctx->window, &error_color);

    /* Restore after 100ms */
    g_timeout_add(100, blink_restore, NULL);
}

/* ---- Lockout display ---- */

void ui_show_lockout(int seconds_remaining) {
    if (!lockout_label) {
        lockout_label = create_wrapped_label("", "lockout-label", 40);
        gtk_box_pack_start(GTK_BOX(main_container), lockout_label, FALSE, FALSE, 5);
        gtk_widget_show(lockout_label);
    }

    char *text = g_strdup_printf("Locked out for %d second%s",
                                  seconds_remaining,
                                  seconds_remaining == 1 ? "" : "s");
    gtk_label_set_text(GTK_LABEL(lockout_label), text);
    g_free(text);
}

void ui_hide_lockout(void) {
    if (lockout_label) {
        gtk_widget_destroy(lockout_label);
        lockout_label = NULL;
    }
}
