/*
  RC Boat Controller - ESP32
  ===========================
  Hotspot: RCBoat_Controller
  Password: 12345678
  IP: 192.168.4.1

  Pin Assignment:
  - ENA (Motor Kiri PWM)  : GPIO 26
  - ENB (Motor Kanan PWM) : GPIO 25
  - IN1 (Motor Kiri +)    : GPIO 33
  - IN2 (Motor Kiri -)    : GPIO 32
  - IN3 (Motor Kanan +)   : GPIO 16
  - IN4 (Motor Kanan -)   : GPIO 17
*/

#include <WiFi.h>
#include <WebServer.h>

// WiFi Hotspot
const char* ssid     = "RCBoat_Controller";
const char* password = "12345678";
IPAddress local_ip(192, 168, 4, 1);
IPAddress gateway(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);

// Pin Definition
#define ENA 26
#define IN1 33
#define IN2 32
#define ENB 25
#define IN3 16
#define IN4 17

// PWM Config
#define PWM_FREQ 1000
#define PWM_RES  8

WebServer server(80);

void setMotorLeft(int spd) {
  spd = constrain(spd, -255, 255);
  if (spd > 0)      { digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);  ledcWrite(ENA, spd);  }
  else if (spd < 0) { digitalWrite(IN1, LOW);  digitalWrite(IN2, HIGH); ledcWrite(ENA, -spd); }
  else              { digitalWrite(IN1, LOW);  digitalWrite(IN2, LOW);  ledcWrite(ENA, 0);    }
}

