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
  input.ts            intent layer (keyboard + pad), spatial navigation, moving focus halo
  gamepad.ts          Gamepad API polling, per-pad layouts, button glyphs, rumble
  motion.ts           reduced motion, view transitions, pause switch, idle scheduling
  light.ts            the sky: a small aurora canvas, its lean toward the focus, the glass edges
  theme.ts            Night by default, Dawn (light) on request, circular reveal
  markdown.ts         minimal safe Markdown -> DOM (no innerHTML of data)
  sessions.ts         last-played times and last applied profile (localStorage)
  format.ts, dom.ts, icons.ts
  ui/                 controls, toast, progress ring, checks, controller card, key art, library list
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

Home opens on a title card set straight on the sky: the game's title large and light, a credit line
of the profile's highlights, and one control made of light, Play; the key art floats to the right
as a sculpture of light (the drop, its streaks as light trails, the echo arcs as rings). Below it
come the share prompt when offered (a secondary Share button), the mode card (when keys are
imported), the library, then "This PC": the Game Booster as a folded card that shows its current
level, and the controllers and setup cards, all on glass. First run is Scan and then Home in Auto
mode, so the top bar says "Set up" without a step count; Scan's Continue unlocks once the device
and the files are known, and Health finishes in the background. Pressing Play sends the art, the
title and Play itself into the launch stage, where Play stretches into a horizon of light. The
stage shows the title and one status line with a small comet: the boot phase from `game://boot` in
words, with the real shader count after it while shaders load, and the horizon rises with the
phases (each phase sets a floor; between events it creeps toward the next one, never past what the
boot has said). It holds until the game is on screen; when it hands over in view, light floods the
window white-gold and falls away to black with the game's title followed by "is running"; when it
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

## Design: light as material

The window is a night sky with a slow aurora of ember and violet light (`src/light.ts`). Panels are
thin sheets of glass that catch that light at their edges. The one action colour is light itself,
white-gold with ember at its foot. Focus is a travelling spotlight, and the sky leans toward it.
Titles are large and light, labels small and widely tracked, like a film's title cards. Motion is
slow, weightless and continuous: no overshoot anywhere, nothing snaps.

All tokens are in `src/styles/tokens.css`. Brand values come from `assets/brand`.

| Token | Night (default) | Dawn (`data-theme="light"`) | Use |
|---|---|---|---|
| `--light` / `-2` / `-3`, `--grad-light` | `#FFF4DE` / `#FFD99A` / `#FFB15A` | same, deeper gradient | the primary action (Play, Continue, Set up), selection, the focus spotlight, the horizon |
| `--ember-1` / `--ember-2`, `--leaf`, `--echo` | `#FFC94A` / `#F2851A`, `#3CCF7E`, `#F59E0B` | same | the drop, its leaf and echo arcs |
| `--bg` | `#05050B` | `#F6F0E9` | under the sky canvas |
| `--glass` / `--glass-2` | indigo at 58 / 62 % | white at 60 / 74 % | panels over the sky (`.card`, `.glass`) |
| `--rim`, `--rim-ember`, `--rim-violet` | hairline, and the two lights an edge can catch | darker hairline | panel edges |
| `--surface` / `-2` / `-3` | `#121027` / `#19163A` / `#241F4A` | `#FFFDFA` / `#F7F2EC` / `#ECE4DB` | opaque things: fields, code, toasts |
| `--text` / `--text-2` / `--muted` | `#F6F1EA` / `#D2CBDC` / `#B0A9C2` | `#1A1624` / `#433C52` / `#5B546A` | text |
| `--accent` | `#FFD08F` | `#8C3C02` | links, kickers |
| `--halo`, `--halo-glow` | `#FFE8C2`, warm glow | `#8C3C02`, ember glow | the focus spotlight |
| `--ok` `--warn` `--fail` `--info` | light tints | deep shades | status, always with an icon and a word |

Text sits straight on the sky (the hero, screen heads, the footer), so `light.ts` caps the night sky
at 0.04 relative luminance and floors the dawn sky at 0.66, per pixel of its small canvas. Measured on
the brightest point of the night sky: text 10.4:1, text-2 7.4:1, muted 5.2:1; on the darkest point
of dawn: text 12:1, muted 4.9:1, the status shades 4.8:1 or more. The intro and the launch stage
stay night in both themes.

- Type: system fonts only (`Segoe UI Variable` on Windows 11). Titles use the display cut at weight
  300 (`--fw-title`) and `--fs-title` (up to 4.4 rem); captions, tabs, labels and badges are
  small capitals tracked at `--track-caps` (0.26 em). Body text stays at 400.
