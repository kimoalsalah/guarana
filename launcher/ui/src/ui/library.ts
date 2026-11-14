// The game list under the hero, set as a table of record: index, title, format and size, Play.
// Loaded lazily when it scrolls near the viewport.

import { h } from "../dom";
import { bytes } from "../format";
import type { Game } from "../types";
import { button } from "./controls";

const CONTAINER: Record<Game["container"], string> = { nup: "NUP", nsp: "NSP", xci: "XCI", nx: "NX", other: "Game file" };

/** onPlay gets the row's title element, so the launch stage can morph from it. */
export function libraryList(games: Game[], opts: { canPlay: boolean; onPlay: (g: Game, title: HTMLElement) => void }): HTMLElement {
  return h(
    "ul",
    { class: "games" },
    games.map((g, i) => {
      const title = h("p", { class: "item-title game-title", text: g.title });
      return h(
        "li",
        { class: "game-row", style: { "--i": String(Math.min(i, 8)) } },
        h("span", { class: "game-n", "aria-hidden": "true", text: String(i + 1).padStart(2, "0") }),
        h("div", { class: "game-text" }, title),
        h("span", { class: "game-meta", text: CONTAINER[g.container] }),
        h("span", { class: "game-meta game-size", text: bytes(g.size_bytes) }),
        button("Play", {
          kind: "secondary",
          size: "sm",
          icon: "play",
          attrs: { "aria-label": `Play ${g.title}`, disabled: opts.canPlay ? undefined : true },
          onClick: () => opts.onPlay(g, title),
        }),
      );
    }),
  );
}
