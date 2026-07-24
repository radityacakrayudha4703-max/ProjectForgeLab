

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
#define LED_PIN 6
#define LED_BLINK_FAST_MS 150
#define LED_BLINK_SLOW_MS 800
#define BATTERY_ADC_PIN 7
#define BATTERY_DIVIDER_RATIO 3.0
#define BATTERY_EMPTY_V 5.4
#define BATTERY_FULL_V 8.4
#define BATTERY_READ_INTERVAL_MS 2000

Adafruit_MLX90640 mlx;
float frame[32*24];

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
DNSServer dnsServer;

unsigned long lastFrameSent = 0;
unsigned long lastLedToggle = 0;
bool ledState = false;
volatile bool blinkRequestPending = false;
bool notifBlinking = false;
int notifBlinkToggles = 0;
float batteryVoltage = 0;
unsigned long lastBatteryRead = 0;

float readBatteryVoltage(){
  int mv = analogReadMilliVolts(BATTERY_ADC_PIN);
  return (mv / 1000.0) * BATTERY_DIVIDER_RATIO;
}

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

  .battery-indicator{
    display:flex;align-items:center;gap:8px;
    padding:6px 12px;
    background:var(--bg-panel-raised);
    border:1px solid var(--border-subtle);
    border-radius:8px;
  }
  .battery-indicator .mono{font-size:13px;font-weight:600;color:var(--text-primary);}
  .batt-bar{fill:var(--border-subtle);transition:fill .3s;}
  .batt-bar.on{fill:var(--cool);}
  .batt-bar.on.warn{fill:var(--warn);}
  .batt-bar.on.danger{fill:var(--danger);}

  .user-badge{
    display:flex;align-items:center;
    padding:6px 14px;
    background:var(--bg-panel-raised);
    border:1px solid var(--border-subtle);
    border-radius:8px;
    font-size:13px;font-weight:600;color:var(--text-primary);
    cursor:pointer;
    transition:filter .15s;
  }
  .user-badge:hover{filter:brightness(1.15);}
  .user-badge::before{content:'👤';margin-right:6px;}

  .lang-choice{display:flex;gap:8px;}
  .lang-option{
    flex:1;
    font-family:'Space Grotesk',sans-serif;
    font-size:13px;font-weight:600;
    padding:10px;
    border-radius:7px;
    border:1px solid var(--border-subtle);
    background:var(--bg-void);
    color:var(--text-secondary);
    cursor:pointer;
    transition:filter .15s, border-color .15s, color .15s;
  }
  .lang-option.active{border-color:var(--cool);color:var(--cool);background:rgba(63,209,197,.08);}
  .lang-option:hover{filter:brightness(1.2);}
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
  .btn-tool{
    font-family:'JetBrains Mono',monospace;
    font-size:10.5px;
    font-weight:600;
    letter-spacing:.03em;
    text-transform:none;
    padding:5px 9px;
    border-radius:6px;
    border:1px solid var(--border-subtle);
    background:var(--bg-panel-raised);
    color:var(--text-secondary);
    cursor:pointer;
    transition:filter .15s, color .15s, border-color .15s;
  }
  .btn-tool:hover{filter:brightness(1.2);color:var(--text-primary);}
  .btn-tool.active{color:var(--cool);border-color:rgba(63,209,197,.4);background:rgba(63,209,197,.08);}

  
  .viewport-wrap{position:relative;}
  .viewport{
    position:relative;
    width:100%;
    aspect-ratio: 4/3;
    background:#000;
    border-radius:6px;
    overflow:hidden;
    border:1px solid var(--border-faint);
    transition: aspect-ratio .25s ease;
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
  .viewport.frozen .scanline{animation-play-state:paused; opacity:.2;}
  .viewport.frozen{border-color:var(--warn);}
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
  .marker.center{color:#f0f2f6;}
  .marker-label{
    position:absolute;font-size:10px;font-family:'JetBrains Mono',monospace;
    padding:1px 5px;border-radius:3px;white-space:nowrap;
    transform:translate(-50%,-130%);
    font-weight:600;
  }
  .marker-label.hot{background:rgba(255,90,60,.16);color:var(--hot);border:1px solid rgba(255,90,60,.3);}
  .marker-label.cool{background:rgba(63,209,197,.14);color:var(--cool);border:1px solid rgba(63,209,197,.3);}
  .marker-label.center{background:rgba(240,242,246,.12);color:#f0f2f6;border:1px solid rgba(240,242,246,.3);}

  .colorbar-row{
    display:flex; align-items:center; gap:10px; margin-top:12px;
  }
  .colorbar{
    flex:1; height:8px; border-radius:4px;
    background:linear-gradient(90deg,#000000 0%,#1e0033 14%,#6a0d78 28%,#c31b6e 42%,#e8382e 56%,#ff8a1f 70%,#ffd63f 84%,#ffffff 100%);
  }
  .colorbar-label{font-size:11px;color:var(--text-secondary);}

  
  .readout-row{display:grid;grid-template-columns:1fr 1fr 1fr;gap:12px;margin-bottom:14px;}
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
  .stat-card.center .value{color:#f0f2f6;}
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

  .level-status-row{
    display:flex;align-items:center;justify-content:space-between;
    padding:10px 0;
  }
  .level-badge{
    font-family:'JetBrains Mono',monospace;
    font-size:15px;font-weight:700;margin-top:4px;
    color:var(--text-primary);
  }

  .modal-overlay{
    display:none;
    position:fixed;inset:0;
    background:rgba(0,0,0,.6);
    z-index:100;
    align-items:center;justify-content:center;
    padding:20px;
  }
  .modal-overlay.show{display:flex;}
  .modal-box{
    background:var(--bg-panel);
    border:1px solid var(--border-subtle);
    border-radius:var(--radius);
    padding:22px;
    width:100%;
    max-width:440px;
    max-height:85vh;
    overflow-y:auto;
  }
  .modal-header{
    display:flex;justify-content:space-between;align-items:center;
    font-size:15px;font-weight:700;margin-bottom:6px;
  }
  .modal-close{
    background:transparent;border:none;color:var(--text-secondary);
    font-size:16px;cursor:pointer;padding:4px;
  }
  .modal-close:hover{color:var(--text-primary);}
  .modal-desc{font-size:12px;color:var(--text-secondary);line-height:1.5;margin-bottom:16px;}
  .level-row{
    display:flex;align-items:center;gap:10px;
    padding:8px 0;
  }
  .level-dot{width:10px;height:10px;border-radius:50%;flex-shrink:0;}
  .level-name{font-size:13px;font-weight:600;color:var(--text-primary);}
  .level-range{font-size:11px;color:var(--text-tertiary);margin-top:2px;}
  .level-edit-row{
    padding:4px 0 12px 20px;
    border-left:1px dashed var(--border-subtle);
    margin-left:4px;
  }
  .level-edit-row label{
    font-size:10.5px;color:var(--text-secondary);display:block;margin-bottom:6px;line-height:1.4;
  }
  .level-edit-row input{
    width:100%;
    background:var(--bg-void);
    border:1px solid var(--border-subtle);
    color:var(--text-primary);
    font-family:'JetBrains Mono',monospace;
    font-size:14px;
    padding:8px 10px;
    border-radius:6px;
    outline:none;
  }
  .level-edit-row input:focus{border-color:var(--cool);}
  .level-edit-row select{
    width:100%;
    background:var(--bg-void);
    border:1px solid var(--border-subtle);
    color:var(--text-primary);
    font-family:'JetBrains Mono',monospace;
    font-size:14px;
    padding:8px 10px;
    border-radius:6px;
    outline:none;
  }
  .level-edit-row select:focus{border-color:var(--cool);}

  
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
    <div class="battery-indicator" id="batteryIndicator" title="Level baterai">
      <svg viewBox="0 0 44 22" width="40" height="20">
        <rect x="1" y="3" width="36" height="16" rx="3" fill="none" stroke="var(--text-secondary)" stroke-width="1.5"/>
        <rect x="38" y="8" width="4" height="6" rx="1" fill="var(--text-secondary)"/>
        <rect class="batt-bar" data-bar="1" x="4" y="6" width="5" height="10" rx="1"/>
        <rect class="batt-bar" data-bar="2" x="11" y="6" width="5" height="10" rx="1"/>
        <rect class="batt-bar" data-bar="3" x="18" y="6" width="5" height="10" rx="1"/>
        <rect class="batt-bar" data-bar="4" x="25" y="6" width="5" height="10" rx="1"/>
        <rect class="batt-bar" data-bar="5" x="32" y="6" width="3" height="10" rx="1"/>
      </svg>
      <span class="mono" id="battText">—%</span>
    </div>
    <div class="user-badge" id="userBadge" title="Klik untuk ubah nama/bahasa">
      <span id="userBadgeName">—</span>
    </div>
    <div class="status-line">
      <div id="statusText"><b data-i18n="status_connecting">Menghubungkan…</b></div>
      <div class="mono" id="hostText">host: —</div>
    </div>
  </header>

  <div class="banner" id="simBanner" data-i18n="sim_banner">◆ Mode simulasi aktif — sensor belum terhubung. Halaman menampilkan data uji untuk pratinjau tampilan.</div>

  <div class="grid">

    <!-- LEFT: thermal viewport -->
    <div class="panel">
      <div class="panel-title">
        <span data-i18n="panel_thermal_title">Citra Termal Live</span>
        <div style="display:flex;gap:8px;align-items:center;">
          <span class="mono" id="fpsText" style="color:var(--text-tertiary);font-weight:400;">— fps</span>
          <button id="btnMirror" class="ghost btn-tool active" title="Cerminkan gambar (kiri-kanan)">⇋ Mirror</button>
          <button id="btnCapture" class="ghost btn-tool" title="Bekukan gambar untuk diambil"><span data-i18n="btn_capture">📷 Foto</span></button>
          <button id="btnDownloadCaptured" class="ghost btn-tool" style="display:none;" title="Unduh gambar yang dibekukan"><span data-i18n="btn_download_captured">⬇ Unduh</span></button>
          <button id="btnBackLive" class="ghost btn-tool" style="display:none;" title="Kembali ke tampilan live"><span data-i18n="btn_back_live">↩ Kembali</span></button>
        </div>
      </div>
      <div class="viewport-wrap">
        <div class="viewport" id="viewportBox">
          <canvas id="thermalCanvas" width="320" height="240"></canvas>
          <div class="scanline"></div>
          <div class="marker hot" id="markHot"></div>
          <div class="marker cool" id="markCool"></div>
          <div class="marker center" id="markCenter"></div>
          <div class="marker-label hot" id="labelHot">—</div>
          <div class="marker-label cool" id="labelCool">—</div>
          <div class="marker-label center" id="labelCenter">—</div>
        </div>
        <div class="colorbar-row">
          <span class="colorbar-label mono" id="cbMin">—°C</span>
          <div class="colorbar"></div>
          <span class="colorbar-label mono" id="cbMax">—°C</span>
        </div>
      </div>

      <div class="readout-row" style="margin-top:16px;">
        <div class="stat-card hot">
          <div class="label" data-i18n="stat_max_label">Suhu Maksimum</div>
          <div class="value mono" id="tMax">—<sup>°C</sup></div>
          <div class="coord mono" id="tMaxCoord">baris —, kolom —</div>
        </div>
        <div class="stat-card cool">
          <div class="label" data-i18n="stat_min_label">Suhu Minimum</div>
          <div class="value mono" id="tMin">—<sup>°C</sup></div>
          <div class="coord mono" id="tMinCoord">baris —, kolom —</div>
        </div>
        <div class="stat-card center">
          <div class="label" data-i18n="stat_center_label">Suhu Tengah</div>
          <div class="value mono" id="tCenter">—<sup>°C</sup></div>
          <div class="coord mono" data-i18n="stat_center_coord">titik tengah citra</div>
        </div>
      </div>
    </div>

    <!-- RIGHT: threshold + controls -->
    <div>
      <div class="panel">
        <div class="panel-title"><span data-i18n="panel_threshold_title">Rentang Threshold</span></div>
        <div class="field-row">
          <div class="field">
            <label for="thLower" data-i18n="label_lower">Batas bawah (°C)</label>
            <input type="number" id="thLower" step="0.1" value="36">
          </div>
          <div class="field">
            <label for="thUpper" data-i18n="label_upper">Batas atas (°C)</label>
            <input type="number" id="thUpper" step="0.1" value="37">
          </div>
        </div>

        <div class="count-display">
          <div class="n mono" id="pixelCount">0</div>
          <div class="cap" data-i18n="count_caption">Piksel dalam rentang</div>
          <div class="range mono" id="rangeEcho">36.0°C – 37.0°C</div>
        </div>

        <div class="level-status-row" style="border-top:1px solid var(--border-faint);padding-top:12px;">
          <div>
            <div class="label" style="font-size:13px;color:var(--text-primary);" data-i18n="status_level_label">Status Level</div>
            <div class="level-badge" id="levelBadge">—</div>
          </div>
          <button class="ghost btn-tool" id="btnOpenSetting" title="Atur ambang notifikasi level">⚙ Setting</button>
        </div>
      </div>

      <div class="panel" style="margin-top:18px;">
        <div class="panel-title"><span data-i18n="panel_record_title">Perekaman Data</span></div>
        <p style="font-size:12px;color:var(--text-secondary);line-height:1.5;margin-bottom:14px;" data-i18n-html="record_desc_html">
          Setiap kali <b style="color:var(--text-primary);">Rekam</b> ditekan, jumlah piksel pada rentang saat ini disimpan sebagai baris baru bernomor urut pada log di bawah.
        </p>
        <div class="btn-row">
          <button class="primary" id="btnRecord" style="flex:1;"><span data-i18n="btn_record">● Rekam</span></button>
          <button class="danger-ghost" id="btnReset" data-i18n="btn_reset">Reset</button>
          <button class="ghost" id="btnExport" data-i18n="btn_export">Unduh CSV</button>
        </div>
      </div>
    </div>
  </div>

  <!-- LOG TABLE -->
  <div class="panel log-panel">
    <div class="panel-title">
      <span data-i18n="panel_log_title">Log Perekaman</span>
      <span class="mono" id="logCountText" style="color:var(--text-tertiary);font-weight:400;">0 entri</span>
    </div>
    <div class="log-scroll">
      <table>
        <thead>
          <tr>
            <th style="width:50px;" data-i18n="th_no">No.</th>
            <th data-i18n="th_time">Waktu</th>
            <th data-i18n="th_threshold">Threshold</th>
            <th data-i18n="th_pixelcount">Jumlah Piksel</th>
            <th data-i18n="th_condition">Kondisi</th>
            <th data-i18n="th_tmax">Suhu Max</th>
            <th data-i18n="th_tmin">Suhu Min</th>
          </tr>
        </thead>
        <tbody id="logBody">
          <tr class="empty-row"><td colspan="7" data-i18n="empty_log">Belum ada data yang direkam.</td></tr>
        </tbody>
      </table>
    </div>
  </div>

</div>

<div class="modal-overlay" id="settingOverlay">
  <div class="modal-box">
    <div class="modal-header">
      <span data-i18n="modal_setting_title">Setting</span>
      <button class="modal-close" id="btnCloseSetting">✕</button>
    </div>

    <div class="level-edit-row" style="border-left:none;margin-left:0;padding-left:0;">
      <label data-i18n="label_rotation">Rotasi gambar</label>
      <select id="rotationSelect">
        <option value="0">0°</option>
        <option value="90">90°</option>
        <option value="180">180°</option>
        <option value="270">270°</option>
      </select>
    </div>

    <div class="modal-header" style="font-size:13px;margin-top:18px;" data-i18n="modal_level_title">Ambang Notifikasi Level</div>
    <p class="modal-desc" data-i18n="modal_level_desc">Notifikasi akan dikirim setiap kali status level berubah dari satu tahap ke tahap lain.</p>

    <div class="level-row" data-level="0">
      <div class="level-dot" style="background:#4d535f;"></div>
      <div class="level-info">
        <div class="level-name" data-i18n="level_kering">Kering</div>
        <div class="level-range mono" id="rangeDisplay0">0 – 15 piksel</div>
      </div>
    </div>

    <div class="level-edit-row">
      <label data-i18n="label_breakA">Batas atas "Kering" / batas bawah "Mulai Terisi"</label>
      <input type="number" id="breakA" step="1" value="15">
    </div>

    <div class="level-row" data-level="1">
      <div class="level-dot" style="background:var(--cool);"></div>
      <div class="level-info">
        <div class="level-name" data-i18n="level_mulai_terisi">Mulai Terisi</div>
        <div class="level-range mono" id="rangeDisplay1">16 – 75 piksel</div>
      </div>
    </div>

    <div class="level-edit-row">
      <label data-i18n="label_breakB">Batas atas "Mulai Terisi" / batas bawah "Mulai Penuh"</label>
      <input type="number" id="breakB" step="1" value="75">
    </div>

    <div class="level-row" data-level="2">
      <div class="level-dot" style="background:var(--warn);"></div>
      <div class="level-info">
        <div class="level-name" data-i18n="level_mulai_penuh">Mulai Penuh</div>
        <div class="level-range mono" id="rangeDisplay2">76 – 135 piksel</div>
      </div>
    </div>

    <div class="level-edit-row">
      <label data-i18n="label_breakC">Batas atas "Mulai Penuh" / batas bawah "Sudah Penuh"</label>
      <input type="number" id="breakC" step="1" value="135">
    </div>

    <div class="level-row" data-level="3">
      <div class="level-dot" style="background:var(--hot);"></div>
      <div class="level-info">
        <div class="level-name" data-i18n="level_sudah_penuh">Sudah Penuh</div>
        <div class="level-range mono" id="rangeDisplay3">≥ 136 piksel</div>
      </div>
    </div>

    <div class="btn-row" style="margin-top:18px;">
      <button class="primary" id="btnSaveSetting" style="flex:1;" data-i18n="btn_save">Simpan</button>
      <button class="ghost" id="btnCancelSetting" data-i18n="btn_cancel">Batal</button>
    </div>
  </div>
</div>

<div class="modal-overlay" id="welcomeOverlay">
  <div class="modal-box">
    <div class="modal-header">
      <span data-i18n="welcome_title">Selamat Datang di THERMASCOPE</span>
    </div>
    <p class="modal-desc" data-i18n="welcome_desc">Isi nama kamu dan pilih bahasa untuk halaman monitoring ini.</p>

    <div class="level-edit-row" style="border-left:none;margin-left:0;padding-left:0;">
      <label data-i18n="welcome_name_label">Nama</label>
      <input type="text" id="welcomeName" data-i18n-ph="welcome_name_placeholder" placeholder="Masukkan nama kamu">
    </div>

    <div class="level-edit-row" style="border-left:none;margin-left:0;padding-left:0;">
      <label data-i18n="welcome_lang_label">Bahasa</label>
      <div class="lang-choice">
        <button type="button" class="lang-option active" id="langOptId" data-lang="id">Bahasa Indonesia</button>
        <button type="button" class="lang-option" id="langOptEn" data-lang="en">English</button>
      </div>
    </div>

    <div class="btn-row" style="margin-top:18px;">
      <button class="primary" id="btnWelcomeStart" style="flex:1;" data-i18n="welcome_start_btn">Mulai</button>
    </div>
  </div>
</div>

<script>
(function(){
  "use strict";

  const I18N = {
    id: {
      sim_banner: '◆ Mode simulasi aktif — sensor belum terhubung. Halaman menampilkan data uji untuk pratinjau tampilan.',
      panel_thermal_title: 'Citra Termal Live',
      btn_capture: '📷 Foto',
      btn_download_captured: '⬇ Unduh',
      btn_back_live: '↩ Kembali',
      stat_max_label: 'Suhu Maksimum',
      stat_min_label: 'Suhu Minimum',
      stat_center_label: 'Suhu Tengah',
      stat_center_coord: 'titik tengah citra',
      panel_threshold_title: 'Rentang Threshold',
      label_lower: 'Batas bawah (°C)',
      label_upper: 'Batas atas (°C)',
      count_caption: 'Piksel dalam rentang',
      status_level_label: 'Status Level',
      panel_record_title: 'Perekaman Data',
      record_desc_html: 'Setiap kali <b style="color:var(--text-primary);">Rekam</b> ditekan, jumlah piksel pada rentang saat ini disimpan sebagai baris baru bernomor urut pada log di bawah.',
      btn_record: '● Rekam',
      btn_reset: 'Reset',
      btn_export: 'Unduh CSV',
      panel_log_title: 'Log Perekaman',
      th_no: 'No.', th_time: 'Waktu', th_threshold: 'Threshold', th_pixelcount: 'Jumlah Piksel',
      th_condition: 'Kondisi', th_tmax: 'Suhu Max', th_tmin: 'Suhu Min',
      empty_log: 'Belum ada data yang direkam.',
      modal_setting_title: 'Setting',
      label_rotation: 'Rotasi gambar',
      modal_level_title: 'Ambang Notifikasi Level',
      modal_level_desc: 'Notifikasi akan dikirim setiap kali status level berubah dari satu tahap ke tahap lain.',
      level_kering: 'Kering', level_mulai_terisi: 'Mulai Terisi', level_mulai_penuh: 'Mulai Penuh', level_sudah_penuh: 'Sudah Penuh',
      label_breakA: 'Batas atas "Kering" / batas bawah "Mulai Terisi"',
      label_breakB: 'Batas atas "Mulai Terisi" / batas bawah "Mulai Penuh"',
      label_breakC: 'Batas atas "Mulai Penuh" / batas bawah "Sudah Penuh"',
      btn_save: 'Simpan', btn_cancel: 'Batal',
      status_connecting: 'Menghubungkan…',
      status_connected: 'Terhubung ke sensor',
      status_disconnected: 'Koneksi terputus',
      status_sim: 'Mode simulasi (sensor tidak terdeteksi)',
      battery_title: 'Level baterai',
      word_row: 'baris', word_col: 'kolom',
      toast_level_title: 'Status level berubah',
      toast_level_body: 'Sekarang: {level} ({count} piksel)',
      toast_invalid_title: 'Nilai tidak valid',
      toast_invalid_body: 'Pastikan urutan batas: Kering < Mulai Terisi < Mulai Penuh.',
      toast_nodata_title: 'Belum ada data',
      toast_nodata_body: 'Log masih kosong, tidak ada yang bisa diunduh.',
      toast_notready_title: 'Belum siap',
      toast_notready_body: 'Tunggu data sensor termuat dulu.',
      confirm_reset_log: 'Reset seluruh log perekaman? Tindakan ini tidak dapat dibatalkan.',
      welcome_title: 'Selamat Datang di THERMASCOPE',
      welcome_desc: 'Isi nama kamu dan pilih bahasa untuk halaman monitoring ini.',
      welcome_name_label: 'Nama',
      welcome_name_placeholder: 'Masukkan nama kamu',
      welcome_lang_label: 'Bahasa',
      welcome_start_btn: 'Mulai',
      log_entries_suffix: 'entri',
      csv_operator_label: 'Operator',
      watermark_by: 'oleh'
    },
    en: {
      sim_banner: '◆ Simulation mode active — sensor not connected. This page is showing test data for preview.',
      panel_thermal_title: 'Live Thermal Image',
      btn_capture: '📷 Capture',
      btn_download_captured: '⬇ Download',
      btn_back_live: '↩ Back',
      stat_max_label: 'Max Temperature',
      stat_min_label: 'Min Temperature',
      stat_center_label: 'Center Temperature',
      stat_center_coord: 'center of image',
      panel_threshold_title: 'Threshold Range',
      label_lower: 'Lower limit (°C)',
      label_upper: 'Upper limit (°C)',
      count_caption: 'Pixels in range',
      status_level_label: 'Level Status',
      panel_record_title: 'Data Recording',
      record_desc_html: 'Each time <b style="color:var(--text-primary);">Record</b> is pressed, the current pixel count is saved as a new numbered row in the log below.',
      btn_record: '● Record',
      btn_reset: 'Reset',
      btn_export: 'Download CSV',
      panel_log_title: 'Recording Log',
      th_no: 'No.', th_time: 'Time', th_threshold: 'Threshold', th_pixelcount: 'Pixel Count',
      th_condition: 'Condition', th_tmax: 'Max Temp', th_tmin: 'Min Temp',
      empty_log: 'No data recorded yet.',
      modal_setting_title: 'Settings',
      label_rotation: 'Image rotation',
      modal_level_title: 'Level Notification Thresholds',
      modal_level_desc: 'A notification is sent every time the level status changes from one stage to another.',
      level_kering: 'Dry', level_mulai_terisi: 'Starting to Fill', level_mulai_penuh: 'Nearly Full', level_sudah_penuh: 'Full',
      label_breakA: 'Upper limit "Dry" / lower limit "Starting to Fill"',
      label_breakB: 'Upper limit "Starting to Fill" / lower limit "Nearly Full"',
      label_breakC: 'Upper limit "Nearly Full" / lower limit "Full"',
      btn_save: 'Save', btn_cancel: 'Cancel',
      status_connecting: 'Connecting…',
      status_connected: 'Connected to sensor',
      status_disconnected: 'Connection lost',
      status_sim: 'Simulation mode (sensor not detected)',
      battery_title: 'Battery level',
      word_row: 'row', word_col: 'col',
      toast_level_title: 'Level status changed',
      toast_level_body: 'Now: {level} ({count} pixels)',
      toast_invalid_title: 'Invalid value',
      toast_invalid_body: 'Make sure the order is correct: Dry < Starting to Fill < Nearly Full.',
      toast_nodata_title: 'No data yet',
      toast_nodata_body: 'The log is still empty, nothing to download.',
      toast_notready_title: 'Not ready',
      toast_notready_body: 'Please wait for sensor data to load first.',
      confirm_reset_log: 'Reset the entire recording log? This action cannot be undone.',
      welcome_title: 'Welcome to THERMASCOPE',
      welcome_desc: 'Enter your name and choose a language for this monitoring page.',
      welcome_name_label: 'Name',
      welcome_name_placeholder: 'Enter your name',
      welcome_lang_label: 'Language',
      welcome_start_btn: 'Start',
      log_entries_suffix: 'entries',
      csv_operator_label: 'Operator',
      watermark_by: 'by'
    }
  };

  let currentLang = localStorage.getItem('thermascope_lang') || 'id';
  let userName = localStorage.getItem('thermascope_name') || '';

  function t(key){
    return (I18N[currentLang] && I18N[currentLang][key]) || (I18N.id[key]) || key;
  }

  function applyI18n(){
    document.documentElement.lang = currentLang;
    document.querySelectorAll('[data-i18n]').forEach((el)=>{
      const key = el.getAttribute('data-i18n');
      el.textContent = t(key);
    });
    document.querySelectorAll('[data-i18n-html]').forEach((el)=>{
      const key = el.getAttribute('data-i18n-html');
      el.innerHTML = t(key);
    });
    document.querySelectorAll('[data-i18n-ph]').forEach((el)=>{
      const key = el.getAttribute('data-i18n-ph');
      el.placeholder = t(key);
    });
    const battIndicator = document.getElementById('batteryIndicator');
    if(battIndicator) battIndicator.title = t('battery_title');
    updateLevelRangeDisplay();
  }

  const GRID_W = 32, GRID_H = 24, N_PIX = GRID_W*GRID_H;
  const UPSCALE = 10;

  let latestFrame = new Float32Array(N_PIX).fill(25);
  let ws = null, wsRetryTimer = null, demoTimer = null, isDemo = false, isConnected = false;
  let records = [], recordCounter = 0;
  let lastFrameTime = performance.now(), fpsSmooth = 0;
  let mirrored = true, rotation = 0;
  let lastSnapshot = null;
  let frozen = false;
  const BATT_EMPTY_V = 5.4, BATT_FULL_V = 8.4;

  const $ = (id)=>document.getElementById(id);
  const canvas = $('thermalCanvas');
  const ctx = canvas.getContext('2d');
  const offCanvas = document.createElement('canvas');
  offCanvas.width = GRID_W; offCanvas.height = GRID_H;
  const offCtx = offCanvas.getContext('2d');

  const statusDot = $('statusDot'), statusText = $('statusText'), hostText = $('hostText');
  const simBanner = $('simBanner'), fpsText = $('fpsText');
  const viewportBox = $('viewportBox');
  const btnMirror = $('btnMirror');
  const thLowerEl = $('thLower'), thUpperEl = $('thUpper');
  const pixelCountEl = $('pixelCount'), rangeEchoEl = $('rangeEcho');
  const tMaxEl = $('tMax'), tMinEl = $('tMin'), tMaxCoordEl = $('tMaxCoord'), tMinCoordEl = $('tMinCoord');
  const tCenterEl = $('tCenter');
  const cbMinEl = $('cbMin'), cbMaxEl = $('cbMax');
  const markHot = $('markHot'), markCool = $('markCool'), labelHot = $('labelHot'), labelCool = $('labelCool');
  const markCenter = $('markCenter'), labelCenter = $('labelCenter');
  const btnCapture = $('btnCapture'), btnDownloadCaptured = $('btnDownloadCaptured'), btnBackLive = $('btnBackLive');
  const levelBadge = $('levelBadge'), btnOpenSetting = $('btnOpenSetting');
  const settingOverlay = $('settingOverlay'), btnCloseSetting = $('btnCloseSetting'), btnCancelSetting = $('btnCancelSetting'), btnSaveSetting = $('btnSaveSetting');
  const breakAEl = $('breakA'), breakBEl = $('breakB'), breakCEl = $('breakC');
  const rotationSelect = $('rotationSelect');
  const welcomeOverlay = $('welcomeOverlay'), welcomeNameEl = $('welcomeName'), btnWelcomeStart = $('btnWelcomeStart');
  const langOptId = $('langOptId'), langOptEn = $('langOptEn');
  const userBadge = $('userBadge'), userBadgeName = $('userBadgeName');
  let selectedWelcomeLang = currentLang;
  const rangeDisplay0 = $('rangeDisplay0'), rangeDisplay1 = $('rangeDisplay1'), rangeDisplay2 = $('rangeDisplay2'), rangeDisplay3 = $('rangeDisplay3');
  const btnRecord = $('btnRecord'), btnReset = $('btnReset'), btnExport = $('btnExport');
  const logBody = $('logBody'), logCountText = $('logCountText');

  hostText.textContent = 'host: ' + (location.host || 'lokal');

  const battText = $('battText');
  const battBars = document.querySelectorAll('.batt-bar');

  function updateBattery(voltage){
    const pct = Math.max(0, Math.min(100, ((voltage-BATT_EMPTY_V)/(BATT_FULL_V-BATT_EMPTY_V))*100));
    const barsOn = pct<=0 ? 0 : Math.ceil(pct/20);
    let tier = 'ok';
    if(barsOn<=1) tier='danger';
    else if(barsOn<=2) tier='warn';
    battBars.forEach((bar)=>{
      const idx = parseInt(bar.getAttribute('data-bar'));
      bar.classList.toggle('on', idx<=barsOn);
      bar.classList.toggle('warn', idx<=barsOn && tier==='warn');
      bar.classList.toggle('danger', idx<=barsOn && tier==='danger');
    });
    battText.textContent = Math.round(pct)+'% · '+voltage.toFixed(2)+'V';
  }

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

  function mapPoint(row, col, w, h, mirror, rot){
    let r = row, c = col, cw = w, ch = h;
    if(mirror){ c = cw-1-c; }
    const steps = ((rot/90)%4+4)%4;
    for(let s=0;s<steps;s++){
      const newRow = c;
      const newCol = ch-1-r;
      r = newRow; c = newCol;
      const nw = ch, nh = cw;
      cw = nw; ch = nh;
    }
    return {row:r, col:c};
  }

  function transformDisplay(canon, w, h, mirror, rot){
    let cur = canon, cw = w, ch = h;
    if(mirror){
      const out = new Float32Array(cw*ch);
      for(let r=0;r<ch;r++){
        for(let c=0;c<cw;c++){
          out[r*cw+c] = cur[r*cw + (cw-1-c)];
        }
      }
      cur = out;
    }
    const steps = ((rot/90)%4+4)%4;
    for(let s=0;s<steps;s++){
      const nw = ch, nh = cw;
      const out = new Float32Array(nw*nh);
      for(let r=0;r<ch;r++){
        for(let c=0;c<cw;c++){
          const nr = c;
          const nc = ch-1-r;
          out[nr*nw+nc] = cur[r*cw+c];
        }
      }
      cur = out; cw = nw; ch = nh;
    }
    return {data:cur, w:cw, h:ch};
  }

  function renderFrame(rawFrame){
    if(frozen) return;
    const canon = new Float32Array(rawFrame.length);
    for(let r=0;r<GRID_H;r++){
      const srcRow = GRID_H-1-r;
      for(let c=0;c<GRID_W;c++){
        const srcCol = GRID_W-1-c;
        canon[r*GRID_W+c] = rawFrame[srcRow*GRID_W+srcCol];
      }
    }

    let min = Infinity, max = -Infinity, minIdx=0, maxIdx=0;
    for(let i=0;i<canon.length;i++){
      const v = canon[i];
      if(v<min){min=v;minIdx=i;}
      if(v>max){max=v;maxIdx=i;}
    }
    const span = (max-min) || 1;

    const disp = transformDisplay(canon, GRID_W, GRID_H, mirrored, rotation);

    if(offCanvas.width !== disp.w || offCanvas.height !== disp.h){
      offCanvas.width = disp.w; offCanvas.height = disp.h;
    }
    const targetCw = disp.w*UPSCALE, targetCh = disp.h*UPSCALE;
    if(canvas.width !== targetCw || canvas.height !== targetCh){
      canvas.width = targetCw; canvas.height = targetCh;
      viewportBox.style.aspectRatio = disp.w+' / '+disp.h;
    }

    const imgData = offCtx.createImageData(disp.w, disp.h);
    for(let i=0;i<disp.data.length;i++){
      const t = (disp.data[i]-min)/span;
      const [r,g,b] = colormap(t);
      imgData.data[i*4+0]=r; imgData.data[i*4+1]=g; imgData.data[i*4+2]=b; imgData.data[i*4+3]=255;
    }
    offCtx.putImageData(imgData,0,0);

    ctx.imageSmoothingEnabled = true;
    ctx.imageSmoothingQuality = 'high';
    ctx.clearRect(0,0,canvas.width,canvas.height);
    ctx.drawImage(offCanvas, 0,0,disp.w,disp.h, 0,0,canvas.width,canvas.height);

    tMaxEl.innerHTML = max.toFixed(1)+'<sup>°C</sup>';
    tMinEl.innerHTML = min.toFixed(1)+'<sup>°C</sup>';
    const maxRow = Math.floor(maxIdx/GRID_W), maxCol = maxIdx%GRID_W;
    const minRow = Math.floor(minIdx/GRID_W), minCol = minIdx%GRID_W;
    tMaxCoordEl.textContent = `${t('word_row')} ${maxRow+1}, ${t('word_col')} ${maxCol+1}`;
    tMinCoordEl.textContent = `${t('word_row')} ${minRow+1}, ${t('word_col')} ${minCol+1}`;
    cbMinEl.textContent = min.toFixed(1)+'°C';
    cbMaxEl.textContent = max.toFixed(1)+'°C';

    const centerRow = Math.floor(GRID_H/2), centerCol = Math.floor(GRID_W/2);
    const centerTemp = canon[centerRow*GRID_W+centerCol];
    tCenterEl.innerHTML = centerTemp.toFixed(1)+'<sup>°C</sup>';

    const maxPos = mapPoint(maxRow, maxCol, GRID_W, GRID_H, mirrored, rotation);
    const minPos = mapPoint(minRow, minCol, GRID_W, GRID_H, mirrored, rotation);
    positionMarker(markHot, labelHot, maxPos.col, maxPos.row, disp.w, disp.h, max.toFixed(1)+'°C');
    positionMarker(markCool, labelCool, minPos.col, minPos.row, disp.w, disp.h, min.toFixed(1)+'°C');
    positionMarker(markCenter, labelCenter, (disp.w-1)/2, (disp.h-1)/2, disp.w, disp.h, centerTemp.toFixed(1)+'°C');

    lastSnapshot = {
      minText: min.toFixed(1)+'°C', maxText: max.toFixed(1)+'°C', centerText: centerTemp.toFixed(1)+'°C',
      hotXPct: ((maxPos.col+0.5)/disp.w)*100, hotYPct: ((maxPos.row+0.5)/disp.h)*100,
      coolXPct: ((minPos.col+0.5)/disp.w)*100, coolYPct: ((minPos.row+0.5)/disp.h)*100
    };

    updateThresholdCount(canon, max, min);

    const now = performance.now();
    const inst = 1000/(now-lastFrameTime);
    fpsSmooth = fpsSmooth ? fpsSmooth*0.8 + inst*0.2 : inst;
    lastFrameTime = now;
    fpsText.textContent = fpsSmooth.toFixed(1)+' fps';
  }

  function positionMarker(markEl, labelEl, col, row, w, h, text){
    const xPct = ((col+0.5)/w)*100;
    const yPct = ((row+0.5)/h)*100;
    markEl.style.left = xPct+'%';
    markEl.style.top = yPct+'%';
    labelEl.style.left = xPct+'%';
    labelEl.style.top = yPct+'%';
    labelEl.textContent = text;
  }

  function drawCrosshair(octx, x, y, color, text, labelAbove){
    octx.strokeStyle = color;
    octx.lineWidth = 2;
    octx.beginPath();
    octx.moveTo(x-9, y); octx.lineTo(x+9, y);
    octx.moveTo(x, y-9); octx.lineTo(x, y+9);
    octx.stroke();
    octx.font = 'bold 13px monospace';
    const textW = octx.measureText(text).width;
    const boxW = textW+12, boxH = 20;
    const boxX = x-boxW/2, boxY = labelAbove ? (y-32) : (y+12);
    octx.fillStyle = 'rgba(10,12,16,0.75)';
    octx.fillRect(boxX, boxY, boxW, boxH);
    octx.strokeStyle = color;
    octx.lineWidth = 1;
    octx.strokeRect(boxX, boxY, boxW, boxH);
    octx.fillStyle = color;
    octx.textAlign = 'center';
    octx.textBaseline = 'middle';
    octx.fillText(text, x, boxY+boxH/2);
    octx.textAlign = 'left';
    octx.textBaseline = 'alphabetic';
  }

  function captureSnapshot(){
    if(!lastSnapshot){ showToast(t('toast_notready_title'), t('toast_notready_body')); return; }
    const W = canvas.width, H = canvas.height;
    const barH = 70;
    const out = document.createElement('canvas');
    out.width = W; out.height = H+barH;
    const octx = out.getContext('2d');

    octx.fillStyle = '#0a0c10';
    octx.fillRect(0,0,out.width,out.height);
    octx.drawImage(canvas, 0, 0, W, H);

    drawCrosshair(octx, lastSnapshot.hotXPct/100*W, lastSnapshot.hotYPct/100*H, '#ff5a3c', lastSnapshot.maxText, true);
    drawCrosshair(octx, lastSnapshot.coolXPct/100*W, lastSnapshot.coolYPct/100*H, '#3fd1c5', lastSnapshot.minText, true);
    drawCrosshair(octx, W/2, H/2, '#f0f2f6', lastSnapshot.centerText, false);

    const gradW = W-140, gradX = 70, gradY = H+22;
    const grad = octx.createLinearGradient(gradX, 0, gradX+gradW, 0);
    grad.addColorStop(0,'#000000'); grad.addColorStop(0.14,'#1e0033'); grad.addColorStop(0.28,'#6a0d78');
    grad.addColorStop(0.42,'#c31b6e'); grad.addColorStop(0.56,'#e8382e'); grad.addColorStop(0.70,'#ff8a1f');
    grad.addColorStop(0.84,'#ffd63f'); grad.addColorStop(1,'#ffffff');
    octx.fillStyle = grad;
    octx.fillRect(gradX, gradY, gradW, 8);
    octx.font = '11px monospace';
    octx.fillStyle = '#8c93a3';
    octx.textAlign = 'right';
    octx.fillText(lastSnapshot.minText, gradX-8, gradY+8);
    octx.textAlign = 'left';
    octx.fillText(lastSnapshot.maxText, gradX+gradW+8, gradY+8);
    octx.textAlign = 'left';
    octx.font = '10px monospace';
    octx.fillStyle = '#4d535f';
    const watermarkName = userName ? (' · '+t('watermark_by')+' '+userName) : '';
    octx.fillText('THERMASCOPE' + watermarkName + ' · ' + new Date().toLocaleString(currentLang==='id'?'id-ID':'en-US'), 12, H+55);

    out.toBlob((blob)=>{
      const url = URL.createObjectURL(blob);
      const a = document.createElement('a');
      const ts = new Date().toISOString().replace(/[:.]/g,'-');
      const safeName = (userName||'user').toLowerCase().replace(/[^a-z0-9]+/g,'-').replace(/^-+|-+$/g,'');
      a.href = url; a.download = `thermascope_${safeName}_${ts}.png`;
      a.click();
      URL.revokeObjectURL(url);
    }, 'image/png');
  }
  btnCapture.addEventListener('click', ()=>{
    frozen = true;
    btnCapture.style.display = 'none';
    btnDownloadCaptured.style.display = '';
    btnBackLive.style.display = '';
    btnMirror.disabled = true;
    rotationSelect.disabled = true;
    viewportBox.classList.add('frozen');
  });
  btnBackLive.addEventListener('click', ()=>{
    frozen = false;
    btnCapture.style.display = '';
    btnDownloadCaptured.style.display = 'none';
    btnBackLive.style.display = 'none';
    btnMirror.disabled = false;
    rotationSelect.disabled = false;
    viewportBox.classList.remove('frozen');
  });
  btnDownloadCaptured.addEventListener('click', captureSnapshot);

  btnMirror.addEventListener('click', ()=>{
    mirrored = !mirrored;
    btnMirror.classList.toggle('active', mirrored);
    renderFrame(latestFrame);
  });
  rotationSelect.addEventListener('change', ()=>{
    rotation = parseInt(rotationSelect.value);
    renderFrame(latestFrame);
  });

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
  let levelBreaks = {a:15, b:75, c:135};
  let currentLevel = null;

  const LEVELS = [
    {key:'level_kering', color:'#4d535f'},
    {key:'level_mulai_terisi', color:'var(--cool)'},
    {key:'level_mulai_penuh', color:'var(--warn)'},
    {key:'level_sudah_penuh', color:'var(--hot)'}
  ];
  function levelName(idx){ return t(LEVELS[idx].key); }

  function getLevelIndex(count){
    if(count <= levelBreaks.a) return 0;
    if(count <= levelBreaks.b) return 1;
    if(count <= levelBreaks.c) return 2;
    return 3;
  }

  function updateLevelRangeDisplay(){
    rangeDisplay0.textContent = `0 – ${levelBreaks.a} piksel`;
    rangeDisplay1.textContent = `${levelBreaks.a+1} – ${levelBreaks.b} piksel`;
    rangeDisplay2.textContent = `${levelBreaks.b+1} – ${levelBreaks.c} piksel`;
    rangeDisplay3.textContent = `≥ ${levelBreaks.c+1} piksel`;
  }

  function checkAlert(count){
    const idx = getLevelIndex(count);
    levelBadge.textContent = levelName(idx);
    levelBadge.style.color = LEVELS[idx].color;
    if(currentLevel === null){
      currentLevel = idx;
      return;
    }
    if(idx !== currentLevel){
      fireAlert(levelName(idx), count);
      currentLevel = idx;
    }
  }

  function fireAlert(lvlNameStr, count){
    showToast(t('toast_level_title'), t('toast_level_body').replace('{level}', lvlNameStr).replace('{count}', count));
    if('Notification' in window && Notification.permission === 'granted'){
      new Notification('THERMASCOPE — ' + lvlNameStr, {
        body: t('toast_level_body').replace('{level}', lvlNameStr).replace('{count}', count)
      });
    }
    beep();
    if(ws && ws.readyState === WebSocket.OPEN){
      ws.send('blink');
    }
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

  function openSetting(){
    breakAEl.value = levelBreaks.a;
    breakBEl.value = levelBreaks.b;
    breakCEl.value = levelBreaks.c;
    rotationSelect.value = rotation;
    settingOverlay.classList.add('show');
    if('Notification' in window && Notification.permission === 'default' && !notifyPermissionAsked){
      notifyPermissionAsked = true;
      Notification.requestPermission();
    }
  }
  function closeSetting(){
    settingOverlay.classList.remove('show');
  }

  function openWelcome(){
    welcomeNameEl.value = userName;
    selectedWelcomeLang = currentLang;
    langOptId.classList.toggle('active', selectedWelcomeLang==='id');
    langOptEn.classList.toggle('active', selectedWelcomeLang==='en');
    welcomeOverlay.classList.add('show');
  }
  function updateUserBadge(){
    userBadgeName.textContent = userName || (currentLang==='id' ? 'Tamu' : 'Guest');
  }
  langOptId.addEventListener('click', ()=>{
    selectedWelcomeLang = 'id';
    langOptId.classList.add('active'); langOptEn.classList.remove('active');
  });
  langOptEn.addEventListener('click', ()=>{
    selectedWelcomeLang = 'en';
    langOptEn.classList.add('active'); langOptId.classList.remove('active');
  });
  btnWelcomeStart.addEventListener('click', ()=>{
    userName = welcomeNameEl.value.trim();
    currentLang = selectedWelcomeLang;
    localStorage.setItem('thermascope_name', userName);
    localStorage.setItem('thermascope_lang', currentLang);
    applyI18n();
    updateUserBadge();
    welcomeOverlay.classList.remove('show');
  });
  userBadge.addEventListener('click', openWelcome);

  btnOpenSetting.addEventListener('click', openSetting);
  btnCloseSetting.addEventListener('click', closeSetting);
  btnCancelSetting.addEventListener('click', closeSetting);
  settingOverlay.addEventListener('click', (e)=>{ if(e.target===settingOverlay) closeSetting(); });
  btnSaveSetting.addEventListener('click', ()=>{
    const a = parseInt(breakAEl.value), b = parseInt(breakBEl.value), c = parseInt(breakCEl.value);
    if(isNaN(a) || isNaN(b) || isNaN(c) || !(a < b && b < c)){
      showToast(t('toast_invalid_title'), t('toast_invalid_body'));
      return;
    }
    levelBreaks = {a, b, c};
    updateLevelRangeDisplay();
    currentLevel = null;
    closeSetting();
  });
  updateLevelRangeDisplay();

  function addRecord(){
    recordCounter++;
    const now = new Date();
    const time = now.toLocaleTimeString('id-ID', {hour:'2-digit',minute:'2-digit',second:'2-digit'});
    let tmax=-Infinity, tmin=Infinity;
    for(const v of latestFrame){ if(v>tmax)tmax=v; if(v<tmin)tmin=v; }
    const lvlIdx = getLevelIndex(lastCount);
    const entry = {
      no: recordCounter,
      time,
      lower: parseFloat(thLowerEl.value).toFixed(1),
      upper: parseFloat(thUpperEl.value).toFixed(1),
      count: lastCount,
      condition: levelName(lvlIdx),
      conditionColor: LEVELS[lvlIdx].color,
      tmax: tmax.toFixed(1),
      tmin: tmin.toFixed(1)
    };
    records.push(entry);
    renderLog();
  }
  function renderLog(){
    if(records.length===0){
      logBody.innerHTML = '<tr class="empty-row"><td colspan="7">'+t('empty_log')+'</td></tr>';
    } else {
      logBody.innerHTML = records.map(r=>`
        <tr>
          <td>${r.no}</td>
          <td>${r.time}</td>
          <td>${r.lower}°C – ${r.upper}°C</td>
          <td style="color:var(--warn);font-weight:600;">${r.count}</td>
          <td style="color:${r.conditionColor};font-weight:600;">${r.condition}</td>
          <td style="color:var(--hot);">${r.tmax}°C</td>
          <td style="color:var(--cool);">${r.tmin}°C</td>
        </tr>`).join('');
    }
    logCountText.textContent = records.length+' '+t('log_entries_suffix');
  }
  btnRecord.addEventListener('click', addRecord);
  btnReset.addEventListener('click', ()=>{
    if(records.length===0) return;
    if(confirm(t('confirm_reset_log'))){
      records = []; recordCounter = 0; renderLog();
    }
  });
  btnExport.addEventListener('click', ()=>{
    if(records.length===0){ showToast(t('toast_nodata_title'), t('toast_nodata_body')); return; }
    const operatorLine = t('csv_operator_label')+': '+(userName||'-')+'\n\n';
    const header = [t('th_no'),t('th_time'),'Threshold Bawah','Threshold Atas',t('th_pixelcount'),t('th_condition'),t('th_tmax'),t('th_tmin')].join(',')+'\n';
    const rows = records.map(r=>`${r.no},${r.time},${r.lower},${r.upper},${r.count},${r.condition},${r.tmax},${r.tmin}`).join('\n');
    const blob = new Blob([operatorLine+header+rows], {type:'text/csv;charset=utf-8;'});
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    const safeName = (userName||'user').toLowerCase().replace(/[^a-z0-9]+/g,'-').replace(/^-+|-+$/g,'');
    a.href = url; a.download = `thermascope_log_${safeName}.csv`;
    a.click();
    URL.revokeObjectURL(url);
  });

  function setStatus(mode, key){
    statusDot.className = 'brand-mark ' + mode;
    statusText.innerHTML = '<b>'+t(key)+'</b>';
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
    setStatus('off','status_connecting');

    const connectTimeout = setTimeout(()=>{
      if(!isConnected) scheduleDemoFallback();
    }, 3000);

    ws.onopen = ()=>{
      clearTimeout(connectTimeout);
      isConnected = true; isDemo = false;
      stopDemo();
      setStatus('live','status_connected');
    };
    ws.onmessage = (evt)=>{
      try{
        const parsed = JSON.parse(evt.data);
        const arr = Array.isArray(parsed) ? parsed : parsed.data;
        if(Array.isArray(arr) && arr.length===N_PIX){
          latestFrame = Float32Array.from(arr);
          renderFrame(latestFrame);
        }
        if(!Array.isArray(parsed) && typeof parsed.batt === 'number'){
          updateBattery(parsed.batt);
        }
      }catch(e){  }
    };
    ws.onclose = ()=>{
      isConnected = false;
      setStatus('off','status_disconnected');
      clearTimeout(connectTimeout);
      wsRetryTimer = setTimeout(connectWS, 4000);
      scheduleDemoFallback();
    };
    ws.onerror = ()=>{  };
  }

  function scheduleDemoFallback(){
    if(isDemo || isConnected) return;
    isDemo = true;
    setStatus('sim','status_sim');
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
      updateBattery(7.6 + Math.sin(demoT*0.1)*0.6);
    }, 200);
  }
  function stopDemo(){
    if(demoTimer){ clearInterval(demoTimer); demoTimer = null; }
  }

  applyI18n();
  updateUserBadge();
  if(!userName){
    openWelcome();
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
  } else if (type == WS_EVT_DATA) {
    blinkRequestPending = true;
  }
}

void sendIndexPage(AsyncWebServerRequest *request) {
  AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", (const uint8_t*)INDEX_HTML, strlen_P(INDEX_HTML));
  request->send(response);
}

void setup() {
  Serial.begin(115200);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  analogReadResolution(12);
  analogSetPinAttenuation(BATTERY_ADC_PIN, ADC_11db);

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

  bool wifiConnected = (WiFi.softAPgetStationNum() > 0);

  if (blinkRequestPending) {
    blinkRequestPending = false;
    notifBlinking = true;
    notifBlinkToggles = 4;
    lastLedToggle = millis();
  }

  if (notifBlinking) {
    if (millis() - lastLedToggle >= LED_BLINK_FAST_MS) {
      lastLedToggle = millis();
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState ? HIGH : LOW);
      notifBlinkToggles--;
      if (notifBlinkToggles <= 0) {
        notifBlinking = false;
      }
    }
  } else if (wifiConnected) {
    if (!ledState) {
      ledState = true;
      digitalWrite(LED_PIN, HIGH);
    }
  } else {
    if (millis() - lastLedToggle >= LED_BLINK_SLOW_MS) {
      lastLedToggle = millis();
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState ? HIGH : LOW);
    }
  }

  if (millis() - lastFrameSent < FRAME_INTERVAL_MS) return;

  if (millis() - lastBatteryRead >= BATTERY_READ_INTERVAL_MS) {
    lastBatteryRead = millis();
    batteryVoltage = readBatteryVoltage();
  }

  if (mlx.getFrame(frame) != 0) {
    return;
  }
  lastFrameSent = millis();

  if (ws.count() == 0) return;

  String payload;
  payload.reserve(768 * 6 + 32);
  payload += "{\"data\":[";
  for (int i = 0; i < 32*24; i++) {
    payload += String(frame[i], 1);
    if (i < 32*24 - 1) payload += ',';
  }
  payload += "],\"batt\":";
  payload += String(batteryVoltage, 2);
  payload += "}";

  ws.textAll(payload);
}
