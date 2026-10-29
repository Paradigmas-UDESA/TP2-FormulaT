// Formula T GUI (given code, not part of the assignment). A separate process:
// it only reads the files the C++ programs write, through gui/serve.py.
//   results/track.txt       the circuit (the given text format)
//   results/qualifying.bin  build/qualifying's results (clase 1's binary format)
//   results/live.bin        build/race's timing tower, every 100 ms
// and asks build/race for a red flag with POST /red-flag.
//
// PENDING: the instructor will rework this interface; the look is a placeholder.
//
// The binary files are raw bytes of the C++ types (little-endian, size_t = 8):
// the layouts below must match include/formulat/binary.h and the C++ structs.
//   header        char[4] magic, uint32 version (1)
//   vector<T>     uint64 size, then each T
//   CarResult     int32 team, int32 driver, uint8 finished, int64 total (ns),
//                 vector<SectionResult>                     (field by field)
//   SectionResult int32 outcome, int32 pitStop, int64 time (ns), float64 residual
//                 (24 bytes, copied whole)
//   live.bin      header "FTLV", int64 elapsed (ns since lights out, < 0 while
//                 the lights are on), vector<CarStatus>
//   CarStatus     int32 team, int32 driver, uint64 completed, int64 total,
//                 int32 state, uint32 position              (32 bytes, copied whole)

"use strict";

const TEAMS = ["Bisection", "Toms748", "Newton", "Halley", "Secant", "RegulaFalsi"];
const DRIVERS = ["Cold", "Warm"];
const STATES = ["Running", "Finished", "Retired", "Stopped"];
const TEAM_COLORS = ["#f5c518", "#00a19b", "#e10600", "#3671c6", "#ff8000", "#b6babd"];
const TYPE_COLORS = {
  Straight: "#6b7280", Hairpin: "#c2410c", Sweepers: "#0e7490", Chicane: "#a21caf", Kerb: "#65a30d",
};
// What the page shows for each state. Car names (team + driver) and section types
// stay as the C++ names, as the console, the track file and the ADR files have them.
const STATE_LABELS = { Running: "en pista", Finished: "terminó", Retired: "abandonó", Stopped: "detenido" };
const LIGHT_INTERVAL_S = 1;  // build/race's lightInterval
const REPLAY_SECONDS = 20;   // the slowest qualifying lap lasts this long at x1

const canvas = document.getElementById("track");
const ctx = canvas.getContext("2d");
const towerEl = document.getElementById("tower");
const controlsEl = document.getElementById("controls");
const messageEl = document.getElementById("message");

// --- reading the files ---------------------------------------------------------------

async function fetchFile(path, kind) {
  const response = await fetch(path, { cache: "no-store" });
  if (!response.ok) throw Object.assign(new Error(`no se encontró ${path}`), { missing: path });
  return kind === "text" ? response.text() : response.arrayBuffer();
}

function parseTrack(text) {
  const lines = text.split("\n").map((l) => l.trim());
  const value = (label) => {
    const line = lines.find((l) => l.startsWith(label + " "));
    if (!line) throw new Error(`a track.txt le falta ${label}`);
    return line.slice(label.length + 1);
  };
  const n = Number(value("layout"));
  const start = lines.findIndex((l) => l.startsWith("layout ")) + 1;
  const sections = lines.slice(start, start + n).map((l) => l.split(" ")[0]);
  return { name: JSON.parse(value("name")), laps: Number(value("laps")), sections };
}

class Reader {
  constructor(buffer) { this.view = new DataView(buffer); this.at = 0; }
  need(bytes) { if (this.at + bytes > this.view.byteLength) throw new Error("el archivo terminó antes de tiempo"); }
  magic(expected) {
    this.need(8);
    const got = String.fromCharCode(...new Uint8Array(this.view.buffer, 0, 4));
    this.at = 4;
    if (got !== expected) throw new Error(`no es un archivo ${expected}`);
    const version = this.u32();
    if (version !== 1) throw new Error(`versión ${version} no soportada`);
  }
  u8() { this.need(1); return this.view.getUint8(this.at++); }
  i32() { this.need(4); const v = this.view.getInt32(this.at, true); this.at += 4; return v; }
  u32() { this.need(4); const v = this.view.getUint32(this.at, true); this.at += 4; return v; }
  i64() { this.need(8); const v = Number(this.view.getBigInt64(this.at, true)); this.at += 8; return v; }
  u64() { this.need(8); const v = Number(this.view.getBigUint64(this.at, true)); this.at += 8; return v; }
  f64() { this.need(8); const v = this.view.getFloat64(this.at, true); this.at += 8; return v; }
  vector(readOne) { const n = this.u64(); return Array.from({ length: n }, () => readOne()); }
}

