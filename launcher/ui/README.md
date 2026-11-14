# Guarana Launcher UI

The web UI of the Guarana Launcher (Tauri 2): first run, game launcher, updater, doctor and problem
reports. Vite 8 + TypeScript, no UI framework. It talks to the Rust backend only through the commands
and events in [`../CONTRACT.md`](../CONTRACT.md), and runs on sample data in a plain browser.

## Run it

```sh
npm ci               # package-lock.json is committed; launcher's `ui:install` runs `npm ci` too
npm run dev          # Vite on http://localhost:5173 (the port tauri.conf.json's devUrl should use)
npm run build        # production build into dist/ (Tauri's frontendDist: ../ui/dist)
npm run typecheck    # tsc --noEmit; kept out of `build` so a type slip can't block a Tauri build
npm run preview      # serves dist/
```

Outside Tauri (no `window.__TAURI_INTERNALS__`) the UI loads `src/mock.ts` instead of
`@tauri-apps/api`, so `npm run dev` in any browser shows the full app. A built `dist/` works from any
static server too, for example `python -m http.server -d dist 8080`. Opening `index.html` straight
from disk does not work: the source is TypeScript and needs Vite.

### Sample-data scenarios

Add query parameters to the URL, for example `http://localhost:5173/?mock=ready&pad=xbox`.

