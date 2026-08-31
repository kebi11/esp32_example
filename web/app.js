/**
 * 第一阶段使用 HTTP Polling：每 1 秒请求 /api/status 与 /api/gnss。
 * 第二阶段再考虑 SSE，第三阶段 WebSocket。
 */

const POLL_INTERVAL_MS = 1000;

const el = (id) => document.getElementById(id);

function formatUptime(seconds) {
    if (typeof seconds !== "number") return "--";
    const h = String(Math.floor(seconds / 3600)).padStart(2, "0");
    const m = String(Math.floor((seconds % 3600) / 60)).padStart(2, "0");
    const s = String(seconds % 60).padStart(2, "0");
    return `${h}:${m}:${s}`;
}

async function fetchJson(url) {
    const res = await fetch(url, { cache: "no-store" });
    if (!res.ok) throw new Error(`${url} -> ${res.status}`);
    return res.json();
}

function renderStatus(data) {
    el("device").textContent = data.device ?? "--";
    el("ip").textContent = data.ip ?? "--";
    el("rssi").textContent = data.wifi_rssi !== undefined ? `${data.wifi_rssi} dBm` : "--";
    el("uptime").textContent = formatUptime(data.uptime);
    el("heap").textContent = data.free_heap !== undefined
        ? `${Math.round(data.free_heap / 1024)} KB`
        : "--";
}

function renderGnss(data) {
    el("fix").textContent = data.fix ? "已定位" : "未定位";
    el("fix").style.color = data.fix ? "var(--ok)" : "var(--err)";
    el("satellites").textContent = data.satellites ?? "--";
    el("lat").textContent = data.latitude?.toFixed(6) ?? "--";
    el("lon").textContent = data.longitude?.toFixed(6) ?? "--";
    el("alt").textContent = data.altitude !== undefined ? `${data.altitude} m` : "--";
    el("speed").textContent = data.speed !== undefined ? `${data.speed} km/h` : "--";
    el("utc").textContent = data.utc ? data.utc.replace("T", " ").replace("Z", "") : "--";
}

function setOnline(online) {
    const badge = el("conn-state");
    badge.className = `badge ${online ? "online" : "offline"}`;
    badge.textContent = online ? "已连接" : "断开";
}

async function refresh() {
    try {
        const [status, gnss] = await Promise.all([
            fetchJson("/api/status"),
            fetchJson("/api/gnss"),
        ]);
        renderStatus(status);
        renderGnss(gnss);
        setOnline(true);
    } catch (e) {
        setOnline(false);
        console.error("refresh failed:", e.message);
    }
}

refresh();
setInterval(refresh, POLL_INTERVAL_MS);
