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
// 2026 F1 team colours (formula1.com/en/teams): Aston Martin, Mercedes, Ferrari, Red Bull, McLaren, Cadillac.
const TEAM_COLORS = ["#229971", "#27f4d2", "#e8002d", "#3671c6", "#ff8000", "#aaaaad"];
const TYPE_COLORS = {
  Straight: "#3aa8dc", Hairpin: "#e8312a", Sweepers: "#f5c400", Chicane: "#e43be4", Kerb: "#57c13c",
};
// Car names (team + driver) and section types stay as the C++ names, as the console,
// the track file and the ADR files have them.
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
  magic(expected, wanted = 1) {
    this.need(8);
    const got = String.fromCharCode(...new Uint8Array(this.view.buffer, 0, 4));
    this.at = 4;
    if (got !== expected) throw new Error(`no es un archivo ${expected}`);
    const version = this.u32();
    if (version !== wanted) throw new Error(`versión ${version} no soportada`);
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

// The cars racing, then (after a restart) those out since before it: finished or
// retired, with no race time (out: true).
function parseLive(buffer) {
  const r = new Reader(buffer);
  r.magic("FTLV", 2);
  const elapsed = r.i64();
  const car = (out) => () => ({
    team: TEAMS[r.i32()], driver: DRIVERS[r.i32()], completed: r.u64(), total: r.i64(),
    state: STATES[r.i32()], position: r.u32(), out,
  });
  const cars = r.vector(car(false));
  return { elapsed, cars, out: r.vector(car(true)) };
}

// --- drawing ---------------------------------------------------------------------------

// The circuit: a stadium (two straights, two half circles). pointAt(t), t in [0, 1).
const W = canvas.width, H = canvas.height, R = 190, STRAIGHT = W - 2 * R - 140;
const CX = W / 2, CY = H / 2 + 20;
const TRACK_WIDTH = 44;  // the cars' lanes (-14 to 14) and their size (6) fit inside
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

// A line along the circuit, from t0 to t1, in a lane (0 is the middle; < 0 is inside).
function strokeAlong(t0, t1, lane, color, width) {
  ctx.strokeStyle = color;
  ctx.lineWidth = width;
  ctx.beginPath();
  const steps = Math.ceil((t1 - t0) * 400);
  for (let s = 0; s <= steps; ++s) {
    const [x, y] = pointAt(t0 + ((t1 - t0) * s) / steps, lane);
    s ? ctx.lineTo(x, y) : ctx.moveTo(x, y);
  }
  ctx.stroke();
}

function drawCircuit(track, title) {
  ctx.clearRect(0, 0, W, H);
  ctx.fillStyle = "#15151e";
  ctx.font = '20px "Russo One", system-ui';
  ctx.fillText(title, 20, 34);
  const n = track.sections.length;
  // Like F1's circuit maps: a dark track, each section's type as a colored line along
  // the middle, and the labels as dark pills.
  ctx.lineCap = "butt";
  strokeAlong(0, 1, 0, "#15151e", TRACK_WIDTH);
  for (let i = 0; i < n; ++i)
    strokeAlong(i / n, (i + 1) / n, 0, TYPE_COLORS[track.sections[i]] || "#888", 4);
  ctx.font = '13px "Titillium Web", system-ui';
  ctx.textAlign = "center";
  for (let i = 0; i < n; ++i) {
    const type = track.sections[i];
    const w = ctx.measureText(type).width + 30, h = 22;  // the dot, the text, padding
    // Inside the circuit, far enough that the pill never touches the track: half the
    // track, a gap, and half the pill measured across the track (it depends on the angle).
    const t = (i + 0.5) / n;
    const [mx, my] = pointAt(t), [ix, iy] = pointAt(t, -1);
    const [nx, ny] = [ix - mx, iy - my];
    const lane = -(TRACK_WIDTH / 2 + 12 + (Math.abs(nx) * w + Math.abs(ny) * h) / 2);
    const [x, y] = pointAt(t, lane);
    ctx.fillStyle = "#15151e";
    ctx.beginPath();
    ctx.roundRect(x - w / 2, y - h / 2, w, h, h / 2);
    ctx.fill();
    // White text, and the type's color as a dot: some colors are hard to read as text.
    ctx.fillStyle = TYPE_COLORS[type] || "#888";
    ctx.beginPath();
    ctx.arc(x - w / 2 + 12, y, 4, 0, 2 * Math.PI);
    ctx.fill();
    ctx.fillStyle = "#fff";
    ctx.fillText(type, x + 7, y + 4);
  }
  ctx.textAlign = "left";
  // The start/finish line: checkered, two rows of squares across the track, a bit wider
  // than it. Drawn turned with the track, so it is across it wherever t = 0 falls.
  const SQUARE = 6, ACROSS = 8;  // 8 squares of 6 px: 48 px, the track is 44
  const [sx, sy] = pointAt(0), [ax, ay] = pointAt(0.001);
  ctx.save();
  ctx.translate(sx, sy);
  ctx.rotate(Math.atan2(ay - sy, ax - sx));
  for (let row = 0; row < 2; ++row)
    for (let k = 0; k < ACROSS; ++k) {
      ctx.fillStyle = (row + k) % 2 ? "#15151e" : "#fff";
      ctx.fillRect((row - 1) * SQUARE, (k - ACROSS / 2) * SQUARE, SQUARE, SQUARE);
    }
  ctx.strokeStyle = "#15151e";  // an outline, so the white squares show on the light panel
  ctx.lineWidth = 1;
  ctx.strokeRect(-SQUARE, (-ACROSS / 2) * SQUARE, 2 * SQUARE, ACROSS * SQUARE);
  ctx.restore();
}

function carIndex(car) { return TEAMS.indexOf(car.team) * 2 + DRIVERS.indexOf(car.driver); }

// Each car has its own lane on the track, so they don't hide each other.
function laneOf(car) { return -14 + (carIndex(car) * 28) / 11; }

// A car: its team's color, filled for Cold, a ring for Warm.
function drawCarAt(car, x, y) {
  const color = TEAM_COLORS[TEAMS.indexOf(car.team)];
  ctx.lineWidth = 3;
  ctx.beginPath();
  ctx.arc(x, y, 6, 0, 2 * Math.PI);
  if (car.driver === "Cold") { ctx.fillStyle = color; ctx.fill(); }
  else { ctx.strokeStyle = color; ctx.stroke(); }
}

function drawCross(x, y, color) {
  ctx.strokeStyle = color;
  ctx.lineWidth = 2;
  ctx.beginPath();
  ctx.moveTo(x - 7, y - 7); ctx.lineTo(x + 7, y + 7);
  ctx.moveTo(x + 7, y - 7); ctx.lineTo(x - 7, y + 7);
  ctx.stroke();
}

function drawCar(car, t) { drawCarAt(car, ...pointAt(t, laneOf(car))); }

// Where a car crashed: a grey X on its lane, the car itself is in the infield.
function drawCrash(car, t) { drawCross(...pointAt(t, laneOf(car)), "#8a8f99"); }

// Off the track, in the infield below the middle (the lights and the red flag banner
// use the middle): the cars that finished, in order and with their position, and
// those that retired, each with an X. A row is drawn only once it has a car.
function drawInfield(finished, retired) {
  const rows = [["Llegaron", finished], ["Abandonaron", retired]];
  rows.forEach(([label, cars], r) => {
    if (!cars.length) return;
    const y = CY + 60 + r * 46, x0 = CX - 220;
    ctx.fillStyle = "#15151e";
    ctx.font = '13px "Titillium Web", system-ui';
    ctx.fillText(label, x0, y + 4);
    ctx.textAlign = "center";
    cars.forEach((car, k) => {
      const x = x0 + 110 + k * 28;
      drawCarAt(car, x, y);
      if (r === 0) { ctx.fillStyle = "#15151e"; ctx.fillText(k + 1, x, y + 22); }
      else drawCross(x, y, "#15151e");
    });
    ctx.textAlign = "left";
  });
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
  ctx.font = '28px "Russo One", system-ui';
  ctx.textAlign = "center";
  ctx.fillText(text, CX, CY + 10);
  ctx.textAlign = "left";
}

function swatch(car) {
  const color = TEAM_COLORS[TEAMS.indexOf(car.team)];
  const style = car.driver === "Cold" ? `background:${color}` : `border:2px solid ${color}`;
  return `<span class="swatch" style="${style}"></span>${car.team} ${car.driver}`;
}

// Times like F1's tower: the leader's in full, everyone else's as a gap to it.
function gap(time, leaderTime, unit, digits = 3) {
  if (time === leaderTime) return (time / unit).toFixed(digits);
  // Negative is possible in the race: race time is each car's own clock, and a car can
  // be behind on track with less time on it.
  const d = (time - leaderTime) / unit;
  return `${d < 0 ? "-" : "+"}${Math.abs(d).toFixed(digits)}`;
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
  const rows = [], finished = [], retired = [];
  replay.cars.forEach((car, pos) => {
    const p = qualifyingPosition(car, replay.clock);
    if (!p.done) drawCar(car, p.at / n);
    else if (car.finished) finished.push(car);  // in order: cars[] is the final order
    else { drawCrash(car, (p.at - 0.5) / n); retired.push(car); }
    const pits = car.sections.filter((s) => s.pitStop).length;
    const penalties = car.sections.filter((s) => s.outcome === "Penalty").length;
    let time = "";  // shown once the lap is over; the order is the final one, so cars[0] is P1
    if (p.done) time = car.finished ? gap(car.total, replay.cars[0].total, 1e6) : "DNF";
    rows.push(`<tr class="${p.done ? "" : "out"}"><td class="num">${car.finished ? pos + 1 : "-"}</td>
      <td>${swatch(car)}</td><td class="num">${time}</td><td class="num">${pits}</td>
      <td class="num">${penalties}</td></tr>`);
  });
  drawInfield(finished, retired);
  table(["", "auto (orden final)", "tiempo (ms)", "boxes", "pen."], rows);
  requestAnimationFrame((next) => frameQualifying(next, gen));
}

// --- live race -------------------------------------------------------------------------

// history: per car, [sections completed, race time] at each poll, to know the leader's
// time at any car's distance (between two polls, interpolated).
const live = { track: null, timer: null, history: new Map() };

async function startLive() {
  controlsEl.innerHTML = `<button id="flag" class="flag">Bandera roja (R)</button>`;
  document.getElementById("flag").onclick = redFlag;
  live.track = null;
  live.history.clear();
  messageEl.textContent = "Esperando a build/race...";
  live.timer = setInterval(pollLive, 100);
}

async function redFlag() {
  if (document.getElementById("flag").disabled) {  // the R key, too
    messageEl.textContent = "Ya terminó un auto: la carrera tiene ganador, no hay bandera roja.";
    return;
  }
  await fetch("/red-flag", { method: "POST" });
  messageEl.textContent = "Bandera roja pedida.";
}

// The leader's race time when it had completed k sections (k <= its own), interpolated
// between the polls that saw it. Before the first poll that saw it, from 0.
function leaderTimeAt(leader, k) {
  const samples = live.history.get(carIndex(leader)) ?? [];
  let [k0, t0] = [0, 0];
  for (const [k1, t1] of samples) {
    if (k1 >= k) return k1 === k0 ? t1 : t0 + ((t1 - t0) * (k - k0)) / (k1 - k0);
    [k0, t0] = [k1, t1];
  }
  return leader.total;
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
      ctx.font = '28px "Russo One", system-ui';
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

  // Running order: finishers by position, then by sections done, retired last. The
  // cars out since before a restart too (they aren't on the track).
  const all = [...state.cars, ...state.out];
  const rank = (c) => (c.state === "Finished" ? 0 : c.state === "Retired" ? 2 : 1);
  const order = all.sort((a, b) =>
    rank(a) - rank(b) || (rank(a) === 0 ? a.position - b.position : b.completed - a.completed || a.total - b.total));

  for (const car of state.cars) {
    const samples = live.history.get(carIndex(car)) ?? [];
    const last = samples[samples.length - 1];
    if (last && car.completed < last[0]) live.history.clear();  // a new race started
    if (!last || car.completed > last[0]) {
      if (!live.history.has(carIndex(car))) live.history.set(carIndex(car), []);
      live.history.get(carIndex(car)).push([car.completed, car.total]);
    }
  }
  for (const car of state.cars) {
    const t = (car.completed % n) / n;
    if (car.state === "Retired") drawCrash(car, t);
    else if (car.state !== "Finished") drawCar(car, t);
  }
  // order has the finishers by position, then the running cars, then the retired.
  drawInfield(order.filter((c) => c.state === "Finished"), order.filter((c) => c.state === "Retired"));
  if (seconds < 0) drawLights(Math.min(5, Math.max(1, 6 + Math.floor(seconds / LIGHT_INTERVAL_S))));
  if (state.cars.some((c) => c.state === "Stopped")) drawBanner("BANDERA ROJA");  // any car stopped

  // Laps behind the first car; time gaps to the first one racing since the restart
  // (race times start again at 0 there: the cars out have none).
  const leader = order.find((c) => !c.out) ?? order[0];
  const rows = order.map((car, i) => {
    const lap = Math.min(Math.floor(car.completed / n) + (car.state === "Running" ? 1 : 0), laps);
    const cls = car.state === "Retired" ? "out" : "";
    const behind = Math.floor((order[0].completed - car.completed) / n);
    const time = car.state === "Retired" ? "DNF" : car.out ? "-" :
      behind > 0 ? `+${behind} ${behind === 1 ? "vuelta" : "vueltas"}` :
      gap(car.total, car === leader ? car.total : leaderTimeAt(leader, car.completed), 1e9);
    return `<tr class="${cls}"><td class="num">${car.state === "Retired" ? "-" : i + 1}</td>
      <td>${swatch(car)}</td><td class="num">${lap}/${laps}</td><td class="num">${time}</td>
      <td class="num">${car.position || ""}</td></tr>`;
  });
  table(["", "auto", "vuelta", "tiempo (s)", "llegó"], rows);
  const flag = document.getElementById("flag");
  flag.disabled = all.some((c) => c.state === "Finished");
  flag.title = flag.disabled ? "Ya terminó un auto: la carrera tiene ganador" : "";
  const running = state.cars.some((c) => c.state === "Running");
  const stopped = state.cars.some((c) => c.state === "Stopped");
  messageEl.textContent = seconds < 0 ? "Semáforo..." :
    running ? `${seconds.toFixed(1)} s desde la largada` :
    stopped ? "Detenida por bandera roja: relancenla con make resume" :
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
