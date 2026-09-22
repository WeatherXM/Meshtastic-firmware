#pragma once

#if defined(WG1200) || defined(HAS_WEATHERXM)

static const char WEATHER_DASHBOARD_HTML[] = R"rawliteral(<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>WeatherXM WG1200 - Meshtastic Node</title>
  <style>
    :root {
      --bg: #0b0f19;
      --card: #161e2e;
      --card-border: #232f48;
      --text: #e2e8f0;
      --text-muted: #94a3b8;
      --accent: #38bdf8;
      --accent-subtle: #0284c7;
      --green: #22c55e;
      --yellow: #eab308;
      --font: system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
    }
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      background-color: var(--bg);
      color: var(--text);
      font-family: var(--font);
      padding: 1.5rem;
      min-height: 100vh;
      display: flex;
      flex-direction: column;
      align-items: center;
    }
    .container {
      width: 100%;
      max-width: 900px;
    }
    header {
      display: flex;
      justify-content: space-between;
      align-items: center;
      margin-bottom: 1.5rem;
      padding-bottom: 1rem;
      border-bottom: 1px solid var(--card-border);
      flex-wrap: wrap;
      gap: 1rem;
    }
    .title-group h1 {
      font-size: 1.5rem;
      font-weight: 700;
      color: var(--accent);
      display: flex;
      align-items: center;
      gap: 0.5rem;
    }
    .title-group p {
      font-size: 0.85rem;
      color: var(--text-muted);
    }
    .actions {
      display: flex;
      gap: 0.5rem;
      align-items: center;
    }
    button, a.btn {
      background: var(--card);
      border: 1px solid var(--card-border);
      color: var(--text);
      padding: 0.4rem 0.8rem;
      border-radius: 6px;
      font-size: 0.85rem;
      cursor: pointer;
      text-decoration: none;
      transition: all 0.2s;
    }
    button:hover, a.btn:hover {
      background: var(--card-border);
      border-color: var(--accent);
    }
    .badge {
      display: inline-flex;
      align-items: center;
      gap: 0.35rem;
      font-size: 0.75rem;
      padding: 0.2rem 0.5rem;
      border-radius: 9999px;
      background: rgba(34, 197, 94, 0.15);
      color: var(--green);
      border: 1px solid rgba(34, 197, 94, 0.3);
    }
    .badge.pulse::before {
      content: "";
      width: 6px;
      height: 6px;
      border-radius: 50%;
      background: currentColor;
    }
    .grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(260px, 1fr));
      gap: 1.25rem;
      margin-bottom: 1.5rem;
    }
    .card {
      background: var(--card);
      border: 1px solid var(--card-border);
      border-radius: 12px;
      padding: 1.25rem;
      display: flex;
      flex-direction: column;
      gap: 0.5rem;
      box-shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.1);
    }
    .card-title {
      font-size: 0.8rem;
      text-transform: uppercase;
      letter-spacing: 0.05em;
      color: var(--text-muted);
      display: flex;
      justify-content: space-between;
      align-items: center;
    }
    .card-value {
      font-size: 2.2rem;
      font-weight: 700;
      color: #fff;
    }
    .card-value small {
      font-size: 1rem;
      font-weight: 400;
      color: var(--text-muted);
      margin-left: 0.25rem;
    }
    .card-sub {
      font-size: 0.85rem;
      color: var(--text-muted);
      display: flex;
      justify-content: space-between;
      border-top: 1px solid rgba(255, 255, 255, 0.05);
      padding-top: 0.5rem;
      margin-top: 0.25rem;
    }
    footer {
      text-align: center;
      font-size: 0.8rem;
      color: var(--text-muted);
      margin-top: auto;
      padding-top: 1rem;
      border-top: 1px solid var(--card-border);
      width: 100%;
      max-width: 900px;
    }
    footer a { color: var(--accent); text-decoration: none; }
  </style>
