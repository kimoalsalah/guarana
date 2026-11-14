// Last-played times. CONTRACT.md has no persisted play history, so the UI keeps its own from
// game://exited. It only knows sessions started from the launcher.

interface SessionRecord {
  last: string;
  seconds: number;
  total: number;
}

const KEY = "guarana.sessions";

function readAll(): Record<string, SessionRecord> {
  try {
    const raw = localStorage.getItem(KEY);
    const parsed: unknown = raw ? JSON.parse(raw) : {};
    return parsed && typeof parsed === "object" ? (parsed as Record<string, SessionRecord>) : {};
  } catch {
    return {};
  }
}

export function lastSession(titleId: string): SessionRecord | null {
  const r = readAll()[titleId];
  return r && typeof r.last === "string" ? r : null;
}

// The last measured frame pacing per game, from the share offer (the emulator's log of a finished
// session). Kept so Home's instrument still has the numbers after the offer is shared or dismissed.

const PACING_KEY = "guarana.pacing";

export interface PacingRecord {
  at: string;
  game_fps: number;
  within2: number;
  shown_fps: number;
  p50: number;
  p95: number;
  p99: number;
  gameplay_secs: number;
}

function readPacing(): Record<string, PacingRecord> {
  try {
    const raw = localStorage.getItem(PACING_KEY);
    const parsed: unknown = raw ? JSON.parse(raw) : {};
    return parsed && typeof parsed === "object" ? (parsed as Record<string, PacingRecord>) : {};
  } catch {
    return {};
  }
}

const num = (n: unknown) => typeof n === "number" && Number.isFinite(n);

export function lastPacing(key: string): PacingRecord | null {
  const r = readPacing()[key];
  return r && num(r.within2) && num(r.p50) && num(r.p95) && num(r.p99) && num(r.game_fps) ? r : null;
}

export function recordPacing(key: string, p: PacingRecord): void {
  const all = readPacing();
  const prev = all[key];
  if (prev && prev.at >= p.at) return;
  all[key] = { at: p.at, game_fps: p.game_fps, within2: p.within2, shown_fps: p.shown_fps, p50: p.p50, p95: p.p95, p99: p.p99, gameplay_secs: p.gameplay_secs };
  try {
    localStorage.setItem(PACING_KEY, JSON.stringify(all));
  } catch {
    /* the instrument shows the offer while it lasts */
  }
}

export function recordSession(titleId: string, seconds: number): void {
  const all = readAll();
  const prev = all[titleId];
  all[titleId] = {
    last: new Date().toISOString(),
    seconds: Math.max(0, Math.round(seconds)),
    total: (prev?.total ?? 0) + Math.max(0, Math.round(seconds)),
  };
  try {
    localStorage.setItem(KEY, JSON.stringify(all));
  } catch {
    /* storage full or disabled: the hero just shows no last-played line */
  }
}