void setMotorRight(int spd) {
  spd = constrain(spd, -255, 255);
  if (spd > 0)      { digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);  ledcWrite(ENB, spd);  }
  else if (spd < 0) { digitalWrite(IN3, LOW);  digitalWrite(IN4, HIGH); ledcWrite(ENB, -spd); }
  else              { digitalWrite(IN3, LOW);  digitalWrite(IN4, LOW);  ledcWrite(ENB, 0);    }
}

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="id">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
<title>RC BOAT</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{background:#0a0c10;font-family:'Share Tech Mono',monospace;touch-action:none;overflow:hidden;height:100vh;display:flex;flex-direction:column}
.topbar{display:flex;justify-content:space-between;align-items:center;padding:5px 10px;border-bottom:1px solid #1a3a1a;background:#060a06;flex-shrink:0}
.tb-l{font-size:10px;color:#2a7a2a;letter-spacing:3px}
.tb-r{font-size:10px;color:#1a4a1a;letter-spacing:2px}
.tb-r span{color:#3aff3a}
.blink{animation:blink 1s infinite}
@keyframes blink{0%,100%{opacity:1}50%{opacity:0}}
.cam-feed{flex:1;background:#030503;border:1px solid #0d1f0d;margin:8px 8px 4px 8px;border-radius:3px;position:relative;display:flex;align-items:center;justify-content:center;overflow:hidden}
.cam-feed img{width:100%;height:100%;object-fit:cover;display:block}
.cam-corner{position:absolute;width:16px;height:16px;border-color:#1a4a1a;border-style:solid;z-index:2}
.tl{top:6px;left:6px;border-width:2px 0 0 2px}
.tr{top:6px;right:6px;border-width:2px 2px 0 0}
.bl{bottom:6px;left:6px;border-width:0 0 2px 2px}
.br{bottom:6px;right:6px;border-width:0 2px 2px 0}
.cam-lbl{position:absolute;top:7px;left:10px;font-size:9px;color:#1a4a1a;letter-spacing:2px;z-index:2}
.cam-rec{position:absolute;top:7px;right:10px;font-size:9px;color:#1a4a1a;letter-spacing:2px;z-index:2}
.cam-bl{position:absolute;bottom:7px;left:10px;font-size:9px;color:#1a4a1a;letter-spacing:1px;z-index:2}
.hline{position:absolute;top:50%;left:8%;right:8%;height:1px;background:#0c1e0c;z-index:1}
.vline{position:absolute;left:50%;top:8%;bottom:8%;width:1px;background:#0c1e0c;z-index:1}
.stat-row{display:flex;gap:5px;padding:0 8px;flex-shrink:0}
.sbox{flex:1;background:#060a06;border:1px solid #0f2a0f;border-radius:3px;padding:4px 0;text-align:center}
.sl{font-size:8px;color:#1a4a1a;letter-spacing:2px;margin-bottom:1px}
.sv{font-size:13px;color:#3aff3a;letter-spacing:1px}
.sv.warn{color:#ff9900}
.ctrl-row{display:flex;gap:8px;align-items:center;padding:6px 8px 8px 8px;flex-shrink:0}
.left-space{flex:1;background:#060a06;border:1px solid #0f2a0f;border-radius:3px;align-self:stretch;display:flex;flex-direction:column;align-items:center;justify-content:center;gap:4px;min-height:155px}
.left-lbl{font-size:8px;color:#0d200d;letter-spacing:2px;text-align:center;line-height:1.8}
.joy-wrap{display:flex;flex-direction:column;align-items:center;gap:4px}
.joy-lbl{font-size:8px;color:#2a6a2a;letter-spacing:2px}
.joy-area{position:relative;width:150px;height:150px;background:#060a06;border:2px solid #0f2a0f;border-radius:50%}
.joy-ring{position:absolute;inset:14px;border:1px solid #0a1a0a;border-radius:50%}
.joy-ring2{position:absolute;inset:34px;border:1px solid #080e08;border-radius:50%}
.joy-hline{position:absolute;top:50%;left:8%;right:8%;height:1px;background:#0a1a0a;transform:translateY(-50%)}
.joy-vline{position:absolute;left:50%;top:8%;bottom:8%;width:1px;background:#0a1a0a;transform:translateX(-50%)}
.joy-knob{position:absolute;width:48px;height:48px;background:#0f2a0f;border:2px solid #2a8a2a;border-radius:50%;top:50%;left:50%;transform:translate(-50%,-50%);cursor:grab;transition:border-color 0.1s}
.joy-knob.active{border-color:#3aff3a;background:#102a10}
.joy-dot{position:absolute;width:14px;height:14px;background:#3aff3a;border-radius:50%;top:50%;left:50%;transform:translate(-50%,-50%)}
.joy-val{font-size:8px;color:#1a4a1a;letter-spacing:1px;text-align:center}
.note{text-align:center;font-size:8px;color:#0f2a0f;letter-spacing:1px;padding:0 0 4px 0;flex-shrink:0}
</style>
</head>
<body>

<div class="topbar">
  <div class="tb-l">RC BOAT // CTRL</div>
  <div class="tb-r"><span class="blink">&#9646;</span> REC &nbsp;|&nbsp; IP: <span>192.168.4.1</span> &nbsp;|&nbsp; <span>CONNECTED</span></div>
</div>

<div class="cam-feed" id="camBox">
  <div class="cam-corner tl"></div>
  <div class="cam-corner tr"></div>
  <div class="cam-corner bl"></div>
  <div class="cam-corner br"></div>
  <div class="hline"></div>
  <div class="vline"></div>
  <div class="cam-lbl">CAM-01 // ESP32-CAM</div>
  <div class="cam-rec"><span class="blink">&#9679;</span> REC</div>
  <div class="cam-bl">192.168.4.2:81/stream</div>
  <svg width="44" height="44" viewBox="0 0 44 44" style="z-index:1">
    <line x1="22" y1="0" x2="22" y2="44" stroke="#1a4a1a" stroke-width="0.8"/>
    <line x1="0" y1="22" x2="44" y2="22" stroke="#1a4a1a" stroke-width="0.8"/>
    <circle cx="22" cy="22" r="9" fill="none" stroke="#1a4a1a" stroke-width="0.8"/>
    <circle cx="22" cy="22" r="2.5" fill="#1a4a1a"/>
  </svg>
</div>

<div class="stat-row">
  <div class="sbox"><div class="sl">MTR-L</div><div class="sv" id="vL">000</div></div>
  <div class="sbox"><div class="sl">MTR-R</div><div class="sv" id="vR">000</div></div>
  <div class="sbox"><div class="sl">STATUS</div><div class="sv" id="vStat">STOP</div></div>
  <div class="sbox"><div class="sl">PWR</div><div class="sv">12V</div></div>
</div>

<div class="ctrl-row">
  <div class="left-space">
    <div class="left-lbl">SPACE<br>RESERVED</div>
  </div>
  <div class="joy-wrap">
    <div class="joy-lbl">DUAL MOTOR // JOYSTICK</div>
    <div class="joy-area" id="joyMain">
      <div class="joy-ring"></div>
      <div class="joy-ring2"></div>
      <div class="joy-hline"></div>
      <div class="joy-vline"></div>
      <div class="joy-knob" id="knobMain"><div class="joy-dot"></div></div>
    </div>
    <div class="joy-val" id="jvMain">FWD:+000 &nbsp; TURN:+000</div>
  </div>
</div>

<div class="note">Y = MAJU / MUNDUR &nbsp;|&nbsp; X = BELOK</div>

<script>
var mL=0,mR=0,drag=false;
var area=document.getElementById('joyMain');
var knob=document.getElementById('knobMain');

function getC(){var r=area.getBoundingClientRect();return{x:r.left+r.width/2,y:r.top+r.height/2};}
function getR(){return area.offsetWidth/2-26;}

function move(ex,ey){
  var c=getC(),R=getR();
  var dx=ex-c.x,dy=ey-c.y;
  var d=Math.sqrt(dx*dx+dy*dy);
  if(d>R){dx=dx/d*R;dy=dy/d*R;}
  knob.style.left=(50+dx/area.offsetWidth*100)+'%';
  knob.style.top=(50+dy/area.offsetHeight*100)+'%';
  var fwd=Math.round(-dy/R*255);
  var turn=Math.round(dx/R*255);
  mL=Math.max(-255,Math.min(255,fwd-turn));
  mR=Math.max(-255,Math.min(255,fwd+turn));
  document.getElementById('jvMain').textContent='FWD:'+(fwd>=0?'+':'')+pad(fwd)+' \u00a0 TURN:'+(turn>=0?'+':'')+pad(turn);
  upd();
  fetch('/set?left='+mL+'&right='+mR).catch(function(){});
}

function release(){
  drag=false;
  knob.classList.remove('active');
  knob.style.left='50%';knob.style.top='50%';
  mL=0;mR=0;
  document.getElementById('jvMain').textContent='FWD:+000 \u00a0 TURN:+000';
  upd();
  fetch('/set?left=0&right=0').catch(function(){});
}

function pad(v){return String(Math.abs(v)).padStart(3,'0');}

function upd(){
  document.getElementById('vL').textContent=pad(mL);
  document.getElementById('vR').textContent=pad(mR);
  var s='STOP';
  if(mL>50&&mR>50)s='MAJU';
  else if(mL<-50&&mR<-50)s='MUNDUR';
  else if(mL>80&&mR<-50)s='SPIN-L';
  else if(mR>80&&mL<-50)s='SPIN-R';
  else if(mL>mR+60)s='BELOK-L';
  else if(mR>mL+60)s='BELOK-R';
  else if(mL!=0||mR!=0)s='GERAK';
  var el=document.getElementById('vStat');
  el.textContent=s;
  el.className='sv'+(s==='STOP'?' warn':'');
}

knob.addEventListener('mousedown',function(e){drag=true;knob.classList.add('active');e.preventDefault();});
knob.addEventListener('touchstart',function(e){drag=true;knob.classList.add('active');e.preventDefault();},{passive:false});
document.addEventListener('mousemove',function(e){if(drag)move(e.clientX,e.clientY);});
document.addEventListener('touchmove',function(e){if(drag){e.preventDefault();move(e.touches[0].clientX,e.touches[0].clientY);}},{passive:false});
document.addEventListener('mouseup',function(){if(drag)release();});
document.addEventListener('touchend',function(){if(drag)release();});
</script>
</body>
</html>
)rawliteral";

void handleRoot() {
  server.send(200, "text/html", INDEX_HTML);
}

void handleSet() {
  if (server.hasArg("left"))  setMotorLeft(server.arg("left").toInt());
  if (server.hasArg("right")) setMotorRight(server.arg("right").toInt());
  server.send(200, "text/plain", "OK");
}

void handleNotFound() {
  server.sendHeader("Location", "http://192.168.4.1", true);
  server.send(302, "text/plain", "");
}

void setup() {
  Serial.begin(115200);

  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);

  ledcAttach(ENA, PWM_FREQ, PWM_RES);
  ledcAttach(ENB, PWM_FREQ, PWM_RES);

  setMotorLeft(0);
  setMotorRight(0);

  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(local_ip, gateway, subnet);
  WiFi.softAP(ssid, password);

  Serial.println("Hotspot aktif!");
  Serial.print("SSID: "); Serial.println(ssid);
  Serial.print("IP  : "); Serial.println(WiFi.softAPIP());

  server.on("/",     handleRoot);
  server.on("/set",  handleSet);
  server.onNotFound(handleNotFound);
  server.begin();

  Serial.println("Web server started.");
}

void loop() {
  server.handleClient();
}
