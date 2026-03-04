# Porting Command & Conquer: Red Alert to BonfireOS

BonfireOS provides a **Red Alert host API** so that a port of [Command & Conquer: Red Alert](https://github.com/electronicarts/CnC_Red_Alert) can run when the user types `REDALERT` at the shell. The OS does **not** include any Red Alert source code; you add the port in a separate module and link it with the kernel.

## Original dependencies (EA source)

The EA-released source expects:

- **DirectX 5** — graphics
- **DirectX Media 5.1** — video playback
- **Greenleaf Communications Library (GCL)** — IPX networking
- **Human Machine Interface "Sound Operating System" (SOS)** — audio

BonfireOS replaces these with the Red Alert host API below. Video uses VGA mode 13h (320×200×256); audio and network are **stub** implementations (no-op) until drivers exist.

## Host API (what BonfireOS provides)

All declarations are in `include/kernel/redalert_host.h`. The port must **only** use these and standard C; no direct hardware or kernel internals.

| Area | Functions | Notes |
|------|------------|--------|
| **Video** | `redalert_video_enter`, `redalert_video_framebuffer`, `redalert_video_set_palette`, `redalert_video_leave` | VGA mode 13h: 320×200, 256 colors, linear buffer |
| **Input** | `redalert_input_get_key`, `redalert_input_mouse`, `redalert_input_clear` | Scancode + up/down; mouse dx/dy and buttons |
| **Time** | `redalert_time_ms`, `redalert_time_delay_ms` | Millisecond tick and delay |
| **Memory** | `redalert_malloc`, `redalert_free`, `redalert_realloc` | 4 MiB heap for Red Alert (MIX, maps, etc.) |
| **File** | `redalert_open`, `redalert_read`, `redalert_write`, `redalert_close`, `redalert_lseek` | POSIX-style; for MIX files, INI, save games |
| **Audio** | `redalert_audio_init`, `redalert_audio_play`, `redalert_audio_stop`, `redalert_audio_stop_all`, `redalert_audio_shutdown` | **Stub**: no sound until a driver is added |
| **Network** | `redalert_net_init`, `redalert_net_broadcast`, `redalert_net_send`, `redalert_net_receive`, `redalert_net_peer_count`, `redalert_net_get_peer_address`, `redalert_net_shutdown` | **Stub**: no IPX/NIC until networking is added |

## Entry point

- The port must define **`redalert_main(int argc, char **argv)`** (strong symbol).
- The kernel calls it when the user runs the `REDALERT` command; `argc == 1`, `argv[0] == "REDALERT"`.
- On return, the kernel switches back to text mode. Return `0` on success, non-zero on error.

## Flow when the user types REDALERT

1. Shell runs `cmd_redalert`: enters mode 13h, inits mouse, clears input queues.
2. Shell calls `redalert_main(1, argv)`.
3. Your port uses the host API to render, read input, load data (e.g. MIX via `redalert_open`/`redalert_read`/`redalert_lseek`), and run the game loop. Audio and network calls are safe but currently no-ops.
4. When the game exits, return from `redalert_main`.
5. Kernel leaves graphics mode and prints the prompt again.

## What you must not do

- Do **not** copy the EA CnC_Red_Alert repository into BonfireOS. Keep the port in your own tree or repo and link it into the kernel build.
- Do **not** rely on Windows, DirectX, Watcom, or SDL. Use only `redalert_*` and the C subset that BonfireOS provides.

## Build integration

- Add your Red Alert port sources to the kernel build (e.g. in the Makefile).
- Your code includes `kernel/redalert_host.h` and implements `redalert_main`.
- The kernel’s weak `redalert_main` stub is overridden by your strong symbol. If you do not link a port, typing `REDALERT` prints: “Red Alert not available (link a Red Alert port to provide redalert_main).”

## Constants (redalert_host.h)

- `REDALERT_SCREEN_WIDTH` = 320, `REDALERT_SCREEN_HEIGHT` = 200, `REDALERT_SCREEN_BPP` = 8.
- Palette: 256 entries × 3 bytes (R, G, B in 0–63) via `redalert_video_set_palette`.
- `REDALERT_AUDIO_MAX_VOICES` = 8, `REDALERT_AUDIO_SAMPLE_RATE` = 22050.
- `REDALERT_NET_MAX_PEERS` = 8, `REDALERT_NET_PACKET_MAX` = 512.

## File I/O and game data

Red Alert typically reads from MIX archives and INI. Use `redalert_open`/`redalert_read`/`redalert_lseek`/`redalert_close`. Currently these map to the in-memory FS; to load from disk you will need to extend the POSIX layer to open files from a FAT volume (or add a dedicated API that uses the existing `fat_*` functions). See the DOOM port doc for the same consideration (WAD on disk).

## Future work

- **Audio**: Implement a PCM driver (e.g. PC speaker or a sound card driver) and wire `redalert_audio_*` to it.
- **Network**: Add an NIC driver and an IPX-like or UDP-based protocol; implement `redalert_net_*` so multiplayer works.
- **Video**: If the port needs a higher resolution, add another VGA (or VBE) mode and expose it via the host API.

BonfireOS remains a **CLI OS**; graphics mode is used only while the Red Alert process is running.