- Space: 4 px grid (`--sp-1` 4 px … `--sp-16` 64 px). Radius: `--r-sm` 8, `--r-md` 12, `--r-btn` 14,
  `--r-lg` 20, `--r-xl` 28. Only switches are pills.
- Motion: `--dur-1/2/3` 220/320/460 ms, `--dur-screen` 640 ms, `--dur-hero` 1100 ms. `--ease-out`
  is a long weightless glide; `--ease-spring` and `--ease-bounce` keep their names but no longer
  overshoot; `--ease-drift` for ambient movement.
- Glass edges: every `.card` and `.glass` has two rings of light on its edge (`::before` ember,
  `::after` violet, a masked 1 px ring). `light.ts` sets their strength (`--lit-a`, `--lit-b`) from
  how close the ember and violet light are, twice a second, quantised to tenths, so an edge repaints
  only when the light has really moved; the change itself is a 1.8 s opacity transition. A panel that
  holds the focus catches full light.

## Motion and performance rules

- The sky is a 160 px wide canvas the browser stretches over the window; a frame is a redraw of
  about 16 000 pixels on the CPU and one small texture upload, never a full-window paint. It runs at
  most 24 frames a second, and not at all while a game runs, the window is hidden or unfocused, a
  view transition plays, under reduced motion (one still frame), or after 45 s without input (it
  settles; the next input wakes it). A static noise tile on `#app`, painted once, keeps the stretched
  gradients from banding.
- Everything else that moves animates `transform` and `opacity`: screen transitions, the hero's
  arrival, the echo ripples, Play's sheen, the horizon, the flood, the intro's spark, line, trails,
  letters and drop. Small one-off transitions also touch other properties (the focus halo's size,
  the segmented thumb's width, the progress ring's stroke, button shadows). The intro paints two small
  things once (the drop's path, the streak masks). Real blur (`backdrop-filter`) is used only on
  small panels: toasts and the Help menu.
- Nothing decorative loops while the player is only looking. On Home the echo arcs ripple three
  times and the drop's core breathes twice, Play's sheen crosses twice, then everything rests. The
  launch stage's art holds still: the horizon and the small comet by the status line are what move,
  and the horizon jumps instead of gliding while `data-paused` is set. Spinners, skeletons and
  progress bars run only while something is loading.
- One pause switch: while a game runs or the window is hidden, `html[data-paused]` stops every CSS
  animation, the sky and the pad loop; while the window is unfocused, the sky, ambient loops
  (`.ambient`) and pad polling stop.
- Gamepad polling runs only with a pad connected and the window focused, and is suspended from
  `launch_game` until `game://exited`, so in-game presses never reach the launcher.
- Screens are separate chunks loaded with `import()`; Home is preloaded while Scan runs; the library
  list and the Markdown renderer load when needed; the mock is its own chunk.
- Screen changes use the View Transitions API: the old view lifts away and the new one settles in
  (forward and back), a drift for tabs. Play sends the hero's art, its title and Play itself into the
  full-window launch stage (`hero-art`, `hero-title`, `play-light`; from a library row only the
  title travels). The stage follows the boot until the game is on screen; the flood plays only when
  it hands over without the emulator's window having taken the foreground (otherwise it would play
  unseen behind the game), and its animations pause as soon as that window is in front. The intro's
  logo flies into the top bar's mark when you press Set up. The theme switch spreads from its button.
- Reduced motion (the system setting or `?motion=reduce`): the sky is one still frame, the intro
  shows its final frame, transitions are instant, the horizon jumps, the hand-over skips the flood,
  nothing loops.

To watch the launch sequence in the browser: `?mock=ready&boot=none` shows the horizon creeping and,
after 20 s, the flood and the fall to black; `?mock=ready&speed=4` slows the boot phases down (the
stage holds still once the emulator's window takes the foreground, as it does for real).

## Controllers and keyboard

Keyboard and pads feed one intent layer (`src/input.ts`); focus moves by geometry, and a single halo
follows it. The halo shows for keyboard and pad use, never for the mouse. A folded card's heading
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

- No Nintendo artwork, logos or names in imagery. The key art is brand-abstract (the drop, its streaks and echo arcs); game titles appear only as text from the user's own files.
- Numbers come from the site: "Measured on an Arc 140T: Hyrule Field 14.75 → 22.83 game fps, frame generation off".
- The Files card always carries: "Guarana never downloads keys, firmware or games. Use your own, dumped from your console." Nothing is imported until the user presses Import or Add folder.
- Diagnostics copied from Doctor are scrubbed (`C:\Users\<name>` becomes `%USERPROFILE%`); reports are built and scrubbed by the backend and only leave the PC when the user submits the GitHub issue.
