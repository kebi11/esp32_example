/**
 * 简单 service worker：缓存静态资源，API/SSE 不拦截。
 * 用于 PWA 离线壳与"添加到主屏幕"。
 */

const CACHE = "gnss-terminal-v1";
const ASSETS = ["/", "/style.css", "/app.js", "/manifest.json", "/icon.svg"];

self.addEventListener("install", (e) => {
    e.waitUntil(
        caches.open(CACHE).then((c) => c.addAll(ASSETS)).then(() => self.skipWaiting())
    );
});

self.addEventListener("activate", (e) => {
    e.waitUntil(
        caches.keys()
            .then((keys) => Promise.all(keys.filter((k) => k !== CACHE).map((k) => caches.delete(k))))
            .then(() => self.clients.claim())
    );
});

self.addEventListener("fetch", (e) => {
    const url = new URL(e.request.url);
    // API 与 SSE 长连接不缓存、不拦截
    if (url.pathname.startsWith("/api/")) {
        return;
    }
    if (e.request.method !== "GET") {
        return;
    }
    e.respondWith(
        caches.match(e.request).then((r) => r || fetch(e.request))
    );
});
