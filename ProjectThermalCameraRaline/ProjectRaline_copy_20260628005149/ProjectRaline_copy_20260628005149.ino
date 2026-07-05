#include <WiFi.h>
#include <WebServer.h>
#include <WiFiManager.h>
#include <Wire.h>
#include <ArduinoJson.h>
#include <Adafruit_MLX90640.h>

WebServer server(80);

Adafruit_MLX90640 mlx;

float frame[32 * 24];
float threshold = 36.5;

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

<h2 id="status">
Hot Pixels : 0
</h2>

<h2>

Threshold

<br><br>

<button onclick="increaseThreshold()">&uarr;</button>

<span id="threshold">36.5

</span>

°C

<button onclick="decreaseThreshold()">&darr;</button>


</h2>

<button onclick="saveRecord()">
Rekam Jumlah Pixel
</button>

<button onclick="resetHistory()">
Reset Riwayat
</button>

<h2>Riwayat</h2>

<div id="history"></div>
<script>

const canvas = document.getElementById("canvas");
const ctx = canvas.getContext("2d");

const cols = 32;
const rows = 24;

const cw = canvas.width / cols;
const ch = canvas.height / rows;

function getColor(v,min,max){

  let ratio = (v-min)/(max-min);

  ratio = Math.max(0,Math.min(1,ratio));

  let r = Math.floor(255*ratio);
  let b = Math.floor(255*(1-ratio));
  let g = Math.floor(150*(1-Math.abs(ratio-0.5)*2));

  return `rgb(${r},${g},${b})`;
}

let threshold = 36.5;

let currentHotPixels = 0;

let recordNumber = 1;

let historyData = [];

function increaseThreshold(){

    threshold += 0.1;

    threshold = Number(threshold.toFixed(1));

    document.getElementById("threshold").innerHTML =
    threshold.toFixed(1);

    fetch("/setThreshold?value="+threshold);

}

function decreaseThreshold(){

    threshold -= 0.1;

    threshold = Number(threshold.toFixed(1));

    document.getElementById("threshold").innerHTML =
    threshold.toFixed(1);

    fetch("/setThreshold?value="+threshold);

}

function resetHistory(){

    historyData = [];

    document.getElementById("history").innerHTML = "";

}

async function updateThermal()
{
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
      ctx.fillStyle="white";
      ctx.font="20px Arial";
      ctx.fillText("MIN : "+min.toFixed(1)+" C",10,25);
      ctx.fillText("MAX : "+max.toFixed(1)+" C",10,50);

      document.getElementById("status").innerHTML =
      "Hot Pixels (>" +
      threshold.toFixed(1) +
      "°C) : " +
      currentHotPixels;
}

updateThermal();
setInterval(updateThermal,200);

function saveRecord(){

    historyData.push(currentHotPixels);

    let text="";

    for(let i=0;i<historyData.length;i++){

        text +=
        "No. "+(i+1)+
        " = "+
        historyData[i]+
        " pixel<br>";

    }

    document.getElementById("history").innerHTML = text;

}



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

    if(frame[i] >= threshold){

      hotPixels++;

    }

  }

  DynamicJsonDocument doc(25000);

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

WiFi.mode(WIFI_AP);

IPAddress local_ip(192,168,4,1);
IPAddress gateway(192,168,4,1);
IPAddress subnet(255,255,255,0);

WiFi.softAPConfig(local_ip, gateway, subnet);

WiFi.softAP("ESP32-THERMAL","12345678");

  Serial.println("Hotspot Started");
  Serial.print("IP : ");
  Serial.println(WiFi.softAPIP());
  Serial.println();
  Serial.println("WIFI CONNECTED");

  Serial.print("THERMAL CAMERA IP : ");

  Serial.println(WiFi.localIP());

  Serial.println();
  Serial.println("OPEN BROWSER:");
  Serial.print("http://");
  Serial.println(WiFi.localIP());

server.on("/", handleRoot);

server.on("/data", handleData);

server.on("/setThreshold", [](){

    if(server.hasArg("value")){

        threshold = server.arg("value").toFloat();

    }

    server.send(200,"text/plain","OK");

});

  server.begin();

  Serial.println("SERVER STARTED");
}

void loop(){

  server.handleClient();
}