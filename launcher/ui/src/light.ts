// The sky. One small canvas (160 px wide) holds the aurora: a few soft ellipses of ember, violet and
// gold light that drift on slow, never-repeating paths. The browser stretches it over the window, so a
// frame costs a 160 x ~100 px redraw and one texture upload, never a full-window paint.
//
// The same loop leans the light toward what you are looking at (the focused control with keys or a
// pad, the pointer with a mouse), and tells each glass panel how much light its edge catches.
//
// Budget: at most FPS frames a second; nothing at all while motion.ts reports paused (a game runs, the
// window is hidden) or idle (the window is unfocused), under reduced motion (one still frame), or after
// REST_AFTER_MS without input (the sky settles; the next input wakes it).

import { inputMode } from "./input";
import { isIdle, isPaused, onIdleChange, onPauseChange, reducedMotion, transitionRunning } from "./motion";
import { currentTheme, onThemeChange } from "./theme";

const FPS = 24;
const REST_AFTER_MS = 45_000;
const COLS = 160;
/** How often panels re-read the light on their edges (ms). Values are quantised, so most reads write nothing. */
const RIM_EVERY_MS = 480;

type RGB = readonly [number, number, number];

interface Ribbon {
  /** Base centre, amplitude and period (s) of the drift, all in window fractions. */
  x: number;
  y: number;
  ax: number;
  ay: number;
  px: number;
  py: number;
  phase: number;
  /** Radii as fractions of the window, and a slow rotation (radians). */
  rx: number;
  ry: number;
  rot: number;
  spin: number;
  color: RGB;
  alpha: number;
  /** How strongly this ribbon is pulled toward the focus (0..1). */
  lean: number;
}

interface Palette {
  sky: [string, string, string];
  blend: GlobalCompositeOperation;
  ribbons: Ribbon[];
  spot: RGB;
  spotAlpha: number;
}

// Night: an indigo-black sky, ember low on the right, violet low on the left, a pale gold veil above.
const NIGHT: Palette = {
  sky: ["#0c0a1f", "#07060f", "#040408"],
  blend: "lighter",
  spot: [255, 222, 168],
  spotAlpha: 0.2,
  ribbons: [
    { x: 0.74, y: 0.42, ax: 0.1, ay: 0.08, px: 71, py: 53, phase: 0.4, rx: 0.46, ry: 0.2, rot: -0.32, spin: 0.05, color: [255, 132, 44], alpha: 0.5, lean: 0.34 },
    { x: 0.2, y: 0.74, ax: 0.13, ay: 0.07, px: 83, py: 61, phase: 2.1, rx: 0.5, ry: 0.22, rot: 0.28, spin: -0.04, color: [118, 76, 255], alpha: 0.46, lean: 0.12 },
    { x: 0.52, y: 0.08, ax: 0.18, ay: 0.05, px: 97, py: 67, phase: 4.2, rx: 0.62, ry: 0.13, rot: 0.06, spin: 0.03, color: [255, 196, 120], alpha: 0.2, lean: 0.08 },
    { x: 0.92, y: 0.9, ax: 0.06, ay: 0.06, px: 59, py: 89, phase: 1.3, rx: 0.3, ry: 0.18, rot: -0.6, spin: 0.06, color: [196, 60, 150], alpha: 0.16, lean: 0 },
  ],
};

// Dawn (the light theme): pearl sky, peach and lavender light, normal blending.
const DAWN: Palette = {
  sky: ["#fbf7f2", "#f6f0e9", "#efe7df"],
  blend: "source-over",
  spot: [255, 236, 204],
  spotAlpha: 0.5,
  ribbons: [
    { x: 0.76, y: 0.4, ax: 0.1, ay: 0.08, px: 71, py: 53, phase: 0.4, rx: 0.46, ry: 0.22, rot: -0.32, spin: 0.05, color: [255, 186, 130], alpha: 0.42, lean: 0.34 },
    { x: 0.2, y: 0.76, ax: 0.13, ay: 0.07, px: 83, py: 61, phase: 2.1, rx: 0.5, ry: 0.24, rot: 0.28, spin: -0.04, color: [184, 166, 255], alpha: 0.36, lean: 0.12 },
    { x: 0.52, y: 0.06, ax: 0.18, ay: 0.05, px: 97, py: 67, phase: 4.2, rx: 0.62, ry: 0.14, rot: 0.06, spin: 0.03, color: [255, 222, 168], alpha: 0.34, lean: 0.08 },
    { x: 0.92, y: 0.92, ax: 0.06, ay: 0.06, px: 59, py: 89, phase: 1.3, rx: 0.3, ry: 0.18, rot: -0.6, spin: 0.06, color: [255, 176, 204], alpha: 0.2, lean: 0 },
  ],
};

