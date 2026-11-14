// Launch stage: the hero's art, title and Play have travelled into the full window, Play becoming a
// horizon of light. launch_game runs, and the stage follows the boot (game://boot: the phase in
// words and the real shader count, on its one status line) while the horizon rises, until the game
// is on screen; then light floods the window and falls away to "running". game://exited brings us back.

import { api, errorText, events } from "../api";
import { h } from "../dom";
import { exitCode } from "../format";
import { setPadSuspended } from "../gamepad";
import { focusFirst } from "../input";
import { pinLight, setSkyLevel } from "../light";
import { isPaused, reducedMotion, setPaused, wait } from "../motion";
import { go, type ScreenDef } from "../router";
import { recordSession } from "../sessions";
import type { BootProgress, Game, GameExited } from "../types";
import { icon } from "../icons";
import { button, spinner } from "../ui/controls";
import { heroArt } from "../ui/hero-art";

type Phase = "starting" | "handoff" | "playing" | "failed" | "done";

// How long the stage holds without news before it hands over anyway (CONTRACT.md, "Boot progress
// and the hand-over").
/** Nothing heard at all: an emulator that writes no boot and whose window never took the foreground. */
const HOLD_QUIET_MS = 20_000;
/** The same for an emulator that writes its boot (follows_boot), before its window has the foreground:
 *  its first line comes after its own start-up, which a cold first start (a virus scan of the new
 *  build) can make long. */
const HOLD_QUIET_SESSION_MS = 45_000;
/** Its window has the foreground and it writes no boot (a version before the launcher session). */
const HOLD_WINDOW_ONLY_MS = 1_000;
/** Its window has the foreground and the boot goes on behind it: the next line resets this. */
const HOLD_BEHIND_MS = 60_000;
/** The first frame is presented; the emulator's loading screen fades out over 0.5 s after it. */
const HOLD_FIRST_FRAME_MS = 1_000;

const count = (n: number) => n.toLocaleString();

/** The boot phase in plain words; null keeps the current line. */
function bootText(b: BootProgress): string | null {
  switch (b.phase) {
    case "starting":
      return "Starting Guarana…";
    case "loading":
      return "Loading the game…";
    case "shaders":
      return "Loading shaders…";
    case "precache":
      return "Preparing shaders ahead…";
    case "launching":
    case "first-frame":
    case "shown":
      return "Starting the game…";
    default:
      return null;
  }
}

