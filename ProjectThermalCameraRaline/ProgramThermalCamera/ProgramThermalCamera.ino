

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <Wire.h>
#include <Adafruit_MLX90640.h>
#include <ArduinoJson.h>

const char* AP_SSID = "THERMASCOPE";
const char* AP_PASS = "thermal123";
IPAddress AP_IP(192, 168, 4, 1);
IPAddress AP_GATEWAY(192, 168, 4, 1);
IPAddress AP_SUBNET(255, 255, 255, 0);

#define I2C_SDA 8
#define I2C_SCL 9
#define FRAME_INTERVAL_MS 250
#define DNS_PORT 53

Adafruit_MLX90640 mlx;
float frame[32*24];

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
DNSServer dnsServer;

unsigned long lastFrameSent = 0;

static const char INDEX_HTML[] PROGMEM = R"HTMLPAGE(
<!DOCTYPE html>
<html lang="id">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0">
<title>THERMASCOPE — MLX90640 Monitor</title>
<link rel="preconnect" href="https://fonts.googleapis.com">
<link href="https://fonts.googleapis.com/css2?family=Space+Grotesk:wght@400;500;600;700&family=JetBrains+Mono:wght@400;500;600;700&display=swap" rel="stylesheet">
<style>
  :root{
    --bg-void:#0a0c10;
    --bg-panel:#12151c;
    --bg-panel-raised:#171b24;
    --border-subtle:#232833;
    --border-faint:#1a1e27;
    --text-primary:#edeff4;
    --text-secondary:#8c93a3;
    --text-tertiary:#4d535f;
    --hot:#ff5a3c;
    --cool:#3fd1c5;
    --warn:#ffb020;
    --danger:#ff3b3b;
    --radius:10px;
  }
  *{margin:0;padding:0;box-sizing:border-box;}
  html,body{
    background:var(--bg-void);
    color:var(--text-primary);
    font-family:'Space Grotesk',sans-serif;
    min-height:100vh;
    -webkit-font-smoothing:antialiased;
  }
  body{
    background-image:
      radial-gradient(circle at 15% 0%, rgba(255,90,60,0.05), transparent 40%),
      radial-gradient(circle at 85% 100%, rgba(63,209,197,0.05), transparent 40%);
  }
  .mono{font-family:'JetBrains Mono',monospace;}

  
  .shell{
    max-width:1320px;
    margin:0 auto;
    padding:20px 24px 60px;
  }
  header{
    display:flex;
    align-items:center;
    justify-content:space-between;
    padding:14px 20px;
    background:var(--bg-panel);
    border:1px solid var(--border-subtle);
    border-radius:var(--radius);
    margin-bottom:18px;
    flex-wrap:wrap;
    gap:12px;
  }
  .brand{display:flex;align-items:center;gap:10px;}
  .brand-mark{
    width:10px;height:10px;border-radius:50%;
    background:var(--text-tertiary);
    box-shadow:0 0 0 3px rgba(255,255,255,0.03);
    flex-shrink:0;
    transition:background .3s, box-shadow .3s;
  }
  .brand-mark.live{background:var(--cool); box-shadow:0 0 0 3px rgba(63,209,197,0.15); animation:pulse 2s infinite;}
  .brand-mark.sim{background:var(--warn); box-shadow:0 0 0 3px rgba(255,176,32,0.15);}
  .brand-mark.off{background:var(--text-tertiary);}
  @keyframes pulse{0%,100%{opacity:1;}50%{opacity:.45;}}
  .brand h1{font-size:16px;font-weight:700;letter-spacing:.06em;}
  .brand span.sub{font-size:12px;color:var(--text-secondary);font-weight:400;letter-spacing:.02em;display:block;margin-top:1px;}
  .status-line{font-size:12px;color:var(--text-secondary);text-align:right;}
  .status-line b{color:var(--text-primary);font-weight:600;}
  .status-line .mono{color:var(--text-tertiary);}

  .grid{
    display:grid;
    grid-template-columns: 1.4fr 1fr;
    gap:18px;
  }
  @media (max-width:920px){ .grid{grid-template-columns:1fr;} }

  .panel{
    background:var(--bg-panel);
    border:1px solid var(--border-subtle);
    border-radius:var(--radius);
    padding:18px;
  }
  .panel-title{
    font-size:11px;
    letter-spacing:.12em;
    text-transform:uppercase;
    color:var(--text-secondary);
    font-weight:600;
    margin-bottom:14px;
    display:flex;
    justify-content:space-between;
    align-items:center;
  }

  
  .viewport-wrap{position:relative;}
  .viewport{
    position:relative;
    width:100%;
    aspect-ratio: 4/3;
    background:#000;
    border-radius:6px;
    overflow:hidden;
    border:1px solid var(--border-faint);
  }
  #thermalCanvas{width:100%;height:100%;display:block;image-rendering:auto;}
  .scanline{
    position:absolute; left:0; right:0; height:2px;
    background:linear-gradient(90deg, transparent, rgba(63,209,197,.55), transparent);
    top:0; pointer-events:none;
    animation:sweep 4.5s linear infinite;
    opacity:.5;
  }
  @keyframes sweep{0%{top:0%;}100%{top:100%;}}
  .marker{
    position:absolute;
    width:16px;height:16px;
    transform:translate(-50%,-50%);
    pointer-events:none;
  }
  .marker::before,.marker::after{
    content:'';position:absolute;background:currentColor;
  }
  .marker::before{width:100%;height:1px;top:50%;left:0;}
  .marker::after{width:1px;height:100%;left:50%;top:0;}
  .marker.hot{color:var(--hot);}
  .marker.cool{color:var(--cool);}
  .marker-label{
    position:absolute;font-size:10px;font-family:'JetBrains Mono',monospace;
    padding:1px 5px;border-radius:3px;white-space:nowrap;
    transform:translate(-50%,-130%);
    font-weight:600;
  }
  .marker-label.hot{background:rgba(255,90,60,.16);color:var(--hot);border:1px solid rgba(255,90,60,.3);}
  .marker-label.cool{background:rgba(63,209,197,.14);color:var(--cool);border:1px solid rgba(63,209,197,.3);}

  .colorbar-row{
    display:flex; align-items:center; gap:10px; margin-top:12px;
  }
  .colorbar{
    flex:1; height:8px; border-radius:4px;
    background:linear-gradient(90deg,#000000 0%,#1e0033 14%,#6a0d78 28%,#c31b6e 42%,#e8382e 56%,#ff8a1f 70%,#ffd63f 84%,#ffffff 100%);
  }
  .colorbar-label{font-size:11px;color:var(--text-secondary);}

  
  .readout-row{display:grid;grid-template-columns:1fr 1fr;gap:12px;margin-bottom:14px;}
  .stat-card{
    background:var(--bg-panel-raised);
    border:1px solid var(--border-subtle);
    border-radius:8px;
    padding:12px 14px;
  }
  .stat-card .label{font-size:10.5px;letter-spacing:.08em;text-transform:uppercase;color:var(--text-secondary);margin-bottom:6px;}
  .stat-card .value{font-size:26px;font-weight:600;line-height:1;}
  .stat-card.hot .value{color:var(--hot);}
  .stat-card.cool .value{color:var(--cool);}
  .stat-card .value sup{font-size:13px;color:var(--text-tertiary);margin-left:2px;}
  .stat-card .coord{font-size:11px;color:var(--text-tertiary);margin-top:4px;}

  
  .field-row{display:flex;gap:10px;margin-bottom:12px;}
  .field{flex:1;}
  .field label{
    font-size:10.5px;letter-spacing:.06em;text-transform:uppercase;
    color:var(--text-secondary);display:block;margin-bottom:6px;
  }
  .field input[type=number]{
    width:100%;
    background:var(--bg-void);
    border:1px solid var(--border-subtle);
    color:var(--text-primary);
    font-family:'JetBrains Mono',monospace;
    font-size:15px;
    padding:9px 10px;
    border-radius:6px;
    outline:none;
    transition:border-color .15s;
  }
  .field input[type=number]:focus{border-color:var(--cool);}
  .field input[type=number].err{border-color:var(--danger);}

  .count-display{
    background:var(--bg-void);
    border:1px solid var(--border-subtle);
    border-radius:8px;
    padding:16px;
    text-align:center;
    margin:14px 0;
  }
  .count-display .n{font-size:42px;font-weight:700;font-family:'JetBrains Mono',monospace;color:var(--warn);line-height:1;}
  .count-display .cap{font-size:11px;color:var(--text-secondary);letter-spacing:.08em;text-transform:uppercase;margin-top:6px;}
  .count-display .range{font-size:12px;color:var(--text-tertiary);margin-top:4px;font-family:'JetBrains Mono',monospace;}

  
  .toggle-row{
    display:flex;align-items:center;justify-content:space-between;
    padding:10px 0;
  }
  .toggle-row .label{font-size:13px;color:var(--text-primary);}
  .toggle-row .desc{font-size:11px;color:var(--text-tertiary);margin-top:2px;}
  .switch{
    position:relative;width:40px;height:22px;flex-shrink:0;
  }
  .switch input{opacity:0;width:0;height:0;}
  .switch .track{
    position:absolute;inset:0;background:var(--bg-void);border:1px solid var(--border-subtle);
    border-radius:20px;cursor:pointer;transition:.2s;
  }
  .switch .track::before{
    content:'';position:absolute;width:16px;height:16px;left:2px;top:2px;
    background:var(--text-tertiary);border-radius:50%;transition:.2s;
  }
  .switch input:checked + .track{background:rgba(255,176,32,.15);border-color:var(--warn);}
  .switch input:checked + .track::before{transform:translateX(18px);background:var(--warn);}

  
  .btn-row{display:flex;gap:10px;margin-top:6px;}
  button{
    font-family:'Space Grotesk',sans-serif;
    font-weight:600;
    font-size:13px;
    border-radius:7px;
    border:1px solid var(--border-subtle);
    padding:11px 16px;
    cursor:pointer;
    transition:filter .15s, transform .1s;
    background:var(--bg-panel-raised);
    color:var(--text-primary);
  }
  button:hover{filter:brightness(1.15);}
  button:active{transform:scale(.97);}
  button.primary{background:var(--cool);border-color:var(--cool);color:#04211f;}
  button.ghost{background:transparent;}
  button.danger-ghost{background:transparent;color:var(--text-secondary);}
  button.danger-ghost:hover{color:var(--danger);border-color:rgba(255,59,59,.4);}
  button:disabled{opacity:.4;cursor:not-allowed;}

  
  .log-panel{margin-top:18px;}
  table{width:100%;border-collapse:collapse;font-size:13px;}
  thead th{
    text-align:left;font-size:10.5px;letter-spacing:.08em;text-transform:uppercase;
    color:var(--text-secondary);padding:8px 10px;border-bottom:1px solid var(--border-subtle);
    font-weight:600;
  }
  tbody td{
    padding:9px 10px;border-bottom:1px solid var(--border-faint);
    font-family:'JetBrains Mono',monospace;color:var(--text-primary);
  }
  tbody tr:nth-child(even){background:rgba(255,255,255,.015);}
  tbody tr:hover{background:rgba(63,209,197,.04);}
  .empty-row td{
    text-align:center;color:var(--text-tertiary);padding:26px 10px;font-family:'Space Grotesk',sans-serif;
    font-size:13px;
  }
  .log-scroll{max-height:280px;overflow-y:auto;}
  .log-scroll::-webkit-scrollbar{width:6px;}
  .log-scroll::-webkit-scrollbar-thumb{background:var(--border-subtle);border-radius:3px;}

  
  #toastZone{
    position:fixed;top:18px;right:18px;display:flex;flex-direction:column;gap:8px;z-index:50;
  }
  .toast{
    background:var(--bg-panel-raised);
    border:1px solid var(--danger);
    border-left:4px solid var(--danger);
    border-radius:7px;
    padding:12px 16px;
    min-width:240px;
    box-shadow:0 8px 24px rgba(0,0,0,.4);
    animation:slidein .25s ease-out;
  }
  @keyframes slidein{from{transform:translateX(30px);opacity:0;}to{transform:translateX(0);opacity:1;}}
  .toast .t-title{font-size:12.5px;font-weight:700;color:var(--danger);letter-spacing:.03em;}
  .toast .t-body{font-size:12px;color:var(--text-secondary);margin-top:3px;}

  .banner{
    display:none;
    align-items:center;gap:8px;
    background:rgba(255,176,32,.08);
    border:1px solid rgba(255,176,32,.3);
    color:var(--warn);
    font-size:12px;
    padding:9px 14px;
    border-radius:7px;
    margin-bottom:16px;
  }
  .banner.show{display:flex;}
</style>
</head>
<body>

<div id="toastZone"></div>

<div class="shell">

  <header>
    <div class="brand">
      <div class="brand-mark off" id="statusDot"></div>
      <div>
        <h1>THERMASCOPE</h1>
        <span class="sub">MLX90640 · 32×24 · ESP32-S3</span>
      </div>
    </div>
    <div class="status-line">
      <div id="statusText"><b>Menghubungkan…</b></div>
      <div class="mono" id="hostText">host: —</div>
    </div>
  </header>

  <div class="banner" id="simBanner">◆ Mode simulasi aktif — sensor belum terhubung. Halaman menampilkan data uji untuk pratinjau tampilan.</div>

  <div class="grid">

    <!-- LEFT: thermal viewport -->
    <div class="panel">
      <div class="panel-title">
        <span>Citra Termal Live</span>
        <span class="mono" id="fpsText" style="color:var(--text-tertiary);font-weight:400;">— fps</span>
      </div>
      <div class="viewport-wrap">
        <div class="viewport">
          <canvas id="thermalCanvas" width="320" height="240"></canvas>
          <div class="scanline"></div>
          <div class="marker hot" id="markHot"></div>
          <div class="marker cool" id="markCool"></div>
          <div class="marker-label hot" id="labelHot">—</div>
          <div class="marker-label cool" id="labelCool">—</div>
        </div>
        <div class="colorbar-row">
          <span class="colorbar-label mono" id="cbMin">—°C</span>
          <div class="colorbar"></div>
          <span class="colorbar-label mono" id="cbMax">—°C</span>
        </div>
      </div>

      <div class="readout-row" style="margin-top:16px;">
        <div class="stat-card hot">
          <div class="label">Suhu Maksimum</div>
          <div class="value mono" id="tMax">—<sup>°C</sup></div>
          <div class="coord mono" id="tMaxCoord">baris —, kolom —</div>
        </div>
        <div class="stat-card cool">
          <div class="label">Suhu Minimum</div>
          <div class="value mono" id="tMin">—<sup>°C</sup></div>
          <div class="coord mono" id="tMinCoord">baris —, kolom —</div>
        </div>
      </div>
    </div>

    <!-- RIGHT: threshold + controls -->
    <div>
      <div class="panel">
        <div class="panel-title"><span>Rentang Threshold</span></div>
        <div class="field-row">
          <div class="field">
            <label for="thLower">Batas bawah (°C)</label>
            <input type="number" id="thLower" step="0.1" value="36">
          </div>
          <div class="field">
            <label for="thUpper">Batas atas (°C)</label>
            <input type="number" id="thUpper" step="0.1" value="37">
          </div>
        </div>

        <div class="count-display">
          <div class="n mono" id="pixelCount">0</div>
          <div class="cap">Piksel dalam rentang</div>
          <div class="range mono" id="rangeEcho">36.0°C – 37.0°C</div>
        </div>

        <div class="toggle-row" style="border-top:1px solid var(--border-faint);padding-top:12px;">
          <div>
            <div class="label">Notifikasi ambang piksel</div>
            <div class="desc" id="alertDesc">Nonaktif</div>
          </div>
          <label class="switch">
            <input type="checkbox" id="alertToggle">
            <span class="track"></span>
          </label>
        </div>
        <div class="field-row" id="alertFieldRow" style="opacity:.4;pointer-events:none;">
          <div class="field">
            <label for="alertLimit">Kirim notifikasi jika jumlah piksel lebih dari</label>
            <input type="number" id="alertLimit" step="1" value="50">
          </div>
        </div>
      </div>

      <div class="panel" style="margin-top:18px;">
        <div class="panel-title"><span>Perekaman Data</span></div>
        <p style="font-size:12px;color:var(--text-secondary);line-height:1.5;margin-bottom:14px;">
          Setiap kali <b style="color:var(--text-primary);">Rekam</b> ditekan, jumlah piksel pada rentang saat ini disimpan sebagai baris baru bernomor urut pada log di bawah.
        </p>
        <div class="btn-row">
          <button class="primary" id="btnRecord" style="flex:1;">● Rekam</button>
          <button class="danger-ghost" id="btnReset">Reset</button>
          <button class="ghost" id="btnExport">Unduh CSV</button>
        </div>
      </div>
    </div>
  </div>

  <!-- LOG TABLE -->
  <div class="panel log-panel">
    <div class="panel-title">
      <span>Log Perekaman</span>
      <span class="mono" id="logCountText" style="color:var(--text-tertiary);font-weight:400;">0 entri</span>
    </div>
    <div class="log-scroll">
      <table>
        <thead>
          <tr>
            <th style="width:50px;">No.</th>
            <th>Waktu</th>
            <th>Threshold</th>
            <th>Jumlah Piksel</th>
            <th>Suhu Max</th>
            <th>Suhu Min</th>
          </tr>
        </thead>
        <tbody id="logBody">
          <tr class="empty-row"><td colspan="6">Belum ada data yang direkam.</td></tr>
        </tbody>
      </table>
    </div>
  </div>

</div>

<script>
(function(){
  "use strict";

  const GRID_W = 32, GRID_H = 24, N_PIX = GRID_W*GRID_H;
  const UPSCALE = 10;

  let latestFrame = new Float32Array(N_PIX).fill(25);
  let ws = null, wsRetryTimer = null, demoTimer = null, isDemo = false, isConnected = false;
  let records = [], recordCounter = 0;
  let lastFrameTime = performance.now(), fpsSmooth = 0;

  const $ = (id)=>document.getElementById(id);
  const canvas = $('thermalCanvas');
  const ctx = canvas.getContext('2d');
  const offCanvas = document.createElement('canvas');
  offCanvas.width = GRID_W; offCanvas.height = GRID_H;
  const offCtx = offCanvas.getContext('2d');

  const statusDot = $('statusDot'), statusText = $('statusText'), hostText = $('hostText');
  const simBanner = $('simBanner'), fpsText = $('fpsText');
  const thLowerEl = $('thLower'), thUpperEl = $('thUpper');
  const pixelCountEl = $('pixelCount'), rangeEchoEl = $('rangeEcho');
  const tMaxEl = $('tMax'), tMinEl = $('tMin'), tMaxCoordEl = $('tMaxCoord'), tMinCoordEl = $('tMinCoord');
  const cbMinEl = $('cbMin'), cbMaxEl = $('cbMax');
  const markHot = $('markHot'), markCool = $('markCool'), labelHot = $('labelHot'), labelCool = $('labelCool');
  const alertToggle = $('alertToggle'), alertDesc = $('alertDesc'), alertLimitEl = $('alertLimit'), alertFieldRow = $('alertFieldRow');
  const btnRecord = $('btnRecord'), btnReset = $('btnReset'), btnExport = $('btnExport');
  const logBody = $('logBody'), logCountText = $('logCountText');

  hostText.textContent = 'host: ' + (location.host || 'lokal');

  const STOPS = [
    [0,   0,0,0],
    [0.14,30,0,51],
    [0.28,106,13,120],
    [0.42,195,27,110],
    [0.56,232,56,46],
    [0.70,255,138,31],
    [0.84,255,214,63],
    [1.0, 255,255,255]
  ];
  function colormap(t){
    t = Math.max(0, Math.min(1, t));
    for(let i=0;i<STOPS.length-1;i++){
      const a = STOPS[i], b = STOPS[i+1];
      if(t>=a[0] && t<=b[0]){
        const f = (t-a[0])/(b[0]-a[0] || 1);
        return [
          Math.round(a[1]+(b[1]-a[1])*f),
          Math.round(a[2]+(b[2]-a[2])*f),
          Math.round(a[3]+(b[3]-a[3])*f)
        ];
      }
    }
    return [255,255,255];
  }

  function renderFrame(rawFrame){
    const frame = new Float32Array(rawFrame.length);
    for(let r=0;r<GRID_H;r++){
      const srcRow = GRID_H-1-r;
      for(let c=0;c<GRID_W;c++){
        const srcCol = GRID_W-1-c;
        frame[r*GRID_W+c] = rawFrame[srcRow*GRID_W+srcCol];
      }
    }

    let min = Infinity, max = -Infinity, minIdx=0, maxIdx=0;
    for(let i=0;i<frame.length;i++){
      const v = frame[i];
      if(v<min){min=v;minIdx=i;}
      if(v>max){max=v;maxIdx=i;}
    }
    const span = (max-min) || 1;

    const imgData = offCtx.createImageData(GRID_W, GRID_H);
    for(let i=0;i<frame.length;i++){
      const t = (frame[i]-min)/span;
      const [r,g,b] = colormap(t);
      imgData.data[i*4+0]=r; imgData.data[i*4+1]=g; imgData.data[i*4+2]=b; imgData.data[i*4+3]=255;
    }
    offCtx.putImageData(imgData,0,0);

    ctx.imageSmoothingEnabled = true;
    ctx.imageSmoothingQuality = 'high';
    ctx.clearRect(0,0,canvas.width,canvas.height);
    ctx.drawImage(offCanvas, 0,0,GRID_W,GRID_H, 0,0,canvas.width,canvas.height);

    tMaxEl.innerHTML = max.toFixed(1)+'<sup>°C</sup>';
    tMinEl.innerHTML = min.toFixed(1)+'<sup>°C</sup>';
    const maxRow = Math.floor(maxIdx/GRID_W), maxCol = maxIdx%GRID_W;
    const minRow = Math.floor(minIdx/GRID_W), minCol = minIdx%GRID_W;
    tMaxCoordEl.textContent = `baris ${maxRow+1}, kolom ${maxCol+1}`;
    tMinCoordEl.textContent = `baris ${minRow+1}, kolom ${minCol+1}`;
    cbMinEl.textContent = min.toFixed(1)+'°C';
    cbMaxEl.textContent = max.toFixed(1)+'°C';

    positionMarker(markHot, labelHot, maxCol, maxRow, max.toFixed(1)+'°C');
    positionMarker(markCool, labelCool, minCol, minRow, min.toFixed(1)+'°C');

    updateThresholdCount(frame, max, min);

    const now = performance.now();
    const inst = 1000/(now-lastFrameTime);
    fpsSmooth = fpsSmooth ? fpsSmooth*0.8 + inst*0.2 : inst;
    lastFrameTime = now;
    fpsText.textContent = fpsSmooth.toFixed(1)+' fps';
  }

  function positionMarker(markEl, labelEl, col, row, text){
    const xPct = ((col+0.5)/GRID_W)*100;
    const yPct = ((row+0.5)/GRID_H)*100;
    markEl.style.left = xPct+'%';
    markEl.style.top = yPct+'%';
    labelEl.style.left = xPct+'%';
    labelEl.style.top = yPct+'%';
    labelEl.textContent = text;
  }

  let lastCount = 0;
  function updateThresholdCount(frame, tmax, tmin){
    const lower = parseFloat(thLowerEl.value);
    const upper = parseFloat(thUpperEl.value);
    let count = 0;
    if(!isNaN(lower) && !isNaN(upper) && lower<=upper){
      for(let i=0;i<frame.length;i++){
        if(frame[i]>=lower && frame[i]<=upper) count++;
      }
    }
    lastCount = count;
    pixelCountEl.textContent = count;
    rangeEchoEl.textContent = `${lower.toFixed(1)}°C – ${upper.toFixed(1)}°C`;
    checkAlert(count);
  }

  function validateThresholds(){
    const lower = parseFloat(thLowerEl.value);
    const upper = parseFloat(thUpperEl.value);
    const bad = isNaN(lower) || isNaN(upper) || lower>upper;
    thLowerEl.classList.toggle('err', bad);
    thUpperEl.classList.toggle('err', bad);
    updateThresholdCount(latestFrame, 0, 0);
  }
  thLowerEl.addEventListener('input', validateThresholds);
  thUpperEl.addEventListener('input', validateThresholds);

  let notifyPermissionAsked = false;
  let alertCooldown = false;
  function checkAlert(count){
    if(!alertToggle.checked) return;
    const limit = parseFloat(alertLimitEl.value);
    if(isNaN(limit)) return;
    if(count > limit && !alertCooldown){
      fireAlert(count, limit);
      alertCooldown = true;
      setTimeout(()=>{ alertCooldown = false; }, 8000);
    }
  }
  function fireAlert(count, limit){
    showToast('Ambang piksel terlampaui', `Terdeteksi ${count} piksel dalam rentang, melebihi batas ${limit}.`);
    if('Notification' in window && Notification.permission === 'granted'){
      new Notification('THERMASCOPE — Ambang terlampaui', {
        body: `${count} piksel dalam rentang (batas: ${limit})`
      });
    }
    beep();
  }
  function showToast(title, body){
    const zone = $('toastZone');
    const el = document.createElement('div');
    el.className = 'toast';
    el.innerHTML = `<div class="t-title">${title}</div><div class="t-body">${body}</div>`;
    zone.appendChild(el);
    setTimeout(()=>{ el.style.transition='opacity .3s'; el.style.opacity='0'; setTimeout(()=>el.remove(),300); }, 5000);
  }
  function beep(){
    try{
      const AudioCtx = window.AudioContext || window.webkitAudioContext;
      const actx = new AudioCtx();
      const osc = actx.createOscillator();
      const gain = actx.createGain();
      osc.frequency.value = 880;
      osc.type = 'sine';
      gain.gain.setValueAtTime(0.08, actx.currentTime);
      gain.gain.exponentialRampToValueAtTime(0.001, actx.currentTime+0.35);
      osc.connect(gain); gain.connect(actx.destination);
      osc.start(); osc.stop(actx.currentTime+0.35);
    }catch(e){}
  }
  alertToggle.addEventListener('change', ()=>{
    const on = alertToggle.checked;
    alertFieldRow.style.opacity = on ? '1' : '.4';
    alertFieldRow.style.pointerEvents = on ? 'auto' : 'none';
    alertDesc.textContent = on ? 'Aktif' : 'Nonaktif';
    if(on && 'Notification' in window && Notification.permission === 'default' && !notifyPermissionAsked){
      notifyPermissionAsked = true;
      Notification.requestPermission();
    }
  });

  function addRecord(){
    recordCounter++;
    const now = new Date();
    const time = now.toLocaleTimeString('id-ID', {hour:'2-digit',minute:'2-digit',second:'2-digit'});
    let tmax=-Infinity, tmin=Infinity;
    for(const v of latestFrame){ if(v>tmax)tmax=v; if(v<tmin)tmin=v; }
    const entry = {
      no: recordCounter,
      time,
      lower: parseFloat(thLowerEl.value).toFixed(1),
      upper: parseFloat(thUpperEl.value).toFixed(1),
      count: lastCount,
      tmax: tmax.toFixed(1),
      tmin: tmin.toFixed(1)
    };
    records.push(entry);
    renderLog();
  }
  function renderLog(){
    if(records.length===0){
      logBody.innerHTML = '<tr class="empty-row"><td colspan="6">Belum ada data yang direkam.</td></tr>';
    } else {
      logBody.innerHTML = records.map(r=>`
        <tr>
          <td>${r.no}</td>
          <td>${r.time}</td>
          <td>${r.lower}°C – ${r.upper}°C</td>
          <td style="color:var(--warn);font-weight:600;">${r.count}</td>
          <td style="color:var(--hot);">${r.tmax}°C</td>
          <td style="color:var(--cool);">${r.tmin}°C</td>
        </tr>`).join('');
    }
    logCountText.textContent = records.length+' entri';
  }
  btnRecord.addEventListener('click', addRecord);
  btnReset.addEventListener('click', ()=>{
    if(records.length===0) return;
    if(confirm('Reset seluruh log perekaman? Tindakan ini tidak dapat dibatalkan.')){
      records = []; recordCounter = 0; renderLog();
    }
  });
  btnExport.addEventListener('click', ()=>{
    if(records.length===0){ showToast('Belum ada data', 'Log masih kosong, tidak ada yang bisa diunduh.'); return; }
    const header = 'No,Waktu,Threshold Bawah,Threshold Atas,Jumlah Piksel,Suhu Max,Suhu Min\n';
    const rows = records.map(r=>`${r.no},${r.time},${r.lower},${r.upper},${r.count},${r.tmax},${r.tmin}`).join('\n');
    const blob = new Blob([header+rows], {type:'text/csv;charset=utf-8;'});
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url; a.download = 'thermascope_log.csv';
    a.click();
    URL.revokeObjectURL(url);
  });

  function setStatus(mode, text){
    statusDot.className = 'brand-mark ' + mode;
    statusText.innerHTML = '<b>'+text+'</b>';
    simBanner.classList.toggle('show', mode==='sim');
  }

  function connectWS(){
    if(ws) { try{ ws.close(); }catch(e){} }
    const url = `ws://${location.hostname}/ws`;
    try{
      ws = new WebSocket(url);
    }catch(e){
      scheduleDemoFallback();
      return;
    }
    setStatus('off','Menghubungkan…');

    const connectTimeout = setTimeout(()=>{
      if(!isConnected) scheduleDemoFallback();
    }, 3000);

    ws.onopen = ()=>{
      clearTimeout(connectTimeout);
      isConnected = true; isDemo = false;
      stopDemo();
      setStatus('live','Terhubung ke sensor');
    };
    ws.onmessage = (evt)=>{
      try{
        const parsed = JSON.parse(evt.data);
        const arr = Array.isArray(parsed) ? parsed : parsed.data;
        if(Array.isArray(arr) && arr.length===N_PIX){
          latestFrame = Float32Array.from(arr);
          renderFrame(latestFrame);
        }
      }catch(e){  }
    };
    ws.onclose = ()=>{
      isConnected = false;
      setStatus('off','Koneksi terputus');
      clearTimeout(connectTimeout);
      wsRetryTimer = setTimeout(connectWS, 4000);
      scheduleDemoFallback();
    };
    ws.onerror = ()=>{  };
  }

  function scheduleDemoFallback(){
    if(isDemo || isConnected) return;
    isDemo = true;
    setStatus('sim','Mode simulasi (sensor tidak terdeteksi)');
    startDemo();
  }

  let demoT = 0;
  function startDemo(){
    if(demoTimer) return;
    demoTimer = setInterval(()=>{
      demoT += 0.06;
      const frame = new Float32Array(N_PIX);
      const cx = GRID_W/2 + Math.sin(demoT)*9;
      const cy = GRID_H/2 + Math.cos(demoT*0.8)*6;
      for(let y=0;y<GRID_H;y++){
        for(let x=0;x<GRID_W;x++){
          const d = Math.hypot(x-cx, y-cy);
          const base = 24 + Math.sin(demoT*0.3)*0.5;
          const heat = 14*Math.exp(-(d*d)/40);
          frame[y*GRID_W+x] = base + heat + (Math.random()-0.5)*0.3;
        }
      }
      latestFrame = frame;
      renderFrame(frame);
    }, 200);
  }
  function stopDemo(){
    if(demoTimer){ clearInterval(demoTimer); demoTimer = null; }
  }

  renderFrame(latestFrame);
  connectWS();

})();
</script>
</body>
</html>

)HTMLPAGE";

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
               AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    Serial.printf("Client WebSocket #%u terhubung\n", client->id());
  } else if (type == WS_EVT_DISCONNECT) {
    Serial.printf("Client WebSocket #%u terputus\n", client->id());
  }
}

