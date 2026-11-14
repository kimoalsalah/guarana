// Instrument parts: the odometer numeral, the frame-pacing strip and the hairline readout. These are
// the "precision instrument" vocabulary every showpiece is built from. Everything moves with
// transform and opacity only; nothing loops on its own except the strip's playhead, which steps at
// 12 fps for two passes, is marked .ambient (stops while the window is unfocused) and stops with
// html[data-paused] like every CSS animation.

import { h, nextId, svg } from "../dom";
import { reducedMotion } from "../motion";

// ---- Odometer ---------------------------------------------------------------------------------------

export interface Odometer {
  el: HTMLElement;
  /** Rolls every digit to the new text. Non-digits (".", ",", "%", "–") are set flat. */
  set(text: string): void;
  value(): string;
}

/**
 * A rolling numeral. Each digit is a 0-9 reel moved by translateY, so a change is a compositor-only
 * roll. Screen readers get the plain text (the reels are aria-hidden).
 * `from` is what the reels show before the first set (they then roll up to `text` on the next frame).
 */
export function odometer(text: string, opts: { cls?: string; from?: string; label?: string } = {}): Odometer {
  const reels = h("span", { class: "odo-reels", "aria-hidden": "true" });
  const plain = h("span", { class: "sr-only" });
  const el = h("span", { class: `odo ${opts.cls ?? ""}` }, reels, plain);
  let current = "";

  const build = (t: string) => {
    // Rebuild only when the shape (digit vs. not, per position) changed; otherwise just roll.
    const shape = t.replace(/\d/g, "0");
    if (reels.dataset.shape === shape) return;
    reels.dataset.shape = shape;
    const kids: HTMLElement[] = [];
    for (const ch of t) {
      if (/\d/.test(ch)) {
        const reel = h("span", { class: "odo-reel" }, Array.from({ length: 10 }, (_, n) => h("span", { text: String(n) })));
        kids.push(h("span", { class: "odo-d" }, reel));
      } else {
        kids.push(h("span", { class: "odo-s", text: ch }));
      }
    }
    reels.replaceChildren(...kids);
  };

  const roll = (t: string) => {
    const cells = Array.from(reels.children) as HTMLElement[];
    const chars = Array.from(t);
    const n = cells.length;
    cells.forEach((cell, i) => {
      const ch = chars[i] ?? "";
      if (cell.classList.contains("odo-d")) {
        const reel = cell.firstElementChild as HTMLElement;
        // Right-most digits lead, like a counter carrying over.
        reel.style.transitionDelay = `${(n - 1 - i) * 28}ms`;
        reel.style.transform = `translateY(${-Number(ch) * 10}%)`;
      } else {
        cell.textContent = ch;
      }
    });
  };

  const set = (t: string) => {
    if (t === current) return;
    current = t;
    plain.textContent = opts.label ? `${t} ${opts.label}` : t;
    build(t);
    roll(t);
  };

  if (opts.from !== undefined && !reducedMotion()) {
    // Show `from` without a transition, then roll to `text` two frames later.
    const start = Array.from(opts.from.padStart(text.length, "0").slice(-text.length));
    build(text);
    reels.classList.add("is-armed");
    roll(Array.from(text).map((c, i) => (/\d/.test(c) ? (/\d/.test(start[i]) ? start[i] : "0") : c)).join(""));
    current = text;
    plain.textContent = opts.label ? `${text} ${opts.label}` : text;
    requestAnimationFrame(() =>
      requestAnimationFrame(() => {
        reels.classList.remove("is-armed");
        if (current === text) roll(text);
      }),
    );
  } else {
    set(text);
  }
  return { el, set, value: () => current };
}

/** "93.4" style: one decimal, or "–" when unknown. */
export function fixed1(n: number | null | undefined): string {
  return n === null || n === undefined || !Number.isFinite(n) ? "–" : n.toFixed(1);
}

// ---- Frame-pacing strip -------------------------------------------------------------------------------