let canvas: HTMLCanvasElement | null = null;
let ctx: CanvasRenderingContext2D | null = null;
let W = COLS;
let H = 100;
let palette: Palette = NIGHT;

let raf = 0;
let last = 0;
let t = 0; // drift time (s); advances only while the loop runs, so a resume never jumps
let lastInput = performance.now();
let lastRim = 0;

// Lean: where the light is pulled (window fractions), its strength, and the eased current values.
const lean = { tx: 0.7, ty: 0.45, tw: 0, x: 0.7, y: 0.45, w: 0 };
let pinned: { x: number; y: number; w: number } | null = null;

// Global brightness (the intro blooms it from 0; the launch stage dims it).
const level = { v: 1, from: 1, to: 1, start: 0, ms: 0 };

const rimSeen = new WeakMap<HTMLElement, string>();

export function initLight(): void {
  canvas = document.createElement("canvas");
  canvas.className = "sky-canvas";
  canvas.setAttribute("aria-hidden", "true");
  // A software canvas on purpose: at 160 px it costs next to nothing to draw on the CPU, and it holds
  // no GPU canvas context of its own next to a running game.
  ctx = canvas.getContext("2d", { alpha: false, willReadFrequently: true });
  document.body.prepend(canvas);
  const grain = noiseTile();
  if (grain) document.documentElement.style.setProperty("--noise", `url("${grain}")`);
  palette = currentTheme() === "light" ? DAWN : NIGHT;
  size();
  draw();

  window.addEventListener("resize", () => {
    size();
    draw();
  });
  onThemeChange((th) => {
    palette = th === "light" ? DAWN : NIGHT;
    draw();
  });
  onPauseChange(() => wake());
  onIdleChange(() => wake());
  document.addEventListener("visibilitychange", () => wake());

  const poke = () => {
    lastInput = performance.now();
    wake();
  };
  document.addEventListener("keydown", poke, { passive: true });
  document.addEventListener("wheel", poke, { passive: true });
  document.addEventListener("pointerdown", poke, { passive: true });
  document.addEventListener("pointermove", (e) => {
    lastInput = performance.now();
    if (inputMode() === "mouse" && e.pointerType === "mouse") aim(e.clientX / innerWidth, e.clientY / innerHeight, 0.55);
    wake();
  }, { passive: true });
  document.addEventListener("focusin", () => {
    lastInput = performance.now();
    aimAtFocus();
    wake();
  });
  window.addEventListener("scroll", () => {
    if (inputMode() !== "mouse") aimAtFocus();
  }, { capture: true, passive: true });
  wake();
}

/** A 128 px tile of faint grain, made once: it dithers the stretched sky so its gradients never band. */
function noiseTile(): string | null {
  const c = document.createElement("canvas");
  c.width = 128;
  c.height = 128;
  const g = c.getContext("2d");
  if (!g) return null;
  const img = g.createImageData(128, 128);
  for (let i = 0; i < img.data.length; i += 4) {
    const v = Math.random() < 0.5 ? 0 : 255;
    img.data[i] = v;
    img.data[i + 1] = v;
    img.data[i + 2] = v;
    img.data[i + 3] = 5 + Math.random() * 7;
  }
  g.putImageData(img, 0, 0);
  try {
    return c.toDataURL("image/png");
  } catch {
    return null;
  }
}

function size(): void {
  if (!canvas) return;
  W = COLS;
  H = Math.max(40, Math.round((COLS * innerHeight) / Math.max(1, innerWidth)));
  canvas.width = W;
  canvas.height = H;
}

function aim(x: number, y: number, w: number): void {
  lean.tx = Math.min(1, Math.max(0, x));
  lean.ty = Math.min(1, Math.max(0, y));
  lean.tw = w;
}

function aimAtFocus(): void {
  const el = document.activeElement;
  if (!(el instanceof HTMLElement) || el === document.body || inputMode() === "mouse") return;
  const r = el.getBoundingClientRect();
  if (r.width < 2) return;
  aim((r.left + r.width / 2) / innerWidth, (r.top + r.height / 2) / innerHeight, 1);
}

/** Holds the light at a point (window fractions) until released with null: the launch stage uses it. */
export function pinLight(p: { x: number; y: number; w?: number } | null): void {
  pinned = p ? { x: p.x, y: p.y, w: p.w ?? 1 } : null;
  if (!canRun()) {
    // No loop to ease it there: place the light at once (one still frame).
    if (pinned) {
      lean.x = pinned.x;
      lean.y = pinned.y;
    }
    lean.w = pinned ? pinned.w : 0;
    // While a game runs nothing is drawn at all; the next frame after it picks this up.
    if (!isPaused()) draw();
    return;
  }
  wake();
}

