async function api(path, opts = {}) {
  const res = await fetch(path, {
    headers: { "Content-Type": "application/json" },
    ...opts,
  });
  const text = await res.text();
  try {
    return JSON.parse(text);
  } catch {
    return { ok: false, error: text || `HTTP ${res.status}` };
  }
}

function setStatus(ok, msg) {
  const el = document.getElementById("statusPill");
  el.textContent = msg;
  el.style.borderColor = ok ? "rgba(120,255,180,0.35)" : "rgba(255,120,120,0.35)";
  el.style.background = ok ? "rgba(120,255,180,0.08)" : "rgba(255,120,120,0.08)";
  el.style.color = ok ? "#baf7d0" : "#ffd0d0";
}

function fillSelect(sel, items) {
  sel.innerHTML = "";
  for (const it of items) {
    const opt = document.createElement("option");
    opt.value = it;
    opt.textContent = it;
    sel.appendChild(opt);
  }
}

function metricValue(sel) {
  const v = sel.value;
  if (v === "distance") return 1;
  if (v === "time") return 2;
  return 3;
}

async function init() {
  const citiesRes = await api("/api/cities");
  if (!citiesRes.ok) {
    setStatus(false, "Backend not reachable");
    return;
  }
  setStatus(true, "Connected");

  const cities = citiesRes.cities || [];
  fillSelect(document.getElementById("src"), cities);
  fillSelect(document.getElementById("dst"), cities);
  if (cities.length >= 2) document.getElementById("dst").value = cities[cities.length - 1];

  document.getElementById("btnDijkstra").onclick = async () => {
    const metric = metricValue(document.getElementById("metric"));
    const src = document.getElementById("src").value;
    const dst = document.getElementById("dst").value;
    const out = document.getElementById("outDijkstra");
    out.textContent = "Computing…";
    const r = await api("/api/dijkstra", { method: "POST", body: JSON.stringify({ metric, src, dst }) });
    out.textContent = r.ok ? `Path: ${r.path}\nTotal: ${r.total}` : `Error: ${r.error}`;
  };

  document.getElementById("btnMst").onclick = async () => {
    const metric = metricValue(document.getElementById("mstMetric"));
    const out = document.getElementById("outMst");
    out.textContent = "Computing…";
    const r = await api("/api/mst", { method: "POST", body: JSON.stringify({ metric }) });
    if (!r.ok) {
      out.textContent = `Error: ${r.error}`;
      return;
    }
    out.textContent = `Total: ${r.total}\n` + (r.edges || []).map((e) => `- ${e}`).join("\n");
  };

  document.getElementById("btnRange").onclick = async () => {
    const l = Number(document.getElementById("rangeL").value);
    const r2 = Number(document.getElementById("rangeR").value);
    const out = document.getElementById("outToll");
    out.textContent = "Querying…";
    const r = await api(`/api/toll/range?l=${encodeURIComponent(l)}&r=${encodeURIComponent(r2)}`);
    out.textContent = r.ok ? `Sum[${r.l}..${r.r}] = ${r.sum}` : `Error: ${r.error}`;
  };

  document.getElementById("btnUpdate").onclick = async () => {
    const edgeId = Number(document.getElementById("updId").value);
    const newToll = Number(document.getElementById("updToll").value);
    const out = document.getElementById("outToll");
    out.textContent = "Updating…";
    const r = await api("/api/toll/update", { method: "POST", body: JSON.stringify({ edgeId, newToll }) });
    out.textContent = r.ok ? `Updated.\n${r.edge}` : `Error: ${r.error}`;
  };

  document.getElementById("btnBloom").onclick = async () => {
    const vehicleId = document.getElementById("vehId").value.trim();
    const out = document.getElementById("outBloom");
    out.textContent = "Checking…";
    const r = await api("/api/bloom", { method: "POST", body: JSON.stringify({ vehicleId }) });
    out.textContent = r.ok
      ? `Before: ${r.before}\nAfter:  ${r.after}\n(note: false positives possible)`
      : `Error: ${r.error}`;
  };

  document.getElementById("btnTopK").onclick = async () => {
    const k = Number(document.getElementById("topK").value);
    const out = document.getElementById("outTopK");
    out.textContent = "Computing…";
    const r = await api("/api/topk", { method: "POST", body: JSON.stringify({ k }) });
    out.textContent = r.ok ? (r.items || []).map((x) => x).join("\n") : `Error: ${r.error}`;
  };

  document.getElementById("btnBellman").onclick = async () => {
    const fuelSavings = Number(document.getElementById("fuelSavings").value);
    const out = document.getElementById("outBellman");
    out.textContent = "Checking…";
    const r = await api("/api/bellman", { method: "POST", body: JSON.stringify({ fuelSavings }) });
    out.textContent = r.ok ? r.note : `Error: ${r.error}`;
  };
}

init().catch((e) => setStatus(false, e?.message || "Init error"));

