// First run: the instrument powers on (~2.5 s). The measuring grid sweeps in from the centre, a scan
// line passes, the drop is drawn as an outline and filled, its streaks are cut, the leaf and the echo
// arcs (predicted frames) tick in, the wordmark is set letter by letter, and a measured number rolls
// on an odometer: Hyrule Field on an Arc 140T, 14.75 to 22.83 game fps. Then one button: "Set up".
// Only transform and opacity move (plus the drop's outline stroke, a small one-off paint).
// Skippable with any key, click or pad button; reduced motion shows the final frame.

import { h, nextId, svg } from "../dom";
import { DROP_D, echoD, LEAF_D } from "../icons";
import { focusElement } from "../input";
import { reducedMotion } from "../motion";
import { go, preload, type ScreenDef } from "../router";
import { odometer } from "../ui/instrument";

function introMark(): SVGSVGElement {
  const id = nextId("intro");
  const echoes = [0, 1, 2]
    .map((i) => `<path class="intro-echo intro-echo-${i}" d="${echoD(i * 7)}" pathLength="1" fill="none" stroke="#F59E0B" stroke-opacity="${[0.7, 0.38, 0.18][i]}" stroke-width="3" stroke-linecap="butt"/>`)
    .join("");
  return svg(
    `<svg class="intro-mark" viewBox="-4 -2 86 68" aria-hidden="true" focusable="false">
      <defs>
        <linearGradient id="${id}-f" x1="22" y1="19" x2="48" y2="53" gradientUnits="userSpaceOnUse"><stop offset="0" stop-color="#FFC94A"/><stop offset="1" stop-color="#F2851A"/></linearGradient>
        <mask id="${id}-m" maskUnits="userSpaceOnUse" x="-10" y="-10" width="100" height="84">
          <rect x="-10" y="-10" width="100" height="84" fill="#fff"/>
          <path class="intro-cut" d="M29 29.2H-6" pathLength="1" stroke="#000" stroke-width="3.2"/>
          <path class="intro-cut" d="M29 42.8H-6" pathLength="1" stroke="#000" stroke-width="3.2"/>
        </mask>
      </defs>
      <path class="intro-outline" d="${DROP_D}" pathLength="1" fill="none" stroke="#FF9A2E" stroke-width=".6"/>
      <path class="intro-drop" d="${DROP_D}" fill="url(#${id}-f)" mask="url(#${id}-m)"/>
      <path class="intro-leaf" d="${LEAF_D}" fill="#3CCF7E"/>
      ${echoes}
    </svg>`,
  );
}

