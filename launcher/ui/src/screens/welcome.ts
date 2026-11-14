// First run. A ~4 s title sequence: darkness, a single point of light that draws a horizon across
// the window, the aurora rising out of it, then the drop formed of light: its streaks fly in, its
// core burns white-gold and cools to ember, the leaf unfolds, the echo arcs ripple ahead. The name
// tracks in letter by letter, light and wide, like a film title; then one control: "Set up".
// Only transform and opacity move (plus the drop's own path, once, on a small SVG). The sky behind
// is light.ts, faded in with setSkyLevel. Skippable with any key, click or pad button.

import { h, nextId, svg } from "../dom";
import { DROP_D, dropPath, echoD, LEAF_D } from "../icons";
import { focusElement } from "../input";
import { setSkyLevel } from "../light";
import { reducedMotion } from "../motion";
import { go, preload, type ScreenDef } from "../router";

const NAME = "Guarana";

function introMark(): SVGSVGElement {
  const id = nextId("intro");
  const echoes = [0, 1, 2]
    .map(
      (i) =>
        `<g class="intro-echo intro-echo-${i}" opacity="${[0.8, 0.45, 0.2][i]}"><path d="${echoD(i * 7)}" pathLength="1" fill="none" stroke="#FFC46A" stroke-opacity=".25" stroke-width="6" stroke-linecap="round"/><path d="${echoD(i * 7)}" pathLength="1" fill="none" stroke="#FFE8C4" stroke-width="1.8" stroke-linecap="round"/></g>`,
    )
    .join("");
  return svg(
    `<svg class="intro-mark" viewBox="-4 -2 86 68" aria-hidden="true" focusable="false">
      <defs>
        <linearGradient id="${id}-f" x1="22" y1="19" x2="48" y2="53" gradientUnits="userSpaceOnUse"><stop offset="0" stop-color="#FFD978"/><stop offset=".55" stop-color="#FFB040"/><stop offset="1" stop-color="#F2851A"/></linearGradient>
        <radialGradient id="${id}-c" cx="40" cy="32" r="18" gradientUnits="userSpaceOnUse"><stop offset="0" stop-color="#FFFFFF"/><stop offset=".45" stop-color="#FFF4DA" stop-opacity=".8"/><stop offset="1" stop-color="#FFF4DA" stop-opacity="0"/></radialGradient>
        <mask id="${id}-m" maskUnits="userSpaceOnUse" x="-10" y="-10" width="100" height="84">
          <rect x="-10" y="-10" width="100" height="84" fill="#fff"/>
          <path class="intro-cut" d="M29 29.2H-6" pathLength="1" stroke="#000" stroke-width="3.2" stroke-linecap="round"/>
          <path class="intro-cut" d="M29 42.8H-6" pathLength="1" stroke="#000" stroke-width="3.2" stroke-linecap="round"/>
        </mask>
        <clipPath id="${id}-d"><path d="${DROP_D}"/></clipPath>
      </defs>
      <g class="intro-drop-wrap">
        <path class="intro-drop" d="${DROP_D}" fill="url(#${id}-f)" mask="url(#${id}-m)"/>
        <g clip-path="url(#${id}-d)" mask="url(#${id}-m)"><circle class="intro-core" cx="40" cy="32" r="18" fill="url(#${id}-c)"/></g>
      </g>
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

    // The stage: a veil of darkness that lifts, a point of light, the horizon it draws, three
    // light trails (the drop's streaks, at window scale).
    const veil = h("div", { class: "intro-veil" });
    const spark = h("div", { class: "intro-spark" });
    const line = h("div", { class: "intro-line" });
    const trails = [0, 1, 2].map((i) => h("span", { class: `intro-trail intro-trail-${i}` }));
    const stage = h("div", { class: "stage", "aria-hidden": "true" }, veil, h("div", { class: "intro-trails" }, trails), line, spark);

    const mark = introMark();
    const logo = h("div", { class: "intro-logo" }, h("div", { class: "bloom" }), mark);
    const letters = Array.from(NAME).map((c) => h("span", { class: "intro-letter", text: c }));
    const word = h("h1", { class: "intro-word" }, letters);
    const tag = h("p", { class: "intro-tag", text: "Switch 2 emulation, tuned for integrated GPUs." });
    const setUp = h(
      "button",
      { type: "button", class: "btn btn-primary btn-xl intro-cta", "data-primary": true, "data-autofocus": true },
      h("span", { class: "btn-label", text: "Set up" }),
    );
    const fine = h("p", { class: "intro-fine", text: "Not affiliated with Nintendo. No keys, firmware or games included." });
    const skip = h("button", { type: "button", class: "btn btn-ghost btn-sm intro-skip" }, h("span", { class: "btn-label", text: "Skip intro" }));
    const center = h("div", { class: "intro-center" }, logo, word, tag, setUp, fine);
    const el = h("div", { class: "intro", "data-phase": reduce ? "ready" : "intro" }, stage, center, skip);

    const anims: Animation[] = [];
    let phase: "intro" | "ready" = reduce ? "ready" : "intro";

    const finish = () => {
      if (phase === "ready") return;
      phase = "ready";
      for (const a of anims) {
        try {
          if (a.effect?.getTiming().iterations === Infinity) a.cancel();
          else a.finish();
        } catch {
          a.cancel();
        }
      }
      setSkyLevel(1, 700);
      el.dataset.phase = "ready";
      skip.hidden = true;
      if (el.isConnected) focusElement(setUp);
    };
    ctx.onLeave(() => setSkyLevel(1));

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
    // Once "Set up" has finished appearing, pressing it works even while the last of the intro plays.
    let setUpVisible = false;
    el.addEventListener("pointerdown", (e) => {
      if (phase !== "intro") return;
      const onSetUp = e.target instanceof Node && setUp.contains(e.target);
      if (!(onSetUp && setUpVisible)) skippedAt = performance.now();
      finish();
    });

    if (reduce) {
      skip.hidden = true;
      setSkyLevel(1);
      return el;
    }

    // ---- Timeline (ms) ----
    const run = (target: Element | null, frames: Keyframe[], opts: KeyframeAnimationOptions): Animation | null => {
      if (!target) return null;
      const a = target.animate(frames, { fill: "backwards", ...opts });
      anims.push(a);
      return a;
    };
    const q = <T extends Element>(sel: string) => mark.querySelector<T>(sel);
    const glide = "cubic-bezier(.16,1,.3,1)";
    const drift = "cubic-bezier(.45,0,.2,1)";

    // Darkness, then the sky rises out of the horizon.
    setSkyLevel(0);
    setTimeout(() => {
      if (phase === "intro" && !ctx.signal.aborted) setSkyLevel(1, 2600);
    }, 900);
    run(veil, [{ opacity: 1 }, { opacity: 1, offset: 0.35 }, { opacity: 0 }], { duration: 2400, easing: drift });

    // A point of light; it draws the horizon outward from itself, then fades into it.
    run(spark, [
      { opacity: 0, transform: "scale(0)" },
      { opacity: 1, transform: "scale(1)", offset: 0.35 },
      { opacity: 1, transform: "scale(1.4)", offset: 0.6 },
      { opacity: 0, transform: "scale(0.6)" },
    ], { duration: 1500, delay: 150, easing: drift, fill: "both" });
    run(line, [
      { opacity: 0, transform: "scaleX(0)" },
      { opacity: 1, transform: "scaleX(0.02)", offset: 0.12 },
      { opacity: 1, transform: "scaleX(1)", offset: 0.55 },
      { opacity: 0.45, transform: "scaleX(1)" },
    ], { duration: 2200, delay: 500, easing: glide });

    // Three trails of light streak in from the left and land in the drop.
    trails.forEach((tr, i) => {
      run(tr, [
        { opacity: 0, transform: "translateX(-40vw) scaleX(0.4)" },
        { opacity: 1, transform: "translateX(-6vw) scaleX(1)", offset: 0.6 },
        { opacity: 0, transform: "translateX(0) scaleX(0.6)" },
      ], { duration: 1200, delay: 1150 + i * 70, easing: glide, fill: "both" });
    });

    // The drop forms: a ball of white light that stretches into motion and cools to ember.
    run(logo.querySelector(".bloom"), [
      { opacity: 0, transform: "scale(.3)" },
      { opacity: 1, transform: "scale(1.25)", offset: 0.4 },
      { opacity: 0.9, transform: "scale(1)" },
    ], { duration: 2200, delay: 1300, easing: glide });
    run(q(".intro-drop-wrap"), [{ opacity: 0, transform: "scale(.3)" }, { opacity: 1, transform: "scale(1)" }], { duration: 1100, delay: 1500, easing: glide });
    run(q(".intro-core"), [{ opacity: 1 }, { opacity: 1, offset: 0.3 }, { opacity: 0.38 }], { duration: 1800, delay: 1500, easing: drift });
    const drop = q<SVGPathElement>(".intro-drop");
    if (drop && CSS.supports("d", 'path("M0 0")')) {
      const tips = [18.6, 18.6, 12, 6, 3, 2.2, 3.4, 4];
      run(drop, tips.map((t, i) => ({ d: `path("${dropPath(t)}")`, offset: i / (tips.length - 1) })), { duration: 1300, delay: 1700, easing: drift });
    }
    for (const [i, cut] of Array.from(mark.querySelectorAll(".intro-cut")).entries()) {
      run(cut, [{ strokeDasharray: "1 1", strokeDashoffset: 1 }, { strokeDasharray: "1 1", strokeDashoffset: 0 }], { duration: 800, delay: 1900 + i * 110, easing: glide });
    }
    run(q(".intro-leaf"), [
      { opacity: 0, transform: "scale(0) rotate(-40deg)" },
      { opacity: 1, transform: "scale(1) rotate(0)" },
    ], { duration: 1100, delay: 2200, easing: glide });
    for (const [i, arc] of Array.from(mark.querySelectorAll(".intro-echo")).entries()) {
      run(arc, [
        { opacity: 0, transform: "translateX(-8px)" },
        { opacity: 1, transform: "translateX(2px)", offset: 0.6 },
        { opacity: 1, transform: "translateX(0)" },
      ], { duration: 900, delay: 2350 + i * 160, easing: glide });
    }

    // The name tracks in from wide spacing, one letter after another, light and slow.
    const mid = (letters.length - 1) / 2;
    letters.forEach((l, i) => {
      run(l, [
        { opacity: 0, transform: `translateX(${((i - mid) * 0.5).toFixed(2)}em)` },
        { opacity: 1, transform: "translateX(0)" },
      ], { duration: 1500, delay: 2500 + Math.abs(i - mid) * 70, easing: glide });
    });
    run(tag, [{ opacity: 0, transform: "translateY(12px)" }, { opacity: 1, transform: "translateY(0)" }], { duration: 1000, delay: 3100, easing: glide });
    run(setUp, [{ opacity: 0, transform: "translateY(16px)" }, { opacity: 1, transform: "translateY(0)" }], { duration: 1000, delay: 3350, easing: glide })
      ?.finished.then(() => (setUpVisible = true), () => undefined);
    run(fine, [{ opacity: 0 }, { opacity: 1 }], { duration: 900, delay: 3700, easing: "ease-out" });
    run(center, [{ transform: "scale(.985)" }, { transform: "scale(1)" }], { duration: 4400, easing: glide });

    // The horizon and the trails sit at the drop's height, wherever the layout put the logo.
    requestAnimationFrame(() => {
      const r = logo.getBoundingClientRect();
      if (r.height) stage.style.setProperty("--oy", `${Math.round(r.top + r.height * 0.53)}px`);
    });

    void Promise.all(anims.map((a) => a.finished.catch(() => undefined))).then(() => {
      if (!ctx.signal.aborted) finish();
    });
    return el;
  },
};

export default screen;