/** Fades the whole sky to `v` (0..1) over `ms`. Instant under reduced motion. */
export function setSkyLevel(v: number, ms = 0): void {
  if (ms <= 0 || !canRun()) {
    level.v = level.to = level.from = v;
    level.ms = 0;
    draw();
    return;
  }
  level.from = level.v;
  level.to = v;
  level.start = performance.now();
  level.ms = ms;
  wake();
}

function animatingLevel(): boolean {
  return level.ms > 0;
}

/** Whether the loop may run at all (it still rests after REST_AFTER_MS without input). */
function canRun(): boolean {
  return !!ctx && !reducedMotion() && !isPaused() && !isIdle() && !document.hidden;
}

function shouldRun(): boolean {
  return canRun() && (performance.now() - lastInput < REST_AFTER_MS || animatingLevel());
}

function wake(): void {
  // A fade that cannot play (paused, idle, reduced) lands on its end at once: one still frame.
  if (!canRun() && level.ms > 0) {
    level.v = level.from = level.to;
    level.ms = 0;
    draw();
  }
  if (raf || !shouldRun()) return;
  last = 0;
  raf = requestAnimationFrame(tick);
}

function tick(now: number): void {
  raf = 0;
  if (!shouldRun()) return;
  raf = requestAnimationFrame(tick);
  // The sky holds still while a view transition plays: the transition is the motion that matters
  // then, and the GPU has the snapshots to blend.
  if (transitionRunning()) {
    last = now;
    return;
  }
  if (last && now - last < 1000 / FPS - 2) return;
  const dt = last ? Math.min(0.1, (now - last) / 1000) : 1 / FPS;
  last = now;
  t += dt;

  // Ease the lean (time constant ~0.7 s): weightless, never a snap.
  const target = pinned ?? { x: lean.tx, y: lean.ty, w: lean.tw };
  const k = 1 - Math.exp(-dt / 0.7);
  lean.x += (target.x - lean.x) * k;
  lean.y += (target.y - lean.y) * k;
  lean.w += (target.w - lean.w) * k;

  if (level.ms > 0) {
    const p = Math.min(1, (now - level.start) / level.ms);
    const e = p < 0.5 ? 2 * p * p : 1 - Math.pow(-2 * p + 2, 2) / 2;
    level.v = level.from + (level.to - level.from) * e;
    if (p >= 1) level.ms = 0;
  }

  draw();
  if (now - lastRim > RIM_EVERY_MS) {
    lastRim = now;
    rims();
  }
}

interface Spot {
  x: number;
  y: number;
}

/** Where each ribbon's centre is right now (window fractions). */
function ribbonAt(r: Ribbon): Spot {
  const tau = Math.PI * 2;
  let x = r.x + r.ax * Math.sin((tau * t) / r.px + r.phase);
  let y = r.y + r.ay * Math.cos((tau * t) / r.py + r.phase * 1.3);
  const pull = r.lean * lean.w;
  x += (lean.x - x) * pull;
  y += (lean.y - y) * pull;
  return { x, y };
}

function draw(): void {
  if (!ctx) return;
  const g = ctx;
  const p = palette;
  g.globalCompositeOperation = "source-over";
  g.globalAlpha = 1;
  const sky = g.createLinearGradient(0, 0, 0, H);
  sky.addColorStop(0, p.sky[0]);
  sky.addColorStop(0.55, p.sky[1]);
  sky.addColorStop(1, p.sky[2]);
  g.fillStyle = sky;
  g.fillRect(0, 0, W, H);

  const lv = Math.max(0, Math.min(1.4, level.v));
  if (lv <= 0.001) return;
  g.globalCompositeOperation = p.blend;
  for (const r of p.ribbons) {
    const c = ribbonAt(r);
    ellipse(g, c.x * W, c.y * H, r.rx * W, r.ry * H, r.rot + Math.sin(t * r.spin) * 0.3, r.color, r.alpha * lv);
  }
  // The spotlight: a small soft light where the focus is.
  const sw = (pinned ? pinned.w : lean.w) * lv;
  if (sw > 0.01) ellipse(g, lean.x * W, lean.y * H, 0.2 * W, 0.16 * W, 0, p.spot, p.spotAlpha * sw);
  g.globalCompositeOperation = "source-over";
  clampLuminance(g, p === DAWN);
}

// ---- Contrast guard ------------------------------------------------------------------------------
// Text sits straight on the sky (the hero, screen heads, the footer), so the sky has hard limits:
// Night never gets brighter than SKY_MAX relative luminance, Dawn never darker than SKY_MIN. With
// the tokens in tokens.css that keeps --muted at 4.5:1 or better anywhere on the sky. It runs on
// the 160 px canvas (about 16 000 pixels), not on the window.

