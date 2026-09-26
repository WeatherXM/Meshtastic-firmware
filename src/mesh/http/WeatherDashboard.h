#pragma once

#if defined(WG1200) || defined(HAS_WEATHERXM)

static const char WEATHER_DASHBOARD_HTML[] = R"rawliteral(<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Weather nodes · WG1200</title>
  <style>
    :root { color-scheme: dark; --bg:#0c141b; --panel:#14212b; --line:#293a46; --text:#ecf3f6; --muted:#a4b8c5; --accent:#87e2c1; }
    * { box-sizing:border-box; }
    body { margin:0; background:var(--bg); color:var(--text); font:15px/1.5 system-ui,-apple-system,sans-serif; }
    .shell { max-width:1220px; margin:auto; padding:32px; }
    header,.toolbar,.station-heading { display:flex; justify-content:space-between; align-items:center; gap:16px; flex-wrap:wrap; }
    header { padding-bottom:24px; border-bottom:1px solid var(--line); }
    h1,h2,p { margin:0; } h1 { font-size:25px; } h2 { font-size:23px; overflow-wrap:anywhere; }
    .eyebrow { color:var(--accent); font-size:11px; font-weight:700; letter-spacing:.16em; text-transform:uppercase; margin-bottom:7px; }
    .muted,small { color:var(--muted); } small { font-size:12px; }
    .toolbar { margin:24px 0; }
    .actions { display:flex; gap:8px; flex-wrap:wrap; }
    button,a { font:inherit; }
    button { border:1px solid var(--line); border-radius:8px; background:var(--panel); color:var(--text); padding:9px 13px; cursor:pointer; }
    button:hover { border-color:var(--accent); }
    button:focus-visible,a:focus-visible { outline:2px solid var(--accent); outline-offset:3px; }
    button:disabled { opacity:.5; cursor:default; }
    button[aria-pressed="true"] { background:#213d36; border-color:var(--accent); color:var(--accent); }
    #connection { color:var(--muted); font-size:13px; } #connection[data-ok="true"] { color:var(--accent); }
    .layout { display:grid; grid-template-columns:290px minmax(0,1fr); gap:24px; align-items:start; }
    aside { background:var(--panel); border:1px solid var(--line); border-radius:12px; overflow:hidden; }
    .list-heading { padding:18px; display:flex; justify-content:space-between; border-bottom:1px solid var(--line); }
    #nodes { max-height:65vh; overflow:auto; padding:8px; display:grid; gap:6px; }
    .node { width:100%; text-align:left; border-color:transparent; background:transparent; padding:13px; }
    .node strong,.node small { display:block; overflow-wrap:anywhere; }
    .node strong { margin-bottom:3px; } .node small:last-child { margin-top:6px; }
    .node[aria-pressed="true"] small { color:#bbd9ce; }
    .station-heading { margin:3px 0 22px; }
    #node-id { font:13px ui-monospace,monospace; color:var(--muted); margin-top:4px; }
    #updated { font-size:13px; color:var(--muted); margin-top:10px; }
    .tag { border:1px solid var(--line); padding:5px 10px; border-radius:20px; font-size:12px; color:var(--muted); }
    #metrics { display:grid; grid-template-columns:repeat(auto-fit,minmax(190px,1fr)); gap:12px; }
    .metric { border:1px solid var(--line); background:var(--panel); border-radius:12px; padding:20px; min-height:124px; }
    .metric-label { color:var(--muted); font-size:13px; margin-bottom:13px; }
    .metric-value { font-size:30px; font-weight:600; letter-spacing:-.04em; }
    .metric-unit { color:var(--muted); font-size:14px; font-weight:400; margin-left:6px; letter-spacing:0; }
    .empty { padding:36px 20px; border:1px dashed var(--line); border-radius:12px; color:var(--muted); }
    .note { color:var(--muted); font-size:12px; margin-top:20px; }
    footer { margin-top:36px; padding-top:16px; border-top:1px solid var(--line); font-size:12px; color:var(--muted); }
    a { color:var(--accent); text-decoration:none; } a:hover { text-decoration:underline; }
    [hidden] { display:none !important; }
    @media(max-width:720px) { .shell { padding:20px 16px; } .layout { grid-template-columns:1fr; } #nodes { max-height:250px; } #metrics { grid-template-columns:repeat(2,minmax(0,1fr)); } .metric { padding:15px; } .metric-value { font-size:26px; } }
    @media(prefers-reduced-motion:no-preference) { button { transition:background .15s,border-color .15s; } }
  </style>
</head>
<body>
  <main class="shell">
    <header>
      <div><p class="eyebrow">WeatherXM · WG1200</p><h1>Weather across your mesh</h1><p class="muted">Environmental readings received from your nodes.</p></div>
      <span id="connection" role="status" aria-live="polite">Connecting…</span>
    </header>
    <div class="toolbar">
      <p id="mode" class="muted">Choose a node to explore its readings.</p>
      <div class="actions">
        <button id="rotate" type="button" aria-pressed="false" disabled>Auto-rotate: off</button>
        <button id="units" type="button" aria-pressed="false">Use °F / imperial</button>
        <button id="refresh" type="button">Refresh</button>
      </div>
    </div>
    <div class="layout">
      <aside aria-label="Weather nodes">
        <div class="list-heading"><strong>Reporting nodes</strong><span id="count">0</span></div>
        <div id="nodes"><p class="muted" style="padding:12px">Loading nodes…</p></div>
      </aside>
      <section aria-label="Selected node readings">
        <div id="empty" class="empty">Waiting for environmental telemetry…</div>
        <div id="detail" hidden>
          <div class="station-heading">
            <div><p class="eyebrow">Selected node</p><h2 id="name"></h2><p id="node-id"></p><p id="updated"></p></div>
            <span id="freshness" class="tag"></span>
          </div>
          <div id="metrics"></div>
          <p class="note">Only reported measurements are shown. Last update refers to environmental telemetry, not other node activity.</p>
        </div>
      </section>
    </div>
    <footer>Readings refresh every 5 seconds. Rotation changes this page only. <a href="/api/v1/weather/nodes">Node data</a> · <a href="/api/v1/weather">TFT weather data</a></footer>
  </main>
  <script>
    const $ = id => document.getElementById(id);
    let nodes = [], selected = null, imperial = false, rotating = false, rotationTimer = null, fetching = false;
    const fields = {
      temperature:['Temperature','°C',1,'temperature'], relative_humidity:['Humidity','%',0],
      barometric_pressure:['Pressure','hPa',1,'pressure'], wind_speed:['Wind speed','m/s',1,'speed'],
      wind_direction:['Wind direction','°',0], wind_gust:['Wind gust','m/s',1,'speed'], wind_lull:['Wind lull','m/s',1,'speed'],
      rainfall_1h:['Rain · last hour','mm',1,'rain'], rainfall_24h:['Rain · last 24 hours','mm',1,'rain'],
      lux:['Illuminance','lx',0], white_lux:['White light','lx',0], ir_lux:['Infrared light','lx',0], uv_lux:['UV light','lx',1],
      soil_temperature:['Soil temperature','°C',1,'temperature'], soil_moisture:['Soil moisture','%',0],
      voltage:['Voltage','V',2], current:['Current','mA',1], gas_resistance:['Gas resistance','MΩ',2], iaq:['Air quality index','',0],
      distance:['Distance','mm',1], weight:['Weight','kg',2], radiation:['Radiation','µR/h',2],
      lightning_strike_count_1h:['Lightning · last hour','strikes',0], lightning_distance_km:['Storm distance','km',1]
    };
    for (let i=0;i<8;i++) {
      fields['adc_voltage_ch'+i]=['ADC voltage '+i,'V',2];
      fields['one_wire_temperature_ch'+i]=['Probe temperature '+i,'°C',1,'temperature'];
    }
    function age(node) {
      return node.age_seconds === null ? null : node.age_seconds + (performance.now()-node.fetchedAt)/1000;
    }
    function ageLabel(node) {
      const seconds = age(node);
      if (seconds === null) return 'Cached · update time unknown';
      if (seconds < 60) return 'Updated '+Math.floor(seconds)+'s ago';
      if (seconds < 3600) return 'Updated '+Math.floor(seconds/60)+'m ago';
      if (seconds < 86400) return 'Updated '+Math.floor(seconds/3600)+'h ago';
      return 'Updated '+Math.floor(seconds/86400)+'d ago';
    }
    function setRotation(enabled) {
      rotating = enabled && nodes.length > 1;
      clearInterval(rotationTimer);
      rotationTimer = null;
      if (rotating) rotationTimer = setInterval(() => {
        const index = nodes.findIndex(n => n.node_id === selected);
        selected = nodes[(index+1)%nodes.length].node_id;
        render();
      },10000);
      $('rotate').textContent = 'Auto-rotate: '+(rotating ? 'on' : 'off');
      $('rotate').setAttribute('aria-pressed',String(rotating));
      $('mode').textContent = rotating ? 'Rotating through all nodes every 10 seconds.' : 'Choose a node to explore its readings.';
    }
    function renderTimes() {
      document.querySelectorAll('[data-age]').forEach(el => {
        const node = nodes.find(n => n.node_id === el.dataset.age);
        if (node) el.textContent = ageLabel(node);
      });
      const node = nodes.find(n => n.node_id === selected);
      if (!node) return;
      $('updated').textContent = ageLabel(node)+(node.receivedAt === null ? '' : ' · '+new Date(node.receivedAt).toLocaleString());
      const seconds = age(node);
      $('freshness').textContent = seconds === null ? 'Cached reading' : seconds >= 1800 ? 'Over 30 minutes old' : 'Recent reading';
    }
    function render() {
      const focused = document.activeElement && document.activeElement.dataset.node;
      $('count').textContent = String(nodes.length);
      $('rotate').disabled = nodes.length < 2;
      $('nodes').replaceChildren();
      for (const node of nodes) {
        const button = document.createElement('button');
        button.type = 'button'; button.className = 'node'; button.dataset.node = node.node_id;
        button.setAttribute('aria-pressed',String(node.node_id === selected));
        const name = document.createElement('strong'); name.textContent = node.name || node.node_id;
        const id = document.createElement('small'); id.textContent = node.node_id;
        const time = document.createElement('small'); time.dataset.age = node.node_id;
        button.append(name,id,time);
        button.addEventListener('click',() => { selected=node.node_id; setRotation(false); render(); });
        $('nodes').append(button);
        if (focused === node.node_id) button.focus({preventScroll:true});
      }
      const node = nodes.find(n => n.node_id === selected);
      $('detail').hidden = !node; $('empty').hidden = !!node;
      if (!node) {
        $('nodes').textContent = 'No environmental telemetry received.';
        $('empty').textContent = 'No weather nodes yet. Readings will appear here when environmental telemetry arrives.';
        return;
      }
      $('name').textContent = node.name || node.node_id; $('node-id').textContent = node.node_id;
      $('metrics').replaceChildren();
      for (const [key,definition] of Object.entries(fields)) {
        const raw = node.metrics[key];
        if (typeof raw !== 'number' || !Number.isFinite(raw)) continue;
        let [label,unit,digits,kind] = definition, value = raw;
        if (imperial) {
          if (kind === 'temperature') { value=value*9/5+32; unit='°F'; }
          if (kind === 'pressure') { value*=0.02953; unit='inHg'; digits=2; }
          if (kind === 'speed') { value*=2.23694; unit='mph'; }
          if (kind === 'rain') { value*=0.0393701; unit='in'; digits=2; }
        }
        const card = document.createElement('article'); card.className='metric'; card.dataset.metric=key;
        const title = document.createElement('p'); title.className='metric-label'; title.textContent=label;
        const reading = document.createElement('p'); reading.className='metric-value'; reading.textContent=value.toFixed(digits);
        const suffix = document.createElement('span'); suffix.className='metric-unit'; suffix.textContent=unit;
        reading.append(suffix); card.append(title,reading); $('metrics').append(card);
      }
      if (!$('metrics').childElementCount) $('metrics').textContent='This report contains no available measurements.';
      renderTimes();
    }
    async function fetchWeather() {
      if (fetching) return;
      fetching = true; $('refresh').disabled = true;
      const controller = new AbortController(), timeout = setTimeout(() => controller.abort(),12000);
      try {
        const response = await fetch('/api/v1/weather/nodes',{cache:'no-store',signal:controller.signal});
        if (!response.ok) throw new Error('HTTP '+response.status);
        const data = await response.json();
        if (!Array.isArray(data.nodes)) throw new Error('Invalid node data');
        const now = performance.now(), wall = Date.now();
        nodes = data.nodes.map(node => {
          const seconds = typeof node.age_seconds === 'number' && node.age_seconds >= 0 ? node.age_seconds : null;
          return {...node,metrics:node.metrics || {},age_seconds:seconds,fetchedAt:now,receivedAt:seconds === null ? null : wall-seconds*1000};
        }).sort((a,b) => (a.name || a.node_id).localeCompare(b.name || b.node_id));
        if (!nodes.some(n => n.node_id === selected)) selected = nodes.length ? nodes[0].node_id : null;
        if (nodes.length < 2) setRotation(false);
        $('connection').textContent='Connected · refreshing every 5s'; $('connection').dataset.ok='true';
        render();
      } catch (error) {
        $('connection').textContent=nodes.length ? 'Connection lost · showing saved readings' : 'Unable to load weather nodes · retrying';
        $('connection').dataset.ok='false';
        if (!nodes.length) $('empty').textContent='Cannot reach the weather API. Check the connection or press Refresh.';
      } finally {
        clearTimeout(timeout); fetching=false; $('refresh').disabled=false;
      }
    }
    $('rotate').addEventListener('click',() => setRotation(!rotating));
    $('units').addEventListener('click',() => {
      imperial=!imperial; $('units').setAttribute('aria-pressed',String(imperial));
      $('units').textContent=imperial ? 'Use °C / metric' : 'Use °F / imperial'; render();
    });
    $('refresh').addEventListener('click',fetchWeather);
    async function poll() { await fetchWeather(); setTimeout(poll,5000); }
    poll(); setInterval(renderTimes,1000);
  </script>
</body>
</html>
)rawliteral";

#endif
