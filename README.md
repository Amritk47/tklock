# TKLOCK #

GTK3 screen locker with configurable input fields, theming, and external authentication. Supports formless dismiss, single-field prompts, and multi-field forms (up to 8 fields).

## dependencies ##

``` shell
sudo apt install libgtk-3-dev libxcb1-dev
```

## building ##

``` shell
cd ${project_root}
make clean && make
```

Binary output: `build/tklock`

``` shell
sudo make install
```

## modes ##

| mode | flag | description |
| :--- | :--- | :--- |
| formless | `-i 0` (default) | any key or click dismisses the screen |
| single field | `-i 1` | one input field (e.g. password prompt) |
| multi-field | `-i 2` ... `-i 8` | multiple labeled fields in a vertical layout |

## input fields ##

| option | description |
| :--- | :--- |
| `-i N`, `--input-fields=N` | number of input fields (0-8, default: 0) |
| `--field1-text=LABEL` ... `--field8-text=LABEL` | label for each field |
| `--field1=MODE` ... `--field8=MODE` | field visibility: `visible` (default) or `hidden` |

Field values are passed to the auth command as positional arguments (`$1`, `$2`, ...).

## authentication ##

| option | description |
| :--- | :--- |
| `--auth-cmd=COMMAND` | shell command for authentication |

The auth command is invoked as `bash -c 'COMMAND' _ "$1" "$2" ...` where `$1`-`$N` are the field values. If no `--auth-cmd` is provided, any input unlocks immediately.

tklock interprets the exit code to determine the UI response:

| exit code | behavior |
| :--- | :--- |
| `0` | unlock and exit |
| other | show error message, increment failed attempt counter |

Non `0` exit codes all result in the same UI behavior. The distinction exists so that the auth command can communicate the reason for failure to logging (`--log-attempts`) or to a custom error message via stderr.

The first line of stderr output (if any) is displayed as the error message. ANSI escape codes are stripped automatically. If stderr is empty, the default `--failed-text` is shown.

## options ##

### display text ###

| option | default |
| :--- | :--- |
| `--message-text=TEXT` | `Press any key to continue` |
| `--verif-text=TEXT` | `verifying` |
| `--failed-text=TEXT` | `verification failed` |
| `--hint-text=TEXT` | `Press any key to continue` |

### behavior ###

| option | description |
| :--- | :--- |
| `-v`, `--version` | show version number |
| `-h`, `--help` | show help message |
| `-n`, `--nofork` | run in foreground |
| `-e`, `--ignore-empty-fields` | don't submit when all fields are empty |
| `-f`, `--show-failed-attempts` | display failed attempt counter |
| `--accept-key=KEYNAME` | formless mode: only this key unlocks (e.g. `Return`, `space`, `F1`) |
| `--pass-media-keys` | pass media keys through in formless mode |
| `--pass-screen-keys` | pass brightness keys through |
| `--timeout=SECONDS` | auto-unlock after N seconds (testing) |
| `--max-attempts=N` | lock out after N failed attempts (0 = unlimited) |
| `--lockout-time=SECONDS` | lockout duration (default: 300) |
| `--input-timeout=SECONDS` | clear input after inactivity (0 = disabled) |
| `--clear-on-escape` | clear fields on Escape key |

### visual ###

| option | description |
| :--- | :--- |
| `--show-clock` | display current time |
| `--show-username` | display logged-in username |
| `--show-hostname` | display system hostname |
| `--no-animate` | disable animated ellipsis during verification |
| `--blink-on-error` | flash screen on auth failure |
| `--show-submit-button` | show a clickable submit button |
| `--force-submit-button` | disable Enter-to-submit (implies `--show-submit-button`) |
| `--hide-message` | hide message text |
| `--hide-hint` | hide hint text in formless mode |
| `--beep-on-error` | system beep on auth failure |
| `--beep-on-keypress` | system beep on each keypress |

### theme colors ###

All colors specified in `RRGGBB` hex format (no `#` prefix).

