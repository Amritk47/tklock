/*
 * auth.c - Authentication command execution
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#include "auth.h"
#include "main.h"
#include "security.h"

/* Build argv: bash -c 'auth_cmd' _ "$1" "$2" ... */
static gchar **build_argv(const char *auth_cmd, char **field_values, int field_count) {
    GPtrArray *argv = g_ptr_array_new();

    g_ptr_array_add(argv, g_strdup("/bin/bash"));
    g_ptr_array_add(argv, g_strdup("-c"));
    g_ptr_array_add(argv, g_strdup(auth_cmd));
    g_ptr_array_add(argv, g_strdup("_"));

    for (int i = 0; i < field_count; i++) {
        g_ptr_array_add(argv, g_strdup(field_values[i] ? field_values[i] : ""));
    }

    g_ptr_array_add(argv, NULL);

    return (gchar **)g_ptr_array_free(argv, FALSE);
}

static void free_argv_secure(gchar **argv) {
    if (!argv) return;

    for (gchar **p = argv; *p; p++) {
        secure_clear(*p, strlen(*p));
        g_free(*p);
    }
    g_free(argv);
}

AuthResult auth_verify(char **field_values, int field_count, char **error_msg) {
    if (error_msg) *error_msg = NULL;

    if (!ctx->config.auth_cmd || strlen(ctx->config.auth_cmd) == 0) {
        return AUTH_SUCCESS;
    }

    gchar **argv = build_argv(ctx->config.auth_cmd, field_values, field_count);
    if (!argv) {
        if (error_msg) *error_msg = g_strdup("Failed to build command");
        return AUTH_API_ERROR;
    }

    gchar *stdout_data = NULL;
    gchar *stderr_data = NULL;
    gint exit_status = 0;
    GError *error = NULL;

    gboolean success = g_spawn_sync(
        NULL, argv, NULL, 0,
        NULL, NULL,
        &stdout_data, &stderr_data,
        &exit_status, &error);

    free_argv_secure(argv);

    if (!success) {
        if (error_msg && error) {
            *error_msg = g_strdup(error->message);
        }
        g_clear_error(&error);
        g_free(stdout_data);
        g_free(stderr_data);
        return AUTH_API_ERROR;
    }

    /* Capture first line of stderr as error message */
    if (stderr_data && strlen(stderr_data) > 0 && error_msg) {
        char *clean = g_strdup(stderr_data);
        char *newline = strchr(clean, '\n');
        if (newline) *newline = '\0';

        GRegex *ansi_regex = g_regex_new("\033\\[[0-9;]*m", 0, 0, NULL);
        if (ansi_regex) {
            char *stripped = g_regex_replace(ansi_regex, clean, -1, 0, "", 0, NULL);
            g_free(clean);
            clean = stripped;
            g_regex_unref(ansi_regex);
        }

        *error_msg = clean;
    }

    g_free(stdout_data);
    g_free(stderr_data);

    if (WIFEXITED(exit_status)) {
        int code = WEXITSTATUS(exit_status);
        if (code >= 0 && code <= 3) {
            return (AuthResult)code;
        }
        return AUTH_API_ERROR;
    }

    return AUTH_API_ERROR;
}

typedef struct {
    char *field_values[MAX_FIELDS];
    int field_count;
    AuthCallback callback;
    gpointer user_data;
    AuthResult result;
    char *error_msg;
} AuthAsyncData;

static void auth_async_data_free(AuthAsyncData *data) {
    for (int i = 0; i < data->field_count; i++) {
        if (data->field_values[i]) {
            secure_free(data->field_values[i], strlen(data->field_values[i]) + 1);
        }
    }
    g_free(data->error_msg);
    g_free(data);
}

static gboolean auth_async_done(gpointer user_data) {
    AuthAsyncData *data = (AuthAsyncData *)user_data;

    if (data->callback) {
        data->callback(data->result, data->error_msg, data->user_data);
    }

    auth_async_data_free(data);
    return G_SOURCE_REMOVE;
}

static gpointer auth_async_thread(gpointer user_data) {
    AuthAsyncData *data = (AuthAsyncData *)user_data;

    data->result = auth_verify(data->field_values, data->field_count, &data->error_msg);

    g_idle_add(auth_async_done, data);

    return NULL;
}

void auth_verify_async(char **field_values, int field_count,
                       AuthCallback callback, gpointer user_data) {
    AuthAsyncData *data = g_new0(AuthAsyncData, 1);

    data->field_count = field_count;
    for (int i = 0; i < field_count && i < MAX_FIELDS; i++) {
        if (field_values[i]) {
            size_t len = strlen(field_values[i]);
            data->field_values[i] = secure_alloc(len + 1);
            if (data->field_values[i]) memcpy(data->field_values[i], field_values[i], len + 1);
        }
    }
    data->callback = callback;
    data->user_data = user_data;

    GThread *thread = g_thread_new("auth", auth_async_thread, data);
    g_thread_unref(thread);
}
