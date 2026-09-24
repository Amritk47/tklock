/*
 * config.h - Configuration parsing
 */

#ifndef TKLOCK_CONFIG_H
#define TKLOCK_CONFIG_H

#include "main.h"
#include <glib.h>

/* Initialize config with defaults */
void config_init(LockerConfig *config);

/* Free allocated strings in config */
void config_cleanup(LockerConfig *config);

/* Parse command-line arguments */
gboolean config_parse_args(int *argc, char ***argv, LockerConfig *config, GError **error);

#endif /* TKLOCK_CONFIG_H */
