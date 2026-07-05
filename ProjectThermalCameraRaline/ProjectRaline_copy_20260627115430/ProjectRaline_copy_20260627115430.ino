#include <WiFi.h>
#include <WebServer.h>
#include <WiFiManager.h>
#include <Wire.h>
#include <ArduinoJson.h>
#include <Adafruit_MLX90640.h>

WebServer server(80);

Adafruit_MLX90640 mlx;

float frame[32 * 24];

const char webpage[] PROGMEM = R"=====(

<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>ESP32 Thermal Camera</title>

<style>

body{
  background:#111;
  color:white;
  text-align:center;
  font-family:Arial;
}

canvas{
  border:2px solid white;
  margin-top:20px;
}

</style>
</head>

<body>

<h1>MLX90640 Thermal Camera</h1>

<canvas id="canvas" width="640" height="480"></canvas>

<br><br>

<h2 id="status">
Hot Pixels (>36°C) : 0
</h2>

<button onclick="saveRecord()">
Rekam Jumlah Pixel
</button>

<button onclick="clearRecord()">
Hapus Riwayat
</button>

<h2>Riwayat Pengukuran</h2>

<div id="history"></div>

<script>

const canvas = document.getElementById("canvas");
const ctx = canvas.getContext("2d");

const cols = 32;
const rows = 24;

const cw = canvas.width / cols;
const ch = canvas.height / rows;  
  let currentHotPixels = 0;
  let recordNumber = 1;
  let historyData = [];

function getColor(v,min,max){

  let ratio = (v-min)/(max-min);

  ratio = Math.max(0,Math.min(1,ratio));

  let r = Math.floor(255*ratio);
  let b = Math.floor(255*(1-ratio));
  let g = Math.floor(150*(1-Math.abs(ratio-0.5)*2));


  return 'rgb(${r},${g},${b})';
}

async function updateThermal(){

  const response = await fetch('/data');
  const json = await response.json();
  const data = json.frame;

currentHotPixels = json.hotPixels;

  let min = Math.min(...data);
  let max = Math.max(...data);

  for(let y=0;y<rows;y++){

    for(let x=0;x<cols;x++){

      let temp = data[y*cols+x];

      ctx.fillStyle = getColor(temp,min,max);

      ctx.fillRect(
        x*cw,
        y*ch,
        cw,
        ch
      );
    }
  }

  document.getElementById("status").innerHTML =
  "Hot Pixels (>36°C) : " + currentHotPixels;

  ctx.fillStyle = "white";

  ctx.font = "20px Arial";

  ctx.fillText("MIN : "+min.toFixed(1)+" C",10,25);
  ctx.fillText("MAX : "+max.toFixed(1)+" C",10,50);
}

function saveRecord(){

    historyData.push(currentHotPixels);

    let text = "";

    for(let i=0;i<historyData.length;i++){

        text +=
        "No."+(i+1)+
        " = "+
        historyData[i]+
        " pixel<br>";

    }

    document.getElementById("history").innerHTML = text;

}

function clearRecord(){

    historyData = [];

    document.getElementById("history").innerHTML = "";

}

setInterval(updateThermal,200);

</script>

</body>
</html>

)=====";

void handleRoot(){

  server.send(200,"text/html",webpage);
}

void handleData(){

  if(mlx.getFrame(frame) != 0){

    server.send(500,"text/plain","Sensor Error");

    return;
  }

  int hotPixels = 0;

  for(int i=0;i<768;i++){

    if(frame[i] >= 36.0){

      hotPixels++;

    }

  }

  DynamicJsonDocument doc(22000);

  doc["hotPixels"] = hotPixels;

  JsonArray arr = doc.createNestedArray("frame");

  for(int i=0;i<768;i++){

    arr.add(frame[i]);

  }

  String json;

  serializeJson(doc,json);

  server.send(200,"application/json",json);

}

void setup(){

  Serial.begin(115200);

  delay(2000);

  Serial.println();
  Serial.println("ESP32 THERMAL CAMERA START");

  Wire.begin(8,9);

  Wire.setClock(100000);

  if(!mlx.begin()){

    Serial.println("MLX90640 ERROR");

    while(1);
  }

  Serial.println("MLX90640 OK");

  WiFiManager wm;

  bool res;

  res = wm.autoConnect("ESP32-THERMAL");

  if(!res){

    Serial.println("WiFi Failed");

    ESP.restart();
  }

  Serial.println();
  Serial.println("WIFI CONNECTED");

  Serial.print("THERMAL CAMERA IP : ");

  Serial.println(WiFi.localIP());

  Serial.println();
  Serial.println("OPEN BROWSER:");
  Serial.print("http://");
  Serial.println(WiFi.localIP());

  server.on("/",handleRoot);

  server.on("/data",handleData);

  server.begin();

  Serial.println("SERVER STARTED");
}

void loop(){

  server.handleClient();
}