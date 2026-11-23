// Wi-Fi setup page logic.
// Talks to:  GET /api/status   GET /api/scan   POST /connect
(function () {
  "use strict";

  const $ = (id) => document.getElementById(id);
  const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

  let busy = false; // true while a connect attempt is running

  // ---------- helpers ----------
  async function api(path, options) {
    const ctrl = new AbortController();
    const timer = setTimeout(() => ctrl.abort(), 6000);
    try {
      const res = await fetch(path, Object.assign({ cache: "no-store", signal: ctrl.signal }, options));
      return await res.json();
    } finally {
      clearTimeout(timer);
    }
  }

  function showMessage(text, type) {
    const box = $("message");
    box.textContent = text;          // textContent: never HTML
    box.className = "alert " + type;
  }

  function hideMessage() {
    $("message").className = "alert hidden";
  }

  // ---------- status ----------
  function setStatus(d) {
    const online = d.connected;
    $("pillDot").className = "dot " + (online ? "ok" : d.connecting ? "warn" : "bad");
    $("pillText").textContent = online ? "Connected: " + d.ssid : d.connecting ? "Connecting..." : "Not connected";

    $("connState").textContent = online ? "Connected" : d.connecting ? "Connecting..." : "Disconnected";
    $("connState").className = online ? "ok-text" : d.connecting ? "" : "bad-text";
    $("ssidNow").textContent = online ? d.ssid : "-";
    $("staIP").textContent = online ? d.staIP : "-";
    $("rssi").textContent = online ? d.rssi + " dBm" : "-";
    $("apIP").textContent = d.apIP || "-";
  }

  async function refreshStatus() {
    try {
      const d = await api("/api/status");
      setStatus(d);
      return d;
    } catch (e) {
      return null; // device unreachable
    }
  }

  // ---------- scan ----------
  function addNote(box, text) {
    const p = document.createElement("p");
    p.className = "muted small";
    p.textContent = text;
    box.appendChild(p);
  }

  function renderNetworks(list, note) {
    const box = $("networkList");
    box.replaceChildren();

    if (list === null) { addNote(box, "Scanning..."); return; }

    // drop hidden names, keep the strongest signal per name
    const best = new Map();
    list.forEach((n) => {
      if (!n.ssid) return;
      if (!best.has(n.ssid) || n.rssi > best.get(n.ssid).rssi) best.set(n.ssid, n);
    });
    const nets = Array.from(best.values()).sort((a, b) => b.rssi - a.rssi);

    if (nets.length === 0) { addNote(box, note || "No networks found."); return; }

    nets.forEach((n) => {
      const b = document.createElement("button");
      b.type = "button";
      b.className = "network";

      const name = document.createElement("span");
      name.textContent = n.ssid;

      const meta = document.createElement("span");
      meta.className = "muted mono small";
      meta.textContent = n.rssi + " dBm" + (n.secure ? "" : " (open)");

      b.append(name, meta);
      b.addEventListener("click", () => {
        $("ssid").value = n.ssid;
        $("password").focus();
      });
      box.appendChild(b);
    });
  }

  async function scan() {
    const btn = $("scanBtn");
    btn.disabled = true;
    renderNetworks(null);
    try {
      for (let i = 0; i < 10; i++) {
        const d = await api("/api/scan");
        if (!d.scanning) { renderNetworks(d.networks); return; }
        await sleep(1000);
      }
      renderNetworks([], "Scan took too long. Try again.");
    } catch (e) {
      renderNetworks([], "Could not scan. Try again.");
    } finally {
      btn.disabled = false;
    }
  }

  // ---------- connect ----------
  async function waitForConnection() {
    showMessage("Connecting to the router...", "info");
    for (let i = 0; i < 15; i++) {
      await sleep(2000);
      const d = await refreshStatus();

      if (!d) {
        // The ESP8266 can change channel while joining, which briefly drops this page.
        showMessage("Device is switching channels. If this page stops responding, reconnect to ESP8266_Setup and reload.", "info");
        continue;
      }
      if (d.connected) {
        showMessage("Connected to " + d.ssid + ". Device IP: " + d.staIP, "success");
        return;
      }
      if (!d.connecting && d.error) {
        showMessage(d.error, "error");
        return;
      }
    }
    showMessage("No answer from the device. Check the name and password.", "error");
  }

  async function onSubmit(ev) {
    ev.preventDefault();
    if (busy) return;
    busy = true;

    const btn = $("submitBtn");
    btn.disabled = true;
    btn.textContent = "Connecting...";
    showMessage("Sending credentials...", "info");

    try {
      let reply;
      try {
        reply = await api("/connect", {
          method: "POST",
          headers: { "Content-Type": "application/x-www-form-urlencoded" },
          body: new URLSearchParams({
            ssid: $("ssid").value.trim(),
            password: $("password").value
          })
        });
      } catch (e) {
        reply = { success: true }; // no reply: the device may already be switching channel
      }

      if (!reply.success) {
        showMessage(reply.message || "Request rejected.", "error");
        return;
      }
      await waitForConnection();
    } finally {
      busy = false;
      btn.disabled = false;
      btn.textContent = "Connect";
    }
  }

  // ---------- init ----------
  $("wifiForm").addEventListener("submit", onSubmit);
  $("scanBtn").addEventListener("click", scan);

  $("clearBtn").addEventListener("click", () => {
    $("wifiForm").reset();
    hideMessage();
  });

  $("togglePw").addEventListener("click", () => {
    const input = $("password");
    const show = input.type === "password";
    input.type = show ? "text" : "password";
    $("togglePw").textContent = show ? "Hide" : "Show";
  });

  refreshStatus();
  setInterval(() => { if (!busy) refreshStatus(); }, 3000);
})();
