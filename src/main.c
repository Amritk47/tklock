/*
 * main.c - program entry point
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <syslog.h>
#include <sys/resource.h>

#include <gdk/gdkx.h>

#include "main.h"
#include "config.h"
#include "security.h"
#include "ui.h"
#include "auth.h"

LockerContext *ctx = NULL;
static guint safety_timeout_id = 0;

static void signal_handler(int signum) {
    (void)signum;
    locker_quit();
}

static void setup_signals(void) {
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);
}

/* Prevent passwords from appearing in core dumps */
static void disable_core_dumps(void) {
    struct rlimit rlim = {0, 0};
    setrlimit(RLIMIT_CORE, &rlim);
}

static gboolean on_safety_timeout(gpointer user_data) {
    (void)user_data;
    fprintf(stderr, "tklock: Safety timeout reached, unlocking\n");
    safety_timeout_id = 0;
    locker_quit();
    return G_SOURCE_REMOVE;
}

static gboolean quit_initiated = FALSE;
static pid_t main_pid = 0;

void locker_quit(void) {
    /* Child processes (visibility monitor) just exit */
    if (main_pid != 0 && getpid() != main_pid) {
        _exit(0);
    }

    if (quit_initiated) return;
    quit_initiated = TRUE;

    security_cleanup();

    if (safety_timeout_id > 0) {
        g_source_remove(safety_timeout_id);
        safety_timeout_id = 0;
    }

    if (ctx) {
        if (ctx->verifying_timer_id > 0) {
            g_source_remove(ctx->verifying_timer_id);
            ctx->verifying_timer_id = 0;
        }
        if (ctx->clock_timer_id > 0) {
            g_source_remove(ctx->clock_timer_id);
            ctx->clock_timer_id = 0;
        }
        if (ctx->input_timeout_id > 0) {
            g_source_remove(ctx->input_timeout_id);
            ctx->input_timeout_id = 0;
        }
    }

    if (ctx && ctx->window) {
        GdkWindow *gdk_win = gtk_widget_get_window(ctx->window);
        if (gdk_win) {
            GdkDisplay *display = gdk_window_get_display(gdk_win);
            GdkSeat *seat = gdk_display_get_default_seat(display);
            gdk_seat_ungrab(seat);
        }
    }

    if (ctx && ctx->window) {
        gtk_widget_destroy(ctx->window);
        ctx->window = NULL;
    }

    if (ctx && ctx->app) {
        g_application_quit(G_APPLICATION(ctx->app));
    }
}

/* Transition state and update UI */
void locker_set_state(LockerState state) {
    if (!ctx) return;

    ctx->state = state;

    switch (state) {
        case STATE_IDLE:
        case STATE_INPUT:
            ui_set_state_idle();
            break;
        case STATE_VERIFYING:
            ui_set_state_verifying();
            break;
        case STATE_WRONG:
            ui_set_state_wrong(ctx->error_message);
            break;
        case STATE_ERROR:
            ui_set_state_wrong(ctx->error_message ? ctx->error_message : "Error");
            break;
    }
}

/* ---- Lockout helpers ---- */

static gboolean is_locked_out(void) {
    if (ctx->config.max_attempts <= 0) return FALSE;
    if (ctx->lockout_until == 0) return FALSE;
    return time(NULL) < ctx->lockout_until;
}

static int get_lockout_remaining(void) {
    if (ctx->lockout_until == 0) return 0;
    int remaining = (int)(ctx->lockout_until - time(NULL));
    return remaining > 0 ? remaining : 0;
}

static void apply_lockout(void) {
    if (ctx->config.max_attempts <= 0) return;
    if (ctx->failed_attempts >= ctx->config.max_attempts) {
        ctx->lockout_until = time(NULL) + ctx->config.lockout_time_secs;
    }
}

/* ---- Logging ---- */

static void log_attempt(gboolean success) {
    if (!ctx->config.log_attempts) return;

    openlog("tklock", LOG_PID, LOG_AUTH);
    if (success) {
        syslog(LOG_INFO, "authentication success");
    } else {
        syslog(LOG_WARNING, "authentication failure (attempt %d)", ctx->failed_attempts);
    }
    closelog();
}

/* Timer wrapper: transitions state back to idle after failed auth */
static gboolean reset_to_idle(gpointer data) {
    (void)data;
    ui_set_state_idle();
    return G_SOURCE_REMOVE;
}

static void do_beep(void) {
    gdk_display_beep(gdk_display_get_default());
}