function parseQualifying(buffer) {
  const r = new Reader(buffer);
  r.magic("FTQB");
  return r.vector(() => ({
    team: TEAMS[r.i32()], driver: DRIVERS[r.i32()], finished: r.u8() !== 0, total: r.i64(),
    sections: r.vector(() => ({
      outcome: ["Clean", "Penalty", "DNF"][r.i32()], pitStop: r.i32() !== 0, time: r.i64(), residual: r.f64(),
    })),
  }));
}

function parseLive(buffer) {
  const r = new Reader(buffer);
  r.magic("FTLV");
  const elapsed = r.i64();
  const cars = r.vector(() => ({
    team: TEAMS[r.i32()], driver: DRIVERS[r.i32()], completed: r.u64(), total: r.i64(),
    state: STATES[r.i32()], position: r.u32(),
  }));
  return { elapsed, cars };
}

// --- drawing ---------------------------------------------------------------------------

// The circuit: a stadium (two straights, two half circles). pointAt(t), t in [0, 1).
const W = canvas.width, H = canvas.height, R = 190, STRAIGHT = W - 2 * R - 140;
const CX = W / 2, CY = H / 2 + 20;
const PERIMETER = 2 * STRAIGHT + 2 * Math.PI * R;

function pointAt(t, lane = 0) {
  let d = (((t % 1) + 1) % 1) * PERIMETER;
  const r = R + lane;
  if (d < STRAIGHT) return [CX - STRAIGHT / 2 + d, CY + r];  // bottom, left to right
  d -= STRAIGHT;
  if (d < Math.PI * R) { const a = Math.PI / 2 - d / R; return [CX + STRAIGHT / 2 + r * Math.cos(a), CY + r * Math.sin(a)]; }
  d -= Math.PI * R;
  if (d < STRAIGHT) return [CX + STRAIGHT / 2 - d, CY - r];  // top, right to left
  d -= STRAIGHT;
  const a = -Math.PI / 2 - d / R;
  return [CX - STRAIGHT / 2 + r * Math.cos(a), CY + r * Math.sin(a)];
}

function drawCircuit(track, title) {
  ctx.clearRect(0, 0, W, H);
  ctx.fillStyle = "#e8e8e8";
  ctx.font = "bold 20px system-ui";
  ctx.fillText(title, 20, 34);
  const n = track.sections.length;
  ctx.lineWidth = 34;
  ctx.lineCap = "butt";
  for (let i = 0; i < n; ++i) {
    ctx.strokeStyle = TYPE_COLORS[track.sections[i]] || "#555";
    ctx.beginPath();
    for (let s = 0; s <= 40; ++s) {
      const [x, y] = pointAt((i + s / 40) / n);
      s ? ctx.lineTo(x, y) : ctx.moveTo(x, y);
    }
    ctx.stroke();
  }
  // Section labels, and the start/finish line.
  ctx.font = "12px system-ui";
  ctx.fillStyle = "#e8e8e8";
  ctx.textAlign = "center";
  for (let i = 0; i < n; ++i) {
    const [x, y] = pointAt((i + 0.5) / n, -40);
    ctx.fillText(track.sections[i], x, y + 4);
  }
  ctx.textAlign = "left";
  const [x0, y0] = pointAt(0, -17), [x1, y1] = pointAt(0, 17);
  ctx.strokeStyle = "#fff";
  ctx.lineWidth = 3;
  ctx.beginPath(); ctx.moveTo(x0, y0); ctx.lineTo(x1, y1); ctx.stroke();
}

function carIndex(car) { return TEAMS.indexOf(car.team) * 2 + DRIVERS.indexOf(car.driver); }

// A car: its team's color, filled for Cold, a ring for Warm. Each car has its
// own lane so they don't hide each other.
function drawCar(car, t, crashed) {
  const i = carIndex(car);
  const [x, y] = pointAt(t, -14 + (i * 28) / 11);
  const color = TEAM_COLORS[TEAMS.indexOf(car.team)];
  ctx.lineWidth = 3;
  ctx.beginPath();
  ctx.arc(x, y, 6, 0, 2 * Math.PI);
  if (car.driver === "Cold") { ctx.fillStyle = color; ctx.fill(); }
  else { ctx.strokeStyle = color; ctx.stroke(); }
  if (crashed) {
    ctx.strokeStyle = "#fff";
    ctx.lineWidth = 2;
    ctx.beginPath();
    ctx.moveTo(x - 7, y - 7); ctx.lineTo(x + 7, y + 7);
    ctx.moveTo(x + 7, y - 7); ctx.lineTo(x - 7, y + 7);
    ctx.stroke();
  }
}