const screen: ScreenDef = {
  title: "Welcome",
  chrome: "none",

  render(ctx) {
    preload("scan");
    const reduce = reducedMotion();

    // ---- The measuring field: grid lines, a scan line and a pointer crosshair.
    const vLines = Array.from({ length: 13 }, (_, i) => h("span", { class: "ix-v", style: { left: `${(i / 12) * 100}%` } }));
    const hLines = Array.from({ length: 9 }, (_, i) => h("span", { class: "ix-h", style: { top: `${(i / 8) * 100}%` } }));
    const scan = h("span", { class: "ix-scan" });
    const cross = h("div", { class: "ix-cross" }, h("span", { class: "ix-cx" }), h("span", { class: "ix-cy" }));
    const coord = h("span", { class: "ix-coord", text: "X 0000  Y 0000" });
    const field = h("div", { class: "ix-field", "aria-hidden": "true" }, vLines, hLines, scan, cross);

    // ---- The plate: mark, wordmark, the measured number, the tagline, Set up.
    const mark = introMark();
    const ticks = h("div", { class: "ix-ticks", "aria-hidden": "true" }, ["t", "t+1", "t+2", "t+3"].map((t) => h("span", { text: t })));
    const logo = h("div", { class: "intro-logo" }, mark, ticks);
    const letters = Array.from("Guarana").map((c) => h("span", { class: "ix-ch" }, h("span", { text: c })));
    const word = h("h1", { class: "intro-word", "aria-label": "Guarana" }, h("span", { "aria-hidden": "true", class: "intro-word-text" }, letters));

    const fps = odometer("00.00");
    const fpsTo = odometer("00.00");
    const rule = h("span", { class: "ix-rule", "aria-hidden": "true" });
    const measure = h(
      "div",
      { class: "ix-measure" },
      rule,
      h(
        "div",
        { class: "ix-readouts" },
        h("span", { class: "ix-num" }, fps.el),
        h("span", { class: "ix-arrow", "aria-hidden": "true", text: "→" }),
        h("span", { class: "ix-num ix-num-to" }, fpsTo.el),
        h("span", { class: "ix-unit", text: "game fps" }),
      ),
      h("p", { class: "ix-caption", text: "Measured on an Arc 140T · Hyrule Field · frame generation off" }),
    );
    const tag = h("p", { class: "intro-tag", text: "Switch 2 emulation, tuned for integrated GPUs." });
    const setUp = h(
      "button",
      { type: "button", class: "btn btn-primary btn-xl intro-cta", "data-primary": true, "data-autofocus": true },
      h("span", { class: "btn-label", text: "Set up" }),
    );
    const fine = h("p", { class: "intro-fine", text: "Not affiliated with Nintendo. No keys, firmware or games included." });
    const skip = h("button", { type: "button", class: "btn btn-ghost btn-sm intro-skip" }, h("span", { class: "btn-label", text: "Skip intro" }));
    const meta = h("p", { class: "ix-meta", "aria-hidden": "true" }, h("span", { text: "Guarana" }), h("span", { text: "First run" }), coord);
    const center = h("div", { class: "intro-center" }, logo, word, measure, tag, h("div", { class: "ix-cta" }, setUp, fine));
    const el = h("div", { class: "intro", "data-phase": reduce ? "ready" : "intro" }, field, meta, center, skip);

    const anims: Animation[] = [];
    const timers: number[] = [];
    let phase: "intro" | "ready" = reduce ? "ready" : "intro";
    const finalNumbers = () => {
      fps.set("14.75");
      fpsTo.set("22.83");
    };

    const finish = () => {
      if (phase === "ready") return;
      phase = "ready";
      for (const t of timers) window.clearTimeout(t);
      for (const a of anims) {
        try {
          a.finish();
        } catch {
          a.cancel();
        }
      }
      finalNumbers();
      el.dataset.phase = "ready";
      skip.hidden = true;
      if (el.isConnected) focusElement(setUp);
    };

    // A click that skips the intro must not also press "Set up" underneath it.
    let skippedAt = 0;
    setUp.addEventListener("click", () => {
      if (phase !== "ready" || performance.now() - skippedAt < 400) return;
      void go("scan", { transition: "intro-exit" });
    });
    skip.addEventListener("click", () => {
      skippedAt = performance.now();
      finish();
    });
    ctx.onIntent((intent) => {
      if (phase === "intro") {
        finish();
        return true;
      }
      return intent === "back";
    });
    const onKey = (e: KeyboardEvent) => {
      if (phase === "intro" && !e.repeat) {
        e.preventDefault();
        finish();
      }
    };
    window.addEventListener("keydown", onKey, { capture: true, signal: ctx.signal });
    // Once "Set up" has appeared, pressing it works even while the last of the intro plays.
    let setUpVisible = false;
    el.addEventListener("pointerdown", (e) => {
      if (phase !== "intro") return;
      const onSetUp = e.target instanceof Node && setUp.contains(e.target);
      if (!(onSetUp && setUpVisible)) skippedAt = performance.now();
      finish();
    });

    // The crosshair follows the pointer (transform only, one write per frame at most), and the corner
    // readout gives its position on the measuring grid. Nothing runs while the pointer rests.
    let frame = 0;
    let px = 0;
    let py = 0;
    el.addEventListener("pointermove", (e) => {
      px = e.clientX;
      py = e.clientY;
      if (frame) return;
      frame = requestAnimationFrame(() => {
        frame = 0;
        cross.style.transform = `translate(${Math.round(px)}px, ${Math.round(py)}px)`;
        cross.classList.add("is-on");
        coord.textContent = `X ${String(Math.round(px)).padStart(4, "0")}  Y ${String(Math.round(py)).padStart(4, "0")}`;
      });
    });
    ctx.signal.addEventListener("abort", () => cancelAnimationFrame(frame));

    if (reduce) {
      skip.hidden = true;
      finalNumbers();
      return el;
    }

    // ---- Timeline (ms). Fast, mechanical, in sequence: nothing overshoots.
    const mech = "cubic-bezier(.2,0,0,1)";
    const run = (target: Element | null, frames: Keyframe[], opts: KeyframeAnimationOptions): Animation | null => {
      if (!target) return null;
      const a = target.animate(frames, { fill: "backwards", easing: mech, ...opts });
      anims.push(a);
      return a;
    };
    const at = (ms: number, fn: () => void) => timers.push(window.setTimeout(fn, ms));
    ctx.signal.addEventListener("abort", () => timers.forEach((t) => window.clearTimeout(t)));
    const q = <T extends Element>(sel: string) => mark.querySelector<T>(sel);

    // The grid sweeps out from the centre lines.
    vLines.forEach((l, i) => run(l, [{ transform: "scaleY(0)" }, { transform: "scaleY(1)" }], { duration: 200, delay: 40 + Math.abs(i - 6) * 22 }));
    hLines.forEach((l, i) => run(l, [{ transform: "scaleX(0)" }, { transform: "scaleX(1)" }], { duration: 200, delay: 140 + Math.abs(i - 4) * 26 }));
    run(scan, [
      { transform: "translateY(0)", opacity: 0 },
      { opacity: 1, offset: 0.1 },
      { opacity: 1, offset: 0.9 },
      { transform: "translateY(100vh)", opacity: 0 },
    ], { duration: 760, delay: 200, easing: "cubic-bezier(.5,0,.5,1)", fill: "both" });
    run(meta, [{ opacity: 0 }, { opacity: 1 }], { duration: 160, delay: 300 });

    // The drop: outline drawn, then filled, streaks cut, leaf, echo arcs with their frame ticks.
    run(q(".intro-outline"), [{ strokeDasharray: "1 1", strokeDashoffset: 1 }, { strokeDasharray: "1 1", strokeDashoffset: 0 }], { duration: 380, delay: 420, easing: "cubic-bezier(.4,0,.2,1)" });
    run(q(".intro-drop"), [{ opacity: 0 }, { opacity: 1 }], { duration: 160, delay: 780, easing: "linear" });
    for (const [i, cut] of Array.from(mark.querySelectorAll(".intro-cut")).entries()) {
      run(cut, [{ strokeDasharray: "1 1", strokeDashoffset: 1 }, { strokeDasharray: "1 1", strokeDashoffset: 0 }], { duration: 160, delay: 880 + i * 60 });
    }
    run(q(".intro-leaf"), [{ opacity: 0, transform: "scale(0)" }, { opacity: 1, transform: "scale(1)" }], { duration: 160, delay: 980 });
    for (const [i, arc] of Array.from(mark.querySelectorAll(".intro-echo")).entries()) {
      run(arc, [{ strokeDasharray: "1 1", strokeDashoffset: 1 }, { strokeDasharray: "1 1", strokeDashoffset: 0 }], { duration: 140, delay: 1060 + i * 90 });
    }
    Array.from(ticks.children).forEach((t, i) => run(t, [{ opacity: 0, transform: "translateY(4px)" }, { opacity: 1, transform: "none" }], { duration: 120, delay: 1040 + i * 90 }));

    // The wordmark, set letter by letter.
    letters.forEach((l, i) => run(l.firstElementChild, [{ transform: "translateY(105%)" }, { transform: "translateY(0)" }], { duration: 200, delay: 1160 + i * 40 }));

    // The measurement: the rule draws, the numbers roll.
    run(rule, [{ transform: "scaleX(0)" }, { transform: "scaleX(1)" }], { duration: 260, delay: 1500 });
    run(measure.querySelector(".ix-readouts"), [{ opacity: 0 }, { opacity: 1 }], { duration: 120, delay: 1560 });
    run(measure.querySelector(".ix-caption"), [{ opacity: 0 }, { opacity: 1 }], { duration: 160, delay: 1700 });
    at(1600, () => fps.set("14.75"));
    at(2000, () => fpsTo.set("22.83"));

    run(tag, [{ opacity: 0, transform: "translateY(6px)" }, { opacity: 1, transform: "none" }], { duration: 200, delay: 2000 });
    run(setUp, [{ opacity: 0, transform: "translateY(8px)" }, { opacity: 1, transform: "none" }], { duration: 200, delay: 2200 })?.finished.then(
      () => (setUpVisible = true),
      () => undefined,
    );
    run(fine, [{ opacity: 0 }, { opacity: 1 }], { duration: 160, delay: 2320 });

    void Promise.all(anims.map((a) => a.finished.catch(() => undefined))).then(() => {
      if (!ctx.signal.aborted) at(450, finish);
    });
    return el;
  },
};

export default screen;