/** One session's frame pacing, as the emulator's log reports it (ShareOffer's numbers). */
export interface Pacing {
  at: string;
  game_fps: number;
  /** Percent of frames within 2 ms of their slot: "frames on time". */
  within2: number;
  shown_fps: number;
  p50: number;
  p95: number;
  p99: number;
  gameplay_secs: number;
}

export interface PacingStrip {
  el: HTMLElement;
  set(p: Pacing | null): void;
  /** Wipes the strip in from the left (once; skipped under reduced motion). */
  reveal(delay?: number): void;
}

const N = 240;
const VW = 960;
const VH = 120;

/** Small seeded PRNG (mulberry32), so a title always draws the same profile. */
function rng(seed: string): () => number {
  let a = 2166136261;
  for (let i = 0; i < seed.length; i++) a = Math.imul(a ^ seed.charCodeAt(i), 16777619);
  return () => {
    a |= 0;
    a = (a + 0x6d2b79f5) | 0;
    let t = Math.imul(a ^ (a >>> 15), 1 | a);
    t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}

/**
 * Frame times whose distribution has the session's p50, p95 and p99 and its share of frames on time:
 * a profile drawn from the measured percentiles, labelled as such, never presented as a recording.
 */
function frames(p: Pacing, seed: string): number[] {
  const r = rng(seed);
  const onTime = Math.min(1, Math.max(0, p.within2 / 100));
  const out: number[] = [];
  for (let i = 0; i < N; i++) {
    const u = r();
    let ms: number;
    if (u < 0.01) ms = p.p99 * (0.92 + r() * 0.16);
    else if (u < 0.05) ms = p.p95 + (p.p99 - p.p95) * r() * 0.6;
    // The tail above is already late; the rest of the late share sits between p50 + 2 ms and p95.
    else if (u < Math.max(0.05, 1 - onTime)) ms = p.p50 + 2.2 + Math.max(0, p.p95 - p.p50 - 2.2) * r();
    else ms = p.p50 + (r() - 0.5) * 3.2;
    out.push(Math.max(1, ms));
  }
  return out;
}

export function pacingStrip(opts: { seed: string; budgetMs?: number; cls?: string } = { seed: "guarana" }): PacingStrip {
  const id = nextId("strip");
  const graph = svg(
    `<svg class="strip-svg" viewBox="0 0 ${VW} ${VH}" preserveAspectRatio="none" aria-hidden="true" focusable="false">
      <defs><clipPath id="${id}-c"><rect x="0" y="0" width="${VW}" height="${VH}"/></clipPath></defs>
      <g clip-path="url(#${id}-c)">
        <rect class="strip-band" x="0" y="0" width="${VW}" height="0"/>
        <path class="strip-grid" d="" vector-effect="non-scaling-stroke"/>
        <path class="strip-bars" d="" vector-effect="non-scaling-stroke"/>
        <path class="strip-late" d="" vector-effect="non-scaling-stroke"/>
        <path class="strip-median" d="" vector-effect="non-scaling-stroke"/>
        <path class="strip-base" d="M0 ${VH - 0.5}H${VW}" vector-effect="non-scaling-stroke"/>
      </g>
    </svg>`,
  );
  const q = <T extends Element>(s: string) => graph.querySelector<T>(s) as T;
  const band = q<SVGRectElement>(".strip-band");
  const grid = q<SVGPathElement>(".strip-grid");
  const bars = q<SVGPathElement>(".strip-bars");
  const late = q<SVGPathElement>(".strip-late");
  const median = q<SVGPathElement>(".strip-median");

  const axisTop = h("span", { class: "strip-axis strip-axis-top" });
  const axisMid = h("span", { class: "strip-axis strip-axis-mid" });
  const caption = h("p", { class: "strip-caption" });
  const head = h("span", { class: "strip-playhead ambient", "aria-hidden": "true" });
  const cover = h("span", { class: "strip-cover", "aria-hidden": "true" }, h("span", { class: "strip-scan" }));
  const frame = h("div", { class: "strip-frame" }, graph, axisTop, axisMid, head, cover);
  const el = h("figure", { class: `strip ${opts.cls ?? ""}` }, frame, h("figcaption", {}, caption));

  const y = (ms: number, max: number) => VH - (Math.min(ms, max) / max) * (VH - 6);

  const set = (p: Pacing | null) => {
    if (!p) {
      // No session yet: the instrument shows its target cadence, a frame boundary every 16.7 ms.
      const budget = opts.budgetMs ?? 16.7;
      const max = budget * 3;
      let d = "";
      for (let i = 0; i <= N; i += 4) d += `M${((i / N) * VW).toFixed(1)} ${VH}V${y(budget, max).toFixed(1)}`;
      grid.setAttribute("d", d);
      bars.setAttribute("d", "");
      late.setAttribute("d", "");
      median.setAttribute("d", `M0 ${y(budget, max).toFixed(1)}H${VW}`);
      band.setAttribute("height", "0");
      axisTop.textContent = `${max.toFixed(0)} ms`;
      axisMid.textContent = `${budget} ms`;
      axisMid.style.setProperty("--y", `${(y(budget, max) / VH) * 100}%`);
      caption.textContent = "Target cadence: a frame every 16.7 ms. Your first session's frame times land here.";
      el.classList.add("is-empty");
      return;
    }
    el.classList.remove("is-empty");
    // Scaled to the p95 so the body of the distribution has height; the rare p99 spikes run off the
    // top of the frame (clipped), which is what a spike looks like on a frame-time graph.
    const max = Math.max(p.p95 * 1.5, p.p50 * 2.5);
    const list = frames(p, opts.seed);
    const lo = p.p50 - 2;
    const hi = p.p50 + 2;
    let onD = "";
    let lateD = "";
    const step = VW / N;
    list.forEach((ms, i) => {
      const x = (i * step + step / 2).toFixed(1);
      const seg = `M${x} ${VH}V${y(ms, max).toFixed(1)}`;
      if (ms > hi || ms < lo) lateD += seg;
      else onD += seg;
    });
    bars.setAttribute("d", onD);
    late.setAttribute("d", lateD);
    median.setAttribute("d", `M0 ${y(p.p50, max).toFixed(1)}H${VW}`);
    band.setAttribute("y", y(hi, max).toFixed(1));
    band.setAttribute("height", Math.max(1, y(lo, max) - y(hi, max)).toFixed(1));
    let g = "";
    for (const ms of [p.p95, p.p99]) g += `M0 ${y(ms, max).toFixed(1)}H${VW}`;
    grid.setAttribute("d", g);
    axisTop.textContent = `${max.toFixed(0)} ms`;
    axisMid.textContent = `p50 ${p.p50.toFixed(1)} ms`;
    axisMid.style.setProperty("--y", `${(y(p.p50, max) / VH) * 100}%`);
    caption.textContent = `Frame times modelled from your last session's p50, p95 and p99. The band is ±2 ms around the median; taller marks missed it.`;
  };

  return {
    el,
    set,
    reveal(delay = 0) {
      if (reducedMotion()) {
        el.classList.add("is-revealed");
        return;
      }
      el.style.setProperty("--reveal-delay", `${delay}ms`);
      el.classList.add("is-revealing");
      cover.addEventListener(
        "animationend",
        () => {
          el.classList.remove("is-revealing");
          el.classList.add("is-revealed");
        },
        { once: true },
      );
    },
  };
}

// ---- Readout ------------------------------------------------------------------------------------------

/** A labelled measurement: big value on top, a mono label under it, an optional unit. */
export function readout(label: string, value: HTMLElement | string, opts: { unit?: string; cls?: string } = {}): HTMLElement {
  return h(
    "div",
    { class: `readout ${opts.cls ?? ""}` },
    h("span", { class: "readout-value" }, value, opts.unit ? h("span", { class: "readout-unit", text: opts.unit }) : null),
    h("span", { class: "readout-label", text: label }),
  );
}

/** Mono label with an index number, like "02 / Mode". */
export function indexLabel(text: string, cls = ""): HTMLElement {
  return h("p", { class: `idx ${cls}` }, h("span", { class: "idx-n", "aria-hidden": "true" }), h("span", { class: "idx-t", text }));
}