function drawLights(lit) {
  for (let i = 0; i < 5; ++i) {
    ctx.beginPath();
    ctx.arc(CX - 120 + i * 60, CY, 22, 0, 2 * Math.PI);
    ctx.fillStyle = i < lit ? "#e10600" : "#333";
    ctx.fill();
  }
}

function drawBanner(text) {
  ctx.fillStyle = "#e10600";
  ctx.fillRect(CX - 170, CY - 30, 340, 60);
  ctx.fillStyle = "#fff";
  ctx.font = "bold 28px system-ui";
  ctx.textAlign = "center";
  ctx.fillText(text, CX, CY + 10);
  ctx.textAlign = "left";
}

function swatch(car) {
  const color = TEAM_COLORS[TEAMS.indexOf(car.team)];
  const style = car.driver === "Cold" ? `background:${color}` : `border:2px solid ${color}`;
  return `<span class="swatch" style="${style}"></span>${car.team} ${car.driver}`;
}

function table(head, rows) {
  towerEl.innerHTML = `<tr>${head.map((h) => `<th>${h}</th>`).join("")}</tr>` + rows.join("");
}

// --- qualifying replay ---------------------------------------------------------------

const replay = { track: null, cars: null, clock: 0, playing: false, speed: 1, last: 0, scale: 1, gen: 0 };

async function startQualifying() {
  controlsEl.innerHTML = `<button id="play">Pausa</button> <button id="restart">Reiniciar</button>
    <select id="speed"><option value="0.25">x0.25</option><option value="1" selected>x1</option>
    <option value="4">x4</option></select>`;
  document.getElementById("play").onclick = (e) => {
    replay.playing = !replay.playing;
    e.target.textContent = replay.playing ? "Pausa" : "Reproducir";
  };
  document.getElementById("restart").onclick = () => { replay.clock = 0; };
  document.getElementById("speed").onchange = (e) => { replay.speed = Number(e.target.value); };
  try {
    replay.track = parseTrack(await fetchFile("/results/track.txt", "text"));
    replay.cars = parseQualifying(await fetchFile("/results/qualifying.bin"));
  } catch (err) {
    messageEl.textContent = `${err.message}: corran make qualifying y recarguen la página.`;
    return;
  }
  // Scale: the slowest lap (a DNF counts up to its crash) lasts REPLAY_SECONDS.
  const slowest = Math.max(...replay.cars.map((c) => c.sections.reduce((s, x) => s + x.time, 0)));
  replay.scale = slowest / REPLAY_SECONDS;  // ns of race per second of replay
  replay.clock = 0;
  replay.playing = true;
  messageEl.textContent = "Una vuelta por auto: en la clasificación real, uno después del otro; acá, todos a la vez.";
  replay.last = performance.now();
  const gen = ++replay.gen;  // one animation loop at a time, even after switching tabs
  requestAnimationFrame((now) => frameQualifying(now, gen));
}

// Where a car is at race time `ns`: sections done plus the fraction of the current one.
function qualifyingPosition(car, ns) {
  let t = 0;
  for (let k = 0; k < car.sections.length; ++k) {
    const time = car.sections[k].time;
    if (ns < t + time) return { at: k + (ns - t) / time, done: false };
    t += time;
  }
  return { at: car.sections.length, done: true };
}

function frameQualifying(now, gen) {
  if (mode !== "qualifying" || gen !== replay.gen) return;
  const dt = (now - replay.last) / 1000;
  replay.last = now;
  if (replay.playing) replay.clock += dt * replay.speed * replay.scale;

  const n = replay.track.sections.length;
  drawCircuit(replay.track, `${replay.track.name}: clasificación`);
  const rows = [];
  replay.cars.forEach((car, pos) => {
    const p = qualifyingPosition(car, replay.clock);
    const crashed = !car.finished && p.done;
    const at = crashed ? p.at - 0.5 : Math.min(p.at, n);
    drawCar(car, at / n, crashed);
    const pits = car.sections.filter((s) => s.pitStop).length;
    const penalties = car.sections.filter((s) => s.outcome === "Penalty").length;
    let time = "";
    if (p.done) time = car.finished ? (car.total / 1e6).toFixed(3) : "DNF";
    rows.push(`<tr class="${p.done ? "" : "out"}"><td class="num">${car.finished ? pos + 1 : "-"}</td>
      <td>${swatch(car)}</td><td class="num">${time}</td><td class="num">${pits}</td>
      <td class="num">${penalties}</td></tr>`);
  });
  table(["", "auto (orden final)", "tiempo (ms)", "boxes", "pen."], rows);
  requestAnimationFrame((next) => frameQualifying(next, gen));
}

