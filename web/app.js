/**
 * 前端逻辑：SSE 实时推送 + 地图 + 轨迹 + PWA。
 */

const el = (id) => document.getElementById(id);

/* -------------------------------------------------------------- 地图 --- */

let map = null;
let marker = null;
let polyline = null;
let trackPath = [];
let focused = false;

function initMap() {
    if (typeof L === "undefined") {
        el("map-hint").textContent = "地图加载失败（无外网，无法加载地图瓦片）";
        return;
    }
    map = L.map("map").setView([20, 0], 2);
    L.tileLayer("https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png", {
        maxZoom: 19,
        attribution: "&copy; OpenStreetMap",
    }).addTo(map);
    marker = L.marker([0, 0]);
    polyline = L.polyline([], { color: "#4da3ff", weight: 4 }).addTo(map);
}

function updateMap(gnss) {
    if (!map || !gnss.fix || !gnss.latitude) {
        return;
    }
    const ll = [gnss.latitude, gnss.longitude];

    if (marker) {
        marker.setLatLng(ll);
        marker.addTo(map);
    }

    const last = trackPath[trackPath.length - 1];
    if (!last || last[0] !== ll[0] || last[1] !== ll[1]) {
        trackPath.push(ll);
        if (trackPath.length > 512) {
            trackPath.shift();
        }
        if (polyline) {
            polyline.setLatLngs(trackPath);
        }
    }

    if (!focused) {
        focused = true;
        map.setView(ll, 16);
    }
}

async function loadTrack() {
    try {
        const d = await fetchJson("/api/track");
        if (d.track && d.track.length) {
            trackPath = d.track.map((p) => [p.lat, p.lon]);
            if (polyline) {
                polyline.setLatLngs(trackPath);
            }
            const last = d.track[d.track.length - 1];
            if (map && last.lat) {
                focused = true;
                map.setView([last.lat, last.lon], 16);
            }
        }
    } catch (e) {
        console.error("loadTrack failed:", e.message);
    }
}

/* ------------------------------------------------------------ 渲染 --- */

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

    el("wifi-config").hidden = !!data.wifi_connected;
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

/* ---------------------------------------------------------- Wi-Fi 配置 --- */

async function saveWifi() {
    const ssid = el("ssid").value.trim();
    const password = el("password").value;
    const msg = el("wifi-msg");

    if (!ssid) {
        msg.textContent = "请输入 Wi-Fi 名称";
        return;
    }

    msg.textContent = "保存中...";
    try {
        const res = await fetch("/api/wifi", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({ ssid, password }),
        });
        if (!res.ok) throw new Error(`HTTP ${res.status}`);
        msg.textContent = "已保存，正在连接...";
    } catch (e) {
        msg.textContent = "保存失败: " + e.message;
        console.error("saveWifi failed:", e.message);
    }
}

/* ------------------------------------------------------------ SSE --- */

function connectSse() {
    const es = new EventSource("/api/events");

    es.onmessage = (e) => {
        try {
            const data = JSON.parse(e.data);
            renderStatus(data.status);
            renderGnss(data.gnss);
            setOnline(true);
            updateMap(data.gnss);
        } catch (err) {
            setOnline(false);
            console.error("sse parse failed:", err.message);
        }
    };

    es.onerror = () => {
        setOnline(false);
    };
}

/* ------------------------------------------------------------- 启动 --- */

initMap();
loadTrack();
connectSse();

if ("serviceWorker" in navigator) {
    navigator.serviceWorker.register("/sw.js").catch(() => {});
}