| Parameter | Values | Default |
|---|---|---|
| `mock` | `first-run` (Arc 140T laptop, Ocarina of Time on disk, keys missing), `ready` (everything set up), `no-oot`, `failure` (launch, install, update check and report fail), `contaminated` (`ready`, and the doctor's settings-hygiene rows find something) | `first-run` |
| `keys`, `update`, `first` | `0` or `1`: keys imported, update available, first run | from `mock` |
| `pad` | `playstation`, `xbox`, `switch`, `generic`, `none`, `late` (connects after 3.5 s) | `playstation` |
| `crash` | `1`: the game exits with 0xC0000005 right after it starts | off |
| `warm` | `1`: an automatic shader warm-up (as after a GPU driver change) runs 2.5 s after start | off |
| `session` | seconds the game runs once it is on screen, then `game://exited` | `7` |
| `boot` | the `game://boot` events a launch plays: `session` (a 20.3+ emulator: phases, shader counts, its window taking the foreground, then the game shown after about 5 s), `old` (only its window taking the foreground), `none` (nothing: the stage hands over after 20 s) | `session` |
| `speed` | multiplies every mock delay (`0.25` is fast, `20` lets you watch each stage) | `1` |
| `screen` | `welcome`, `scan`, `profile`, `home`, `updates`, `doctor`, `report` | from `get_state` |
| `motion` | `reduce`: the reduced-motion version | system setting |

The footer shows a "Sample data" badge whenever the mock is in use.

## Structure

```
index.html            boot splash (plain markup, dark), #app, module entry
public/favicon.svg    brand favicon (copy of assets/brand/favicon.svg)
src/
  main.ts             entry: theme, motion policy, shell, input, controllers, first screen
  api.ts              the only module with command/event names; Tauri or mock backend
  mock.ts             stateful sample backend with progress events (lazy chunk, browser only)
  types.ts            CONTRACT.md types, plus Controller and the proposed Toggle bounds
  state.ts            cached reads (detect_system, list_profiles, get_state, list_games), signals
  router.ts           lazy screens, back stack, tabs, focus and announcements per navigation
  shell.ts            top bar (brand, tabs, theme), main outlet, footer, toasts, live region
  input.ts            intent layer (keyboard + pad), spatial navigation, the moving focus bracket
  gamepad.ts          Gamepad API polling, per-pad layouts, button glyphs, rumble
  motion.ts           reduced motion, view transitions, pause switch, idle scheduling
  theme.ts            dark by default, light (paper chart) on request, scan wipe
  markdown.ts         minimal safe Markdown -> DOM (no innerHTML of data)
  sessions.ts         last-played times and last applied profile (localStorage)
  format.ts, dom.ts, icons.ts
  ui/                 controls, instrument (odometer, pacing strip, readout), toast, progress ring, checks, controller card, library list
  screens/            welcome, scan, profile, home, launch, updates, doctor, report (one chunk each)
  styles/             tokens.css, base.css, components.css, screens.css, intro.css (app.css imports them)
```

## Screens and the contract

| Screen | Calls | Listens |
|---|---|---|
| Boot | `get_state` (first run decides Welcome or Home), Tauri `getVersion()` for the footer | `install://progress` while `get_state` installs the bundled build, `game://exited` (safety net), `controller://changed`, `warm://progress` |
| Welcome | nothing; preloads Scan | |
| Scan | `detect_system`, `list_profiles`, `locate_files {scope:"quick"}` (or `"deep"`), `run_doctor`, `import_keys`, `add_game_folder`, `pick_folder`, `set_mode` (setup ends with Auto, no profile step), `apply_fix`, `open_path`, `list_controllers`; preloads Home and Profile | `locate://progress`, `doctor://progress`, `controller://changed` |
| Profile (under Advanced) | `list_profiles`, `apply_profile {profile_id, overrides}` (only values that differ from `Toggle.default`; sets the mode to custom) | |
| Home | `get_mode`, `set_mode` (the Auto / Smooth / Quality / Battery switch), `autopilot_undo`, `autopilot_enable`, `get_booster`, `set_booster_level`, `booster_undo_all` (the Game Booster card), `stop_warm_shaders`, `get_state`, `list_games`, `list_profiles`, `prefetch_caches {title_id}` once per session on idle, `warm_shaders {title_id}` ("Prepare shaders"), `check_updates` once per session | `autopilot://changed`, `booster://changed`, `warm://progress` (Play waits while the title's shaders build, also for the automatic run after a driver change) |
| Launch | `launch_game {title_id, profile_id}` | `game://exited` and `game://boot` (both registered before launching; the stage shows the boot phase and the real shader count, and holds until the game is on screen, see CONTRACT.md "Boot progress and the hand-over") |
| Updates | `check_updates {channel}`, `install_version`, `set_current_version` | `install://progress` |
| Doctor | `run_doctor`, `apply_fix`, `ask_ai` (the `install_update` fix shows its `install://progress` on the button) | `doctor://progress`, `install://progress` |
| Report | `ask_ai`, `collect_report {kind, note}`, `open_path`, `open_url` | |

Home has one ember button, Play. The hero is an instrument: the game's title set enormous, the
frames-on-time numeral (the share offer's `within2`, kept per game in `localStorage` by
`sessions.ts` so it survives Share and Not now), and the frame-pacing strip, drawn from the last
session's p50, p95 and p99 and labelled as modelled from them. With no measured session yet, the
strip shows the target cadence and the numeral a dash. Below come the share notice, the mode
(02), the library (03) as a table of record, then "This PC" (04): the Game Booster as a folded
panel that shows its current level, and the controllers and setup panels. First run is Scan and
then Home in Auto mode, so the top bar says "Set up" without a step count; Scan's Continue unlocks
once the device and the files are known, and Health finishes in the background. The launch stage
is a countdown instrument: the title and the strip carry over from the hero, the measuring grid
sweeps in, the five boot phases from `game://boot` are segments that lock into place, the real
shader count rolls on an odometer while shaders load, and a T+ clock runs until the game has the
GPU. It holds until the game is on screen, then the instrument collapses to one line that opens
like a shutter, and the stage shows the game's title followed by "is running" on black; when it
hands over after a hold time without news, the launcher knows only that the process started, not
that the game has drawn a frame.

Every listener is registered before the command that streams, and removed when its screen goes.
Links never navigate the webview: `https://` links go through `open_url`, nothing else opens.

Argument keys are sent in snake_case, exactly as CONTRACT.md writes them (the Rust commands use
`rename_all = "snake_case"`; `CAMEL_CASE_ARGS` in `src/api.ts` flips it if that ever changes).
Rejections are shown as the sentence the backend sends. The Stable/Nightly switch passes
`{ channel }` to `check_updates`.

`list_profiles` returns the applied profile with the user's saved values merged in, and
`apply_profile` replaces the saved overrides, so Profile's Apply sends only the toggles that differ
from `Toggle.default` and "Restore defaults" applies the profile without overrides. On a first
start `get_state` waits for the install of the bundled build; its `install://progress` shows under
the boot mark.

The mock mirrors the backend: the same profiles and toggles as `src-tauri/src/profiles.rs`, the
contract's check and fix ids in order, and the stage names of every progress event.

## Design tokens: "a precision instrument"

All in `src/styles/tokens.css`. Measurement is the aesthetic: pure black, one ember accent,
hairlines, huge numerals, flat sharp controls. The instrument parts (odometer, pacing strip,
readout) are in `src/ui/instrument.ts`.

| Token | Dark (default) | Light ("paper chart") | Use |
|---|---|---|---|
| `--ember` | `#FF9A2E` | same | the one accent fill: Play, primary buttons, selected rails, late frames |
| `--accent` | `--ember` | `#9A3F08` | ember as text and focus (4.5:1 on its background) |
| `--bg`, `--surface` | `#000` | `#F3F2EE` | page and panels (panels are hairline frames, not fills) |
| `--line` / `-2` / `-3` | white at 13 / 24 / 46 % | black at 14 / 28 / 50 % | hairlines |
| `--grid`, `--grid-mark` | white at 5 / 16 % | black at 5 / 18 % | the 96 px measuring grid and its plus marks (static) |
| `--text` / `--text-2` / `--muted` | `#F4F4F0` / `#BFBFB8` / `#8F8F88` | `#0B0B0A` / `#383835` / `#5A5A55` | text, all at least 4.5:1 |
| `--ok` `--warn` `--fail` `--info` | soft green, gold, coral, grey | darker shades | status, always with an icon and a word |

The night stages (Welcome and Launch) stay black in both themes.

- Type: system fonts only. Display (`Segoe UI Variable Display`) for titles at `--fs-title` and thin numerals at `--fs-numeral`; mono (`Cascadia Mono`) uppercase and tracked for every label, unit and index (`01 /`).
- Space: 4 px grid; `--module` 96 px for the background grid; Home has a 112 px index rail. Radius: 0 everywhere (the `--r-*` names stay and resolve to 0).
- Motion: fast, mechanical and exact. `--dur-1/2/3` are 120/160/200 ms, `--ease-mech` has no overshoot (`--ease-spring` and `--ease-bounce` resolve to it), `--stagger` 28 ms. Sequences are choreographed, never springy.

## Motion and performance rules

- Everything that moves animates `transform` and `opacity`: the grid sweeps, segment fills, odometer reels, the strip's reveal cover and playhead, the focus bracket's corners, screen transitions. The exceptions are one-off and small: the intro's drop outline, streak cuts and echo arcs (stroke-dashoffset), the segmented rail's width, and the clip-path of the return transition and the theme wipe on their snapshots. No `filter`, no `backdrop-filter`.
- Nothing decorative loops while the player is only looking. The strip's playhead steps at 12 fps for two passes, then rests, and is `.ambient`. Spinners are a ticking clock hand (12 steps a second); the active boot segment scans at 30 steps a second; both run only while something is loading.
- One pause switch: while a game runs or the window is hidden, `html[data-paused]` stops every CSS animation, the launch instrument's transitions and its T+ clock, and the pad loop; while the window is unfocused, ambient loops (`.ambient`) and pad polling stop.
- Gamepad polling runs only with a pad connected and the window focused, and is suspended from `launch_game` until `game://exited`, so in-game presses never reach the launcher.
- Screens are separate chunks loaded with `import()`; Home is preloaded while Scan runs; the library list and the Markdown renderer load when needed; the mock is its own chunk.
- Screen changes use the View Transitions API, as a camera on rails: the old screen leaves in 140 ms, the new one tracks in over 200 ms from the same direction. Play morphs the hero's title and pacing strip into the launch instrument; a return from a game opens Home like a shutter; the theme switch is a scan wipe from the top; the intro's mark flies into the top bar's mark when you press Set up.
- Reduced motion (the system setting or `?motion=reduce`): the intro shows its final frame, numbers are set without rolling, transitions are instant, the bracket jumps, nothing loops.

## Controllers and keyboard

Keyboard and pads feed one intent layer (`src/input.ts`); focus moves by geometry, and a single focus
bracket follows it: four corners that travel to the next control a little wide and clamp onto it
(160 ms, then 90 ms), and bump against the edge when there is nowhere to go. A held direction
scrolls instantly instead of queueing smooth scrolls. The bracket shows for keyboard and pad use,
never for the mouse; under Windows contrast themes a real outline replaces it. A folded card's heading
(a `<details>` summary, such as Game Booster or Advanced) is in the focus order, and Select opens or
closes it.

| Intent | Keyboard | Xbox | PlayStation | Switch Pro | Generic |
|---|---|---|---|---|---|
| Move | arrows | D-pad / left stick | D-pad / left stick | D-pad / left stick | D-pad / left stick |
| Select | Enter / Space | A | Cross | A (right button) | bottom button |
| Back | Esc | B | Circle | B (bottom button) | right button |
| Switch tabs | Ctrl+Tab / Ctrl+Shift+Tab | LB / RB | L1 / R1 | L / R | L / R |
| Screen's main action | | Menu | Options | + | Start |
| Scroll | | right stick | right stick | right stick | right stick |

The Gamepad API's standard mapping is positional (button 0 is the bottom face button), so Switch
layouts select with the right button, where A sits. Glyphs follow the pad used last (else the first
pad `list_controllers` reports) and are drawn here: letters or shapes in circles, no platform logos.

## Content rules

- No Nintendo artwork, logos or names in imagery. The art is procedural (grid, hairlines, the drop and its echo arcs); game titles appear only as text from the user's own files.
- The pacing strip never poses as a recording: its caption says it is modelled from the session's percentiles, and with no session it shows the target cadence.
- Numbers come from the site: "Measured on an Arc 140T: Hyrule Field 14.75 → 22.83 game fps, frame generation off".
- The Files card always carries: "Guarana never downloads keys, firmware or games. Use your own, dumped from your console." Nothing is imported until the user presses Import or Add folder.
- Diagnostics copied from Doctor are scrubbed (`C:\Users\<name>` becomes `%USERPROFILE%`); reports are built and scrubbed by the backend and only leave the PC when the user submits the GitHub issue.