const screen: ScreenDef = {
  title: "Starting",
  chrome: "none",

  render(ctx) {
    const game = ctx.params.game as Game | undefined;
    const profileId = (ctx.params.profileId as string | null | undefined) ?? undefined;
    // A file opened from Explorer or a guarana:// link: started by its path, which may be outside the library.
    const byPath = ctx.params.byPath === true;
    const art = heroArt({ cls: "launch-art" });
    const title = h("h1", { class: "launch-title", text: game?.title ?? "Game" });
    // From the hero both morph; from a library row only the title does (that page has no hero art).
    if (ctx.params.morph === true) art.style.setProperty("view-transition-name", "hero-art");
    if (ctx.params.morph === true || ctx.params.morph === "title") title.style.setProperty("view-transition-name", "hero-title");
    // One line of what is happening: the boot phase in words (game://boot), and while shaders load
    // their real count after it. A small spinner says the line is live; the text changes when the
    // phase does. The count changes every percent, so it sits outside the live region: screen
    // readers hear the phases, not every number.
    const statusText = h("span", { role: "status", text: "Starting Guarana…" });
    const countText = h("span", { class: "launch-count" });
    const status = h("p", { class: "launch-status" }, spinner("launch-spin"), statusText, countText);
    const actions = h("div", { class: "launch-actions" });
    const blackout = h("div", { class: "blackout", "aria-hidden": "true" });
    const eyebrow = h("p", { class: "eyebrow launch-eyebrow", text: "Starting" });
    const body = h("div", { class: "launch-body" }, eyebrow, title, status, actions);

    // The horizon: a line of light across the window that rises as the boot goes on (transform only,
    // on its own layer). Play's slab of light becomes it when the stage opens from the hero.
    const band = h("div", { class: "horizon-band" }, h("span", { class: "horizon-line" }));
    const horizon = h("div", { class: "horizon", "aria-hidden": "true" }, h("div", { class: "horizon-glow" }), band, h("div", { class: "horizon-fill" }));
    if (ctx.params.morph === true) band.style.setProperty("view-transition-name", "play-light");
    // When the game is ready, light floods the window white-gold and falls away to black.
    const flood = h("div", { class: "flood", "aria-hidden": "true" });
    const el = h("div", { class: "launch is-starting" }, horizon, art, body, flood, blackout);

    let phase: Phase = "starting";
    let enteredResolve: () => void = () => undefined;
    const entered = new Promise<void>((resolve) => {
      enteredResolve = () => resolve();
    });
    ctx.onEntered(() => {
      art.style.removeProperty("view-transition-name");
      title.style.removeProperty("view-transition-name");
      band.style.removeProperty("view-transition-name");
      enteredResolve();
    });
    // Back and tab switching do nothing while the game starts or runs.
    ctx.onIntent((intent) => phase !== "failed" && (intent === "back" || intent === "tabPrev" || intent === "tabNext" || intent === "primary"));
    ctx.onLeave(() => {
      setPadSuspended(false);
      setPaused("game", false);
      clearInterval(creepTimer);
      pinLight(null);
      setSkyLevel(1, 900);
    });

    // ---- Rise: 0 is the horizon low at the foot of the window, 1 is the game ready.
    // Each boot phase sets a floor; between events the light creeps toward the next floor, so it
    // never stands still and never claims more than the boot has said.
    let rise = 0;
    let ceiling = 0.16;
    const setRise = (p: number, next = ceiling) => {
      rise = Math.max(rise, Math.min(1, p));
      ceiling = Math.max(rise, next);
      horizon.style.transform = `translateY(${((0.86 - 0.56 * rise) * 100).toFixed(2)}%)`;
      pinLight({ x: 0.5, y: 0.86 - 0.56 * rise, w: 0.8 });
    };
    setRise(0.04);
    setSkyLevel(0.7, 1400);
    const creepTimer = setInterval(() => {
      if (phase !== "starting" || isPaused() || reducedMotion()) return;
      const gap = ceiling - rise;
      if (gap > 0.004) setRise(rise + gap * 0.12);
    }, 1600);
    const FLOORS: Partial<Record<BootProgress["phase"], [number, number]>> = {
      starting: [0.06, 0.16],
      loading: [0.18, 0.27],
      launching: [0.84, 0.91],
      "first-frame": [0.93, 0.97],
      shown: [1, 1],
    };
    const riseFor = (b: BootProgress) => {
      if ((b.phase === "shaders" || b.phase === "precache") && b.total > 0) {
        const f = Math.min(1, b.done / b.total);
        setRise(0.28 + 0.52 * f, 0.28 + 0.52 * Math.min(1, f + 0.08));
        return;
      }
      if (b.phase === "shaders" || b.phase === "precache") {
        setRise(0.28, 0.5);
        return;
      }
      const floor = FLOORS[b.phase];
      if (floor) setRise(floor[0], floor[1]);
    };

    // game://boot: the stage holds until the game is on screen, and hands over anyway after the
    // HOLD_* times without news, as it did before the emulator reported its boot.
    let launched = false;
    let followsBoot = false; // launch_game: this emulator writes its boot
    let sawBoot = false; // the emulator writes its boot (a launcher session)
    let sawWindow = false; // a window of the emulator has had the foreground
    let lastBoot: BootProgress["phase"] | null = null;
    let holdTimer: ReturnType<typeof setTimeout> | undefined;
    ctx.signal.addEventListener("abort", () => clearTimeout(holdTimer));

    const showBoot = (b: BootProgress) => {
      const text = bootText(b);
      if (text) statusText.textContent = text;
      const counted = (b.phase === "shaders" || b.phase === "precache") && b.total > 0;
      countText.textContent = counted ? `${count(b.done)} of ${count(b.total)}` : "";
      riseFor(b);
    };

    const fail = (message: string) => {
      clearTimeout(holdTimer);
      countText.textContent = "";
      phase = "failed";
      clearInterval(creepTimer);
      setPaused("game", false);
      setPadSuspended(false);
      el.classList.remove("is-starting", "is-handoff", "is-playing");
      el.classList.add("is-failed");
      eyebrow.replaceChildren(icon("fail"), "Didn't start");
      statusText.textContent = message;
      actions.replaceChildren(
        button("Back to Home", { kind: "primary", size: "lg", primary: true, autofocus: true, onClick: () => void go("home", { transition: "back", reset: true }) }),
        button("Report a problem", { kind: "secondary", size: "lg", icon: "flag", onClick: () => void go("report", { params: { kind: "crash", note: message } }) }),
      );
      focusFirst(actions);
    };

    const onExit = (e: GameExited) => {
      if (!game || phase === "done" || phase === "failed") return;
      clearTimeout(holdTimer);
      const early = phase === "starting" || phase === "handoff" || e.seconds < 10;
      setPaused("game", false);
      setPadSuspended(false);
      if (game.title_id) recordSession(game.title_id, e.seconds);
      if (e.code !== 0 && early) {
        fail(`${game.title} closed while starting (exit code ${exitCode(e.code)}). The log is in the report.`);
        return;
      }
      phase = "done";
      void go("home", { transition: "return", reset: true, params: { exited: { code: e.code, seconds: e.seconds, title: game.title } } });
    };

    if (!game) {
      setTimeout(() => fail("Nothing to start."), 0);
      return el;
    }

    const handOver = async () => {
      if (ctx.signal.aborted || phase !== "starting") return;
      clearTimeout(holdTimer);
      // Behind the game's window the dimming would play unseen: straight to "running" then.
      // In front of it, the horizon reaches the top, the light floods the window white-gold and
      // falls away to black (1.2 s, transform and opacity only).
      if (!sawWindow) {
        phase = "handoff";
        statusText.textContent = "Handing over to the game window…";
        countText.textContent = "";
        setRise(1);
        el.classList.replace("is-starting", "is-handoff");
        await wait(reducedMotion() ? 0 : 1200);
        if (phase !== "handoff") return;
      }
      phase = "playing";
      el.classList.remove("is-starting", "is-handoff");
      el.classList.add("is-playing");
      // "Running", not "Playing": after a HOLD_* time without news the launcher knows only that the
      // process started, not that a frame is up.
      const playing = h(
        "div",
        { class: "playing" },
        h("p", { class: "playing-title", text: `${game.title} is running` }),
        h("p", { class: "playing-hint", text: "It opens in its own window. Close the game to come back here." }),
      );
      body.replaceChildren(playing);
      requestAnimationFrame(() => requestAnimationFrame(() => playing.classList.add("is-shown")));
      // Nothing moves in the launcher while the game runs: it shares the GPU.
      setPaused("game", true);
    };

    // The next hand-over deadline, from what the boot has told so far.
    const schedule = () => {
      if (!launched || phase !== "starting") return;
      clearTimeout(holdTimer);
      const ms =
        lastBoot === "shown"
          ? 0
          : lastBoot === "first-frame"
            ? HOLD_FIRST_FRAME_MS
            : sawWindow
              ? sawBoot
                ? HOLD_BEHIND_MS
                : HOLD_WINDOW_ONLY_MS
              : followsBoot
                ? HOLD_QUIET_SESSION_MS
                : HOLD_QUIET_MS;
      holdTimer = setTimeout(() => void handOver(), ms);
    };

    const onBoot = (b: BootProgress) => {
      if (phase !== "starting") return;
      if (b.phase === "window") {
        sawWindow = true;
        // The emulator's window covers the stage now: nothing here may take GPU time from it.
        setPaused("game", true);
      } else {
        sawBoot = true;
        lastBoot = b.phase;
        showBoot(b);
      }
      schedule();
    };

    void (async () => {
      // Listen first, so an exit right after start is never missed.
      try {
        await events.gameExited(onExit, ctx.signal);
      } catch (e) {
        fail(errorText(e));
        return;
      }
      // Without boot events the stage falls back to the HOLD_QUIET_MS hand-over.
      await events.gameBoot(onBoot, ctx.signal).catch(() => undefined);
      setPadSuspended(true);
      try {
        const started = await api.launchGame(
          byPath
            ? { title_id: game.title_id || undefined, path: game.path, profile_id: profileId }
            : { title_id: game.title_id, profile_id: profileId },
        );
        followsBoot = started.follows_boot === true;
      } catch (e) {
        await entered;
        fail(errorText(e));
        return;
      }
      if (ctx.signal.aborted || phase !== "starting") return;
      await entered;
      if (phase !== "starting") return;
      // Hold the stage until the game is on screen (or a HOLD_* time passes without news).
      launched = true;
      schedule();
    })();

    return el;
  },
};

export default screen;