/* Auth result callback (dispatched to main thread) */
static void on_auth_complete(AuthResult result, const char *error_msg, gpointer user_data) {
    (void)user_data;

    g_free(ctx->error_message);
    ctx->error_message = error_msg ? g_strdup(error_msg) : NULL;

    if (result == AUTH_SUCCESS) {
        log_attempt(TRUE);
        locker_quit();
        return;
    }

    ctx->failed_attempts++;
    log_attempt(FALSE);

    if (ctx->config.beep_on_error) do_beep();
    if (ctx->config.blink_on_error) ui_blink_error();

    apply_lockout();
    locker_set_state(STATE_WRONG);
    ui_clear_inputs();

    g_timeout_add(2000, reset_to_idle, NULL);
}

/* Collect field values and submit for authentication */
static void attempt_auth(void) {
    char *values[MAX_FIELDS];
    int count;

    ui_get_field_values(values, &count);

    if (is_locked_out()) {
        ui_show_lockout(get_lockout_remaining());
        for (int i = 0; i < count; i++) {
            if (values[i]) secure_free(values[i], strlen(values[i]) + 1);
        }
        return;
    }
    ui_hide_lockout();

    if (ctx->config.ignore_empty_fields) {
        gboolean all_empty = TRUE;
        for (int i = 0; i < count; i++) {
            if (values[i] && strlen(values[i]) > 0) {
                all_empty = FALSE;
                break;
            }
        }
        if (all_empty) {
            for (int i = 0; i < count; i++) {
                if (values[i]) secure_free(values[i], strlen(values[i]) + 1);
            }
            return;
        }
    }

    locker_set_state(STATE_VERIFYING);
    auth_verify_async(values, count, on_auth_complete, NULL);

    for (int i = 0; i < count; i++) {
        if (values[i]) secure_free(values[i], strlen(values[i]) + 1);
    }
}

/* Deferred-auth trampoline: runs attempt_auth() on a fresh main-loop iteration. */
static gboolean deferred_attempt_auth(gpointer data) {
    (void)data;
    if (ctx->state != STATE_VERIFYING) attempt_auth();
    return G_SOURCE_REMOVE;
}

/* Public entry point so the Submit button can trigger auth without synthesizing
 * a Return key event (which --force-submit-button would swallow).
 * Auth is deferred to a g_idle callback: on success attempt_auth -> on_auth_complete
 * -> locker_quit destroys the window (and the Submit button). If we ran that
 * synchronously here we would be destroying the button while its "clicked" signal
 * emission is still unwinding on the stack -> use-after-free. Deferring lets the
 * signal emission fully complete first. */
void locker_attempt_auth(void) {
    g_idle_add(deferred_attempt_auth, NULL);
}

static gboolean on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer data) {
    (void)widget;
    (void)data;

    /* Formless: key unlocks (except passthrough keys, and filtered by accept_key) */
    if (MODE_IS_FORMLESS(&ctx->config)) {
        if (ctx->config.pass_media_keys || ctx->config.pass_screen_keys) {
            switch (event->keyval) {
                case GDK_KEY_AudioPlay:
                case GDK_KEY_AudioPause:
                case GDK_KEY_AudioStop:
                case GDK_KEY_AudioNext:
                case GDK_KEY_AudioPrev:
                case GDK_KEY_AudioMute:
                case GDK_KEY_AudioLowerVolume:
                case GDK_KEY_AudioRaiseVolume:
                    if (ctx->config.pass_media_keys) return FALSE;
                    break;
                case GDK_KEY_MonBrightnessUp:
                case GDK_KEY_MonBrightnessDown:
                    if (ctx->config.pass_screen_keys) return FALSE;
                    break;
            }
        }
        /* If accept_key is set, only unlock on that specific key */
        if (ctx->config.accept_key != 0 &&
            gdk_keyval_to_lower(event->keyval) != gdk_keyval_to_lower(ctx->config.accept_key)) {
            return TRUE; /* swallow the keypress, don't unlock */
        }
        locker_quit();
        return TRUE;
    }

    if (event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_KP_Enter) {
        if (!ctx->config.force_submit_button && ctx->state != STATE_VERIFYING) {
            attempt_auth();
        }
        return TRUE;
    }

    return ui_on_key_press(widget, event, data);
}

static gboolean on_button_press(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    (void)widget;
    (void)event;
    (void)data;

    if (MODE_IS_FORMLESS(&ctx->config)) {
        /* If accept_key is set, mouse clicks no longer unlock */
        if (ctx->config.accept_key != 0)
            return TRUE;
        locker_quit();
        return TRUE;
    }

    return FALSE;
}