| option | default | description |
| :--- | :--- | :--- |
| `--color-bg` | `171421` | window background |
| `--color-fg` | `ffffff` | text color |
| `--color-frame` | `2d2d2d` | frame/entry background |
| `--color-border` | `4a4a4a` | border/outline color |
| `--color-accent` | `5294e2` | focus highlight, hints |
| `--color-error` | `ff5555` | error text |
| `--color-clock` | `55ff55` | clock text color |
| `--color-info` | `55ff55` | username/hostname text color |

### typography ###

| option | default | description |
| :--- | :--- | :--- |
| `--font-family=FONT` | JetBrains Mono (with fallbacks) | CSS font-family string |
| `--font-size=PIXELS` | `14` | base font size |
| `--entry-width=CHARS` | `25` | entry field width in characters |
| `--center-input` | off | center-align typed text inside input entries |
| `--text-alignment=ALIGN` | `center` | alignment for message/hint/status text and field groups: `center`, `left`, `right` |
| `--message-markup` | off | render `--message-text` as Pango markup (enables `<span>` color, disables auto-wrap so the caller controls line breaks) |

### frame styling ###

| option | default | description |
| :--- | :--- | :--- |
| `--frame-padding=PIXELS` | `32` | internal frame padding |
| `--frame-min-width=PIXELS` | `420` | minimum frame width |
| `--no-frame` | off | hide the frame border |

### window geometry ###

| option | description |
| :--- | :--- |
| `--position=POS` | window position: `center`, `top-left`, `top-right`, `bottom-left`, `bottom-right` (default: fullscreen) |
| `--width=PIXELS` | window width (0 = fullscreen) |
| `--height=PIXELS` | window height (0 = fullscreen) |
| `--offset-x=PERCENT` | horizontal offset from position (-100 to 100) |
| `--offset-y=PERCENT` | vertical offset from position (-100 to 100) |
| `--border=RRGGBB` | colored border around the window |
| `--border-width=PIXELS` | border thickness (default: 2) |

### security ###

| option | description |
| :--- | :--- |
| `--log-attempts` | log auth attempts to syslog (`LOG_AUTH` facility) |
| `--no-grab` | skip keyboard/pointer grab (testing only) |


## security features ##

- XCB keyboard and pointer grabs on the root window prevent input bypass
- forked child process re-raises the window if another window covers it
- field values allocated with `mlock()` and cleared with `explicit_bzero()` before freeing
- `RLIMIT_CORE` set to 0 to prevent passwords appearing in crash dumps
- X11 override-redirect bypasses the window manager

## file structure ##

``` shell
tklock/
├── src/
│   ├── main.c           # entry point, signal handling, state machine
│   ├── main.h           # shared types (LockerConfig, LockerContext, enums)
│   ├── config.c         # CLI argument parsing via GOption
│   ├── config.h         # config function declarations
│   ├── ui.c             # GTK widget creation, CSS generation, state updates
│   ├── ui.h             # UI function declarations
│   ├── auth.c           # auth command execution (sync + async via GThread)
│   ├── auth.h           # auth function declarations
│   ├── security.c       # XCB grabs, visibility monitoring, secure memory
│   └── security.h       # security types and function declarations
├── data/
│   └── lock.css         # reference style sheet
├── Makefile
└── README.md
```

## auth flow ##

``` text
IDLE ──[Enter]──> VERIFYING ──[success]──> quit
                      │
                      └──[failure]──> FAILED ──[2s]──> IDLE
```

1. user fills fields and presses Enter (or clicks Submit)
2. field values collected into `char*[]` via `secure_alloc()` (mlock'd)
3. `auth_verify_async()` spawns a GThread
4. thread runs `bash -c 'auth_cmd' _ "$1" "$2" ...` via `g_spawn_sync()`
5. callback dispatched to main thread via `g_idle_add()`
6. exit code `0` = unlock; `1`-`3` = show error, increment counter, apply lockout if needed
7. field values wiped with `secure_clear()` and freed
