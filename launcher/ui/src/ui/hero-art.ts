// The key art for Home and the launch stage: the brand drop as a sculpture of light, floating in the
// sky with no frame. Its speed streaks are long trails of light, its core burns white-gold, and the
// echo arcs (predicted frames) are rings of light ahead of it. Nothing in it depicts a game.
// Layers that move (glow, echoes, core) are separate elements, so they animate on the compositor.

import { h, nextId, svg } from "../dom";
import { DROP_D, echoD, LEAF_D } from "../icons";

const VIEW = "-120 0 210 64";

export function heroArt(opts: { cls?: string } = {}): HTMLElement {
  const id = nextId("art");
  const mark = svg(
    `<svg class="art-svg" viewBox="${VIEW}" preserveAspectRatio="xMaxYMid meet" aria-hidden="true" focusable="false">
      <defs>
        <linearGradient id="${id}-f" x1="22" y1="19" x2="48" y2="53" gradientUnits="userSpaceOnUse"><stop offset="0" stop-color="#FFD978"/><stop offset=".55" stop-color="#FFB040"/><stop offset="1" stop-color="#F2851A"/></linearGradient>
        <linearGradient id="${id}-s" x1="-118" y1="0" x2="22" y2="0" gradientUnits="userSpaceOnUse"><stop offset="0" stop-color="#FFE7B8" stop-opacity="0"/><stop offset=".62" stop-color="#FFD48A" stop-opacity=".22"/><stop offset="1" stop-color="#FFF1D6" stop-opacity=".95"/></linearGradient>
        <radialGradient id="${id}-c" cx="41" cy="31" r="15" gradientUnits="userSpaceOnUse"><stop offset="0" stop-color="#FFFDF5" stop-opacity=".95"/><stop offset=".5" stop-color="#FFF1CF" stop-opacity=".35"/><stop offset="1" stop-color="#FFF1CF" stop-opacity="0"/></radialGradient>
        <mask id="${id}-m" maskUnits="userSpaceOnUse" x="-130" y="-8" width="240" height="80"><rect x="-130" y="-8" width="240" height="80" fill="#fff"/><path d="M-4 29.2H29M-4 42.8H29" stroke="#000" stroke-width="3.2" stroke-linecap="round"/></mask>
        <clipPath id="${id}-d"><path d="${DROP_D}"/></clipPath>
      </defs>
      <g fill="none" stroke="url(#${id}-s)" stroke-linecap="round">
        <path d="M-116 29.2H16" stroke-width="3.6" stroke-opacity=".18"/>
        <path d="M-100 42.8H16" stroke-width="3.6" stroke-opacity=".18"/>
        <path d="M-116 29.2H16" stroke-width="1.1"/>
        <path d="M-84 36H6" stroke-width=".6"/>
        <path d="M-100 42.8H16" stroke-width="1.1"/>
      </g>
      <path d="${DROP_D}" fill="url(#${id}-f)" mask="url(#${id}-m)"/>
      <g class="art-core" clip-path="url(#${id}-d)" mask="url(#${id}-m)"><circle cx="41" cy="31" r="15" fill="url(#${id}-c)"/></g>
      <path d="${LEAF_D}" fill="#3CCF7E"/>
    </svg>`,
  );
  const echoes = [0, 1, 2].map((i) =>
    svg(
      `<svg class="art-echo art-echo-${i}" viewBox="${VIEW}" preserveAspectRatio="xMaxYMid meet" aria-hidden="true" focusable="false">
        <path d="${echoD(i * 7)}" fill="none" stroke="#FFC46A" stroke-opacity=".22" stroke-width="6" stroke-linecap="round"/>
        <path d="${echoD(i * 7)}" fill="none" stroke="#FFE6BC" stroke-width="1.6" stroke-linecap="round"/>
      </svg>`,
    ),
  );
  return h(
    "div",
    { class: `art ${opts.cls ?? ""}`, "aria-hidden": "true" },
    h("div", { class: "art-mark" }, h("div", { class: "art-glow" }), mark, h("div", { class: "art-echoes ambient" }, echoes)),
  );
}
