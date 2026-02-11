# Porting DOOM to BonfireOS

BonfireOS provides a **DOOM host API** so that a DOOM port (e.g. from [id-Software/DOOM](https://github.com/id-Software/DOOM), linuxdoom-1.10) can run when the user types `DOOM` at the shell. The OS does **not** include any DOOM source code; you add the port in a separate module and link it with the kernel.

## Host API (what BonfireOS provides)

All declarations are in `include/kernel/doom_host.h`. The port must **only** use these and standard C; no direct hardware or kernel internals.

| Area | Functions | Notes |
|------|------------|--------|
| **Video** | `doom_video_enter`, `doom_video_framebuffer`, `doom_video_set_palette`, `doom_video_leave` | VGA mode 13h: 320×200, 256 colors, linear buffer at 0xA0000 |
| **Input** | `doom_input_get_key`, `doom_input_mouse`, `doom_input_clear` | Scancode + up/down; mouse dx/dy and buttons |
| **Time** | `doom_time_ms`, `doom_time_delay_ms` | Millisecond tick and delay |
| **Memory** | `doom_malloc`, `doom_free`, `doom_realloc` | 2 MiB heap for DOOM |
| **File** | `doom_open`, `doom_read`, `doom_write`, `doom_close`, `doom_lseek` | POSIX-style; use for WAD and config |

## Entry point

- The port must define **`doom_main(int argc, char **argv)`** (strong symbol).
- The kernel calls it when the user runs the `DOOM` command; `argc == 1`, `argv[0] == "DOOM"`.
- On return, the kernel switches back to text mode. Return `0` on success, non-zero on error.

## Flow when the user types DOOM

1. Shell runs `cmd_doom`: enters mode 13h, inits mouse, clears input queues.
2. Shell calls `doom_main(1, argv)`.
3. Your port uses the host API to render, read input, load WAD (via `doom_open`/`doom_read`/`doom_lseek`), and run the game loop.
4. When the game exits, return from `doom_main`.
5. Kernel leaves graphics mode and prints the prompt again.

## What you must not do

- Do **not** copy or add the id-Software DOOM repository into BonfireOS. Keep the port in your own tree or repo and link it into the kernel build.
- Do **not** rely on Linux, X11, or SDL. Use only `doom_*` and the C library subset that BonfireOS provides (no libc beyond the host API and existing POSIX-style file I/O).

## Build integration

- Add your DOOM port sources to the kernel build (e.g. in the Makefile or CMake).
- Your code includes `kernel/doom_host.h` and implements `doom_main`.
- The kernel’s weak `doom_main` stub is overridden by your strong symbol. If you do not link a port, typing `DOOM` prints: “DOOM not available (link a DOOM port to provide doom_main).”

## Constants (doom_host.h)

- `DOOM_SCREEN_WIDTH` = 320, `DOOM_SCREEN_HEIGHT` = 200, `DOOM_SCREEN_BPP` = 8.
- Palette: 256 entries × 3 bytes (R, G, B in 0–63) via `doom_video_set_palette`.

BonfireOS remains a **CLI OS**; graphics mode is used only while the DOOM process is running.