</head>
<body>
  <div class="container">
    <header>
      <div class="title-group">
        <h1>WeatherXM WG1200</h1>
        <p>Meshtastic Node & Weather Station Gateway</p>
      </div>
      <div class="actions">
        <span id="status-badge" class="badge pulse">Online</span>
        <button id="unit-toggle" onclick="toggleUnits()">Switch to Imperial</button>
        <a href="/" class="btn">Meshtastic Web UI</a>
      </div>
    </header>

    <div class="grid">
      <div class="card">
        <div class="card-title">Temperature <span>🌡️</span></div>
        <div class="card-value" id="val-temp">--<small>°C</small></div>
        <div class="card-sub">
          <span>Feels like: <strong id="val-feels">--</strong></span>
          <span>Dew point: <strong id="val-dew">--</strong></span>
        </div>
      </div>

      <div class="card">
        <div class="card-title">Barometric Pressure <span>⏱️</span></div>
        <div class="card-value" id="val-press">--<small>hPa</small></div>
        <div class="card-sub">
          <span>Sensor: <strong>BMP390 Onboard</strong></span>
          <span id="val-trend">Stable</span>
        </div>
      </div>

      <div class="card">
        <div class="card-title">Humidity <span>💧</span></div>
        <div class="card-value" id="val-hum">--<small>%</small></div>
        <div class="card-sub">
          <span>Min: <strong id="val-hum-min">--</strong></span>
          <span>Max: <strong id="val-hum-max">--</strong></span>
        </div>
      </div>

      <div class="card">
        <div class="card-title">Wind Speed & Direction <span>💨</span></div>
        <div class="card-value" id="val-wind">--<small>m/s</small></div>
        <div class="card-sub">
          <span>Direction: <strong id="val-wind-dir">--</strong></span>
          <span>Gust: <strong id="val-wind-gust">--</strong></span>
        </div>
      </div>

      <div class="card">
        <div class="card-title">Precipitation <span>🌧️</span></div>
        <div class="card-value" id="val-rain">--<small>mm</small></div>
        <div class="card-sub">
          <span>Rate: <strong id="val-rain-rate">--</strong></span>
          <span>Daily Total: <strong id="val-rain-cml">--</strong></span>
        </div>
      </div>

      <div class="card">
        <div class="card-title">Solar & UV <span>☀️</span></div>
        <div class="card-value" id="val-solar">--<small>W/m²</small></div>
        <div class="card-sub">
          <span>UV Index: <strong id="val-uv">--</strong></span>
          <span>Light: <strong id="val-lux">-- lux</strong></span>
        </div>
      </div>
    </div>

    <div class="card" style="margin-bottom: 1.5rem;">
      <div class="card-title">Station & Mesh Status</div>
      <div style="display: grid; grid-template-columns: repeat(auto-fit, minmax(180px, 1fr)); gap: 1rem; margin-top: 0.5rem; font-size: 0.85rem;">
        <div>Station: <strong id="val-station">WS1001/WS1300</strong></div>
        <div>Packets Received: <strong id="val-packets">0</strong></div>
        <div>Station RSSI: <strong id="val-rssi">-- dBm</strong></div>
        <div>Station SNR: <strong id="val-snr">-- dB</strong></div>
      </div>
    </div>

    <footer>
      <p>Meshtastic firmware for WeatherXM WG1200 | API endpoints: <a href="/api/v1/weather">/api/v1/weather</a> | <a href="/json/report">/json/report</a></p>
    </footer>
  </div>

  <script>
    let imperial = false;

    function toggleUnits() {
      imperial = !imperial;
      document.getElementById('unit-toggle').innerText = imperial ? "Switch to Metric" : "Switch to Imperial";
      fetchWeather();
    }

    async function fetchWeather() {
      try {
        const res = await fetch('/api/v1/weather');
        if (!res.ok) return;
        const data = await res.json();
        updateUI(data);
      } catch (e) {
        console.error("Failed to fetch weather data", e);
      }
    }

    function updateUI(d) {
      const cur = d.current || {};
      const obs = cur.observation || {};
      const dev = cur.device || {};

      const tempUnit = imperial ? "°F" : "°C";
      const pressUnit = imperial ? "inHg" : "hPa";
      const speedUnit = imperial ? "mph" : "m/s";
      const rainUnit = imperial ? "in" : "mm";

      if (obs.temperature !== undefined) {
        let t = obs.temperature;
        if (imperial) t = (t * 9/5) + 32;
        document.getElementById('val-temp').innerHTML = `${t.toFixed(1)}<small>${tempUnit}</small>`;
      }

      if (obs.feels_like !== undefined) {
        let fl = obs.feels_like;
        if (imperial) fl = (fl * 9/5) + 32;
        document.getElementById('val-feels').innerText = `${fl.toFixed(1)} ${tempUnit}`;
      }

      if (obs.dew_point !== undefined) {
        let dp = obs.dew_point;
        if (imperial) dp = (dp * 9/5) + 32;
        document.getElementById('val-dew').innerText = `${dp.toFixed(1)} ${tempUnit}`;
      }

      if (obs.pressure !== undefined) {
        let p = obs.pressure;
        if (imperial) p = p * 0.0295299875;
        document.getElementById('val-press').innerHTML = `${p.toFixed(imperial ? 2 : 1)}<small>${pressUnit}</small>`;
      }

      if (obs.humidity !== undefined) {
        document.getElementById('val-hum').innerHTML = `${Math.round(obs.humidity)}<small>%</small>`;
      }

      if (obs.wind_speed !== undefined) {
        let ws = obs.wind_speed;
        if (imperial) ws = ws * 2.23694;
        document.getElementById('val-wind').innerHTML = `${ws.toFixed(1)}<small>${speedUnit}</small>`;
      }

      if (obs.wind_direction !== undefined) {
        document.getElementById('val-wind-dir').innerText = `${obs.wind_direction}°`;
      }

      if (obs.wind_gust !== undefined) {
        let wg = obs.wind_gust;
        if (imperial) wg = wg * 2.23694;
        document.getElementById('val-wind-gust').innerText = `${wg.toFixed(1)} ${speedUnit}`;
      }

      if (obs.precipitation_rate !== undefined) {
        let pr = obs.precipitation_rate;
        if (imperial) pr = pr * 0.0393701;
        document.getElementById('val-rain').innerHTML = `${pr.toFixed(1)}<small>${rainUnit}/h</small>`;
        document.getElementById('val-rain-rate').innerText = `${pr.toFixed(1)} ${rainUnit}/h`;
      }

      if (obs.precipitation_accumulated !== undefined) {
        let pa = obs.precipitation_accumulated;
        if (imperial) pa = pa * 0.0393701;
        document.getElementById('val-rain-cml').innerText = `${pa.toFixed(1)} ${rainUnit}`;
      }

      if (obs.solar_irradiance !== undefined) {
        document.getElementById('val-solar').innerHTML = `${Math.round(obs.solar_irradiance)}<small>W/m²</small>`;
      }

      if (obs.uv_index !== undefined) {
        document.getElementById('val-uv').innerText = obs.uv_index.toFixed(1);
      }

      if (obs.illuminance !== undefined) {
        document.getElementById('val-lux').innerText = `${Math.round(obs.illuminance)} lux`;
      }

      if (dev.station_name) {
        document.getElementById('val-station').innerText = dev.station_name;
      }
      if (dev.rssi !== undefined) {
        document.getElementById('val-rssi').innerText = `${dev.rssi} dBm`;
      }
      if (dev.snr !== undefined) {
        document.getElementById('val-snr').innerText = `${dev.snr.toFixed(1)} dB`;
      }
    }

    fetchWeather();
    setInterval(fetchWeather, 5000);
  </script>
</body>
</html>
)rawliteral";

#endif