static void on_activate(GtkApplication *app, gpointer user_data) {
    (void)user_data;

    ctx->window = ui_create_window(app);
    if (!ctx->window) {
        fprintf(stderr, "tklock: Failed to create window\n");
        g_application_quit(G_APPLICATION(app));
        return;
    }

    ui_apply_css();

    ui_setup_fields();

    g_signal_connect(ctx->window, "key-press-event", G_CALLBACK(on_key_press), NULL);
    g_signal_connect(ctx->window, "button-press-event", G_CALLBACK(on_button_press), NULL);

    gtk_widget_show_all(ctx->window);

    /* Flush events so the window is mapped before grabbing */
    while (gtk_events_pending()) {
        gtk_main_iteration();
    }

    if (!ctx->config.no_grab) {
        GdkWindow *gdk_win = gtk_widget_get_window(ctx->window);
        if (gdk_win) {
            GdkDisplay *display = gdk_window_get_display(gdk_win);
            GdkSeat *seat = gdk_display_get_default_seat(display);

            GdkGrabStatus status = gdk_seat_grab(
                seat, gdk_win,
                GDK_SEAT_CAPABILITY_ALL,
                TRUE, NULL, NULL, NULL, NULL);

            if (status != GDK_GRAB_SUCCESS) {
                fprintf(stderr, "tklock: Failed to grab input (status=%d)\n", status);
            }
        }
    }

    security_setup_visibility_monitor(ctx->window);
    ui_focus_first_field();

    if (ctx->config.timeout_secs > 0) {
        safety_timeout_id = g_timeout_add_seconds(
            ctx->config.timeout_secs, on_safety_timeout, NULL);
    }
}

static void on_shutdown(GtkApplication *app, gpointer user_data) {
    (void)app;
    (void)user_data;

    if (quit_initiated) return;

    if (safety_timeout_id > 0) {
        g_source_remove(safety_timeout_id);
        safety_timeout_id = 0;
    }

    if (ctx) {
        if (ctx->verifying_timer_id > 0) {
            g_source_remove(ctx->verifying_timer_id);
            ctx->verifying_timer_id = 0;
        }
        if (ctx->clock_timer_id > 0) {
            g_source_remove(ctx->clock_timer_id);
            ctx->clock_timer_id = 0;
        }
        if (ctx->input_timeout_id > 0) {
            g_source_remove(ctx->input_timeout_id);
            ctx->input_timeout_id = 0;
        }
    }

    security_ungrab_input();
    security_cleanup();
}

static GLogWriterOutput suppress_gdk_device_warning(GLogLevelFlags level,
                                                     const GLogField *fields,
                                                     gsize n_fields,
                                                     gpointer data) {
    (void)data;
    for (gsize i = 0; i < n_fields; i++) {
        if (strcmp(fields[i].key, "MESSAGE") == 0 &&
            fields[i].value &&
            strstr((const char *)fields[i].value, "not holding a GdkDevice"))
            return G_LOG_WRITER_HANDLED;
    }
    return g_log_writer_default(level, fields, n_fields, data);
}

int main(int argc, char *argv[]) {
    GError *error = NULL;
    int status;

    main_pid = getpid();

    /* Suppress harmless "not holding a GdkDevice" warnings from
     * device-less X11 focus events */
    g_log_set_writer_func(suppress_gdk_device_warning, NULL, NULL);

    disable_core_dumps();

    ctx = g_new0(LockerContext, 1);
    if (!ctx) {
        fprintf(stderr, "tklock: Failed to allocate context\n");
        return EXIT_FAILURE;
    }

    config_init(&ctx->config);

    if (!config_parse_args(&argc, &argv, &ctx->config, &error)) {
        fprintf(stderr, "tklock: %s\n", error->message);
        g_error_free(error);
        config_cleanup(&ctx->config);
        g_free(ctx);
        return EXIT_FAILURE;
    }

    if (!security_init()) {
        fprintf(stderr, "tklock: Failed to initialize security\n");
        config_cleanup(&ctx->config);
        g_free(ctx);
        return EXIT_FAILURE;
    }

    setup_signals();

    ctx->app = gtk_application_new(NULL, G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(ctx->app, "activate", G_CALLBACK(on_activate), NULL);
    g_signal_connect(ctx->app, "shutdown", G_CALLBACK(on_shutdown), NULL);

    status = g_application_run(G_APPLICATION(ctx->app), 0, NULL);

    g_object_unref(ctx->app);
    g_free(ctx->error_message);
    config_cleanup(&ctx->config);
    g_free(ctx);

    return status;
}