void sendIndexPage(AsyncWebServerRequest *request) {
  AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", (const uint8_t*)INDEX_HTML, strlen_P(INDEX_HTML));
  request->send(response);
}

void setup() {
  Serial.begin(115200);

  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(AP_IP, AP_GATEWAY, AP_SUBNET);
  WiFi.softAP(AP_SSID, AP_PASS);
  delay(200);

  Serial.println();
  Serial.print("Hotspot aktif, SSID: ");
  Serial.println(AP_SSID);
  Serial.print("IP tetap: ");
  Serial.println(WiFi.softAPIP());

  dnsServer.start(DNS_PORT, "*", AP_IP);

  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(1000000);

  if (!mlx.begin(MLX90640_I2CADDR_DEFAULT, &Wire)) {
    Serial.println("Sensor MLX90640 tidak terdeteksi. Periksa wiring.");
    Serial.println("(Server tetap dijalankan agar halaman tetap bisa dibuka untuk pengecekan.)");
  } else {
    mlx.setMode(MLX90640_CHESS);
    mlx.setResolution(MLX90640_ADC_18BIT);
    mlx.setRefreshRate(MLX90640_4_HZ);
  }

  server.on("/", HTTP_GET, sendIndexPage);
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  server.onNotFound(sendIndexPage);

  server.begin();

  Serial.println("Server siap.");
  Serial.println("Sambungkan WiFi ke hotspot di atas, lalu buka 192.168.4.1 di Chrome.");
}

void loop() {
  dnsServer.processNextRequest();
  ws.cleanupClients();

  if (millis() - lastFrameSent < FRAME_INTERVAL_MS) return;

  if (mlx.getFrame(frame) != 0) {
    return;
  }
  lastFrameSent = millis();

  if (ws.count() == 0) return;

  String payload;
  payload.reserve(768 * 6 + 2);
  payload += '[';
  for (int i = 0; i < 32*24; i++) {
    payload += String(frame[i], 1);
    if (i < 32*24 - 1) payload += ',';
  }
  payload += ']';

  ws.textAll(payload);
}