const SKY_MAX = 0.04;
const SKY_MIN = 0.66;
const toLin = new Float32Array(256);
for (let i = 0; i < 256; i++) {
  const c = i / 255;
  toLin[i] = c <= 0.04045 ? c / 12.92 : Math.pow((c + 0.055) / 1.055, 2.4);
}
const LIN_STEPS = 4096;
const toSrgb = new Uint8ClampedArray(LIN_STEPS + 1);
for (let i = 0; i <= LIN_STEPS; i++) {
  const l = i / LIN_STEPS;
  toSrgb[i] = Math.round(255 * (l <= 0.0031308 ? l * 12.92 : 1.055 * Math.pow(l, 1 / 2.4) - 0.055));
}

function clampLuminance(g: CanvasRenderingContext2D, light: boolean): void {
  let img: ImageData;
  try {
    img = g.getImageData(0, 0, W, H);
  } catch {
    return;
  }
  const d = img.data;
  let changed = false;
  for (let i = 0; i < d.length; i += 4) {
    const r = toLin[d[i]];
    const gg = toLin[d[i + 1]];
    const b = toLin[d[i + 2]];
    const L = 0.2126 * r + 0.7152 * gg + 0.0722 * b;
    if (!light && L > SKY_MAX) {
      const k = SKY_MAX / L;
      d[i] = toSrgb[Math.round(r * k * LIN_STEPS)];
      d[i + 1] = toSrgb[Math.round(gg * k * LIN_STEPS)];
      d[i + 2] = toSrgb[Math.round(b * k * LIN_STEPS)];
      changed = true;
    } else if (light && L < SKY_MIN) {
      const t = (SKY_MIN - L) / (1 - L);
      d[i] = toSrgb[Math.round((r + (1 - r) * t) * LIN_STEPS)];
      d[i + 1] = toSrgb[Math.round((gg + (1 - gg) * t) * LIN_STEPS)];
      d[i + 2] = toSrgb[Math.round((b + (1 - b) * t) * LIN_STEPS)];
      changed = true;
    }
  }
  if (changed) g.putImageData(img, 0, 0);
}

function ellipse(g: CanvasRenderingContext2D, x: number, y: number, rx: number, ry: number, rot: number, c: RGB, a: number): void {
  g.save();
  g.translate(x, y);
  g.rotate(rot);
  g.scale(Math.max(1, rx), Math.max(1, ry));
  const grad = g.createRadialGradient(0, 0, 0, 0, 0, 1);
  const rgb = `${c[0]}, ${c[1]}, ${c[2]}`;
  grad.addColorStop(0, `rgba(${rgb}, ${a.toFixed(3)})`);
  grad.addColorStop(0.42, `rgba(${rgb}, ${(a * 0.46).toFixed(3)})`);
  grad.addColorStop(0.75, `rgba(${rgb}, ${(a * 0.12).toFixed(3)})`);
  grad.addColorStop(1, `rgba(${rgb}, 0)`);
  g.fillStyle = grad;
  g.fillRect(-1, -1, 2, 2);
  g.restore();
}

// ---- Glass edges ---------------------------------------------------------------------------------
// Every .card / .glass in the screen has two edge rings (ember and violet, see components.css) whose
// opacity is --lit-a / --lit-b. They are set from the distance to the nearest ember and violet light,
// quantised to tenths, so a panel repaints its rim only when the light has really moved.

function rims(): void {
  const panels = document.querySelectorAll<HTMLElement>(".screen .card, .screen .glass");
  if (!panels.length) return;
  const vw = innerWidth;
  const vh = innerHeight;
  const reach = Math.max(vw, vh) * 0.62;
  const ember = ribbonAt(palette.ribbons[0]);
  const violet = ribbonAt(palette.ribbons[1]);
  const ex = ember.x * vw;
  const ey = ember.y * vh;
  const vx = violet.x * vw;
  const vy = violet.y * vh;
  for (const el of panels) {
    const r = el.getBoundingClientRect();
    if (r.bottom < 0 || r.top > vh || r.width < 4) continue;
    const a = lit(r, ex, ey, reach);
    const b = lit(r, vx, vy, reach);
    const key = `${a}|${b}`;
    if (rimSeen.get(el) === key) continue;
    rimSeen.set(el, key);
    el.style.setProperty("--lit-a", a);
    el.style.setProperty("--lit-b", b);
  }
}

function lit(r: DOMRect, x: number, y: number, reach: number): string {
  const dx = x < r.left ? r.left - x : x > r.right ? x - r.right : 0;
  const dy = y < r.top ? r.top - y : y > r.bottom ? y - r.bottom : 0;
  const d = Math.hypot(dx, dy);
  const v = Math.max(0, 1 - d / reach);
  const eased = 0.2 + 0.8 * v * v;
  return (Math.round(eased * 10) / 10).toFixed(1);
}

/** Recomputes the rims now (after a screen renders), whether or not the loop runs. */
export function refreshRims(): void {
  requestAnimationFrame(() => rims());
}
