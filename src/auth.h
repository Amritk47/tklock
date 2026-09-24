/*
 * auth.h - Authentication command execution
 */

#ifndef TKLOCK_AUTH_H
#define TKLOCK_AUTH_H

#include "main.h"

/* Run auth command synchronously; field values passed as positional args.
 * Returns exit code mapped to AuthResult. Captures stderr into error_msg. */
AuthResult auth_verify(char **field_values, int field_count, char **error_msg);

/* Async wrapper: runs auth_verify in a thread, invokes callback on main loop */
typedef void (*AuthCallback)(AuthResult result, const char *error_msg, gpointer user_data);
void auth_verify_async(char **field_values, int field_count,
                       AuthCallback callback, gpointer user_data);

#endif /* TKLOCK_AUTH_H */