// --- live race -------------------------------------------------------------------------

const live = { track: null, timer: null };

async function startLive() {
  controlsEl.innerHTML = `<button id="flag" class="flag">Bandera roja (R)</button>`;
  document.getElementById("flag").onclick = redFlag;
  live.track = null;
  messageEl.textContent = "Esperando a build/race...";
  live.timer = setInterval(pollLive, 100);
}

async function redFlag() {
  await fetch("/red-flag", { method: "POST" });
  messageEl.textContent = "Bandera roja pedida.";
}

async function pollLive() {
  let state;
  try {
    if (!live.track) live.track = parseTrack(await fetchFile("/results/track.txt", "text"));
    state = parseLive(await fetchFile("/results/live.bin"));
  } catch (err) {
    if (mode !== "live") return;  // the tab changed while this poll was waiting
    if (err.missing) {  // build/race hasn't run: before clase 4, it can't
      ctx.clearRect(0, 0, W, H);
      ctx.fillStyle = "#8a8f99";
      ctx.font = "bold 28px system-ui";
      ctx.textAlign = "center";
      ctx.fillText("La carrera en vivo llega en la clase 4", CX, CY);
      ctx.textAlign = "left";
      messageEl.textContent =
        `${err.message}. Cuando completen la clase 4, corran make race y esta vista se actualiza sola.`;
    } else {
      messageEl.textContent = `${err.message}: esperando a build/race...`;
    }
    return;
  }
  if (mode !== "live") return;
  const n = live.track.sections.length, laps = live.track.laps;
  drawCircuit(live.track, `${live.track.name}: carrera`);
  const seconds = state.elapsed / 1e9;

  // Running order: finishers by position, then by sections done, retired last.
  const rank = (c) => (c.state === "Finished" ? 0 : c.state === "Retired" ? 2 : 1);
  const order = [...state.cars].sort((a, b) =>
    rank(a) - rank(b) || (rank(a) === 0 ? a.position - b.position : b.completed - a.completed || a.total - b.total));

  for (const car of state.cars) {
    const t = car.state === "Finished" ? 0 : (car.completed % n) / n;
    drawCar(car, t, car.state === "Retired");
  }
  if (seconds < 0) drawLights(Math.min(5, Math.max(1, 6 + Math.floor(seconds / LIGHT_INTERVAL_S))));
  if (state.cars.some((c) => c.state === "Stopped")) drawBanner("BANDERA ROJA");  // any car stopped

  const rows = order.map((car, i) => {
    const lap = Math.min(Math.floor(car.completed / n) + (car.state === "Running" ? 1 : 0), laps);
    const cls = car.state === "Retired" ? "out" : "";
    return `<tr class="${cls}"><td class="num">${car.state === "Retired" ? "-" : i + 1}</td>
      <td>${swatch(car)}</td><td class="num">${lap}/${laps}</td><td>${STATE_LABELS[car.state]}</td>
      <td class="num">${car.position || ""}</td></tr>`;
  });
  table(["", "auto", "vuelta", "estado", "llegó"], rows);
  const running = state.cars.some((c) => c.state === "Running");
  const stopped = state.cars.some((c) => c.state === "Stopped");
  messageEl.textContent = seconds < 0 ? "Semáforo..." :
    running ? `${seconds.toFixed(1)} s desde la largada` :
    stopped ? "Detenida por bandera roja: relancenla con build/race --resume results/red-flag.bin" :
    "Carrera terminada (así quedó la última carrera).";
}

// --- tabs ------------------------------------------------------------------------------

let mode = null;

function show(next) {
  if (mode === next) return;
  mode = next;
  clearInterval(live.timer);
  document.getElementById("tab-qualifying").classList.toggle("active", mode === "qualifying");
  document.getElementById("tab-live").classList.toggle("active", mode === "live");
  towerEl.innerHTML = "";
  messageEl.textContent = "";
  ctx.clearRect(0, 0, W, H);
  (mode === "qualifying" ? startQualifying : startLive)();
}

document.getElementById("tab-qualifying").onclick = () => show("qualifying");
document.getElementById("tab-live").onclick = () => show("live");
document.addEventListener("keydown", (e) => {
  if (mode === "live" && (e.key === "r" || e.key === "R")) redFlag();
});
show("qualifying");
