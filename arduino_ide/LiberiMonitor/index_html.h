#pragma once
#include <Arduino.h>

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Liberi</title>
    <link rel="preconnect" href="https://fonts.googleapis.com">
    <link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
    <link href="https://fonts.googleapis.com/css2?family=Outfit:wght@100..900&display=swap" rel="stylesheet">
    <style>
        html {
            font-family: 'Outfit', sans-serif;
            color: #202020;
        }
        body {
            display: block;
            overflow: hidden;
            text-align: center;
            height: 100vh;
            width: 100vw;
            margin: 0;
            padding: 0;
            background: radial-gradient(ellipse at top, #d0d0d0, transparent),
                        radial-gradient(ellipse at bottom, #73ff00, transparent);
            transition: background 0.8s ease;
        }
        body h1 {
            font-size: 5em;
            margin-top: 10vh;
        }
        body h2 {
            font-size: 6em;
            font-weight: 200;
            margin: 20px 0;
        }
        body p {
            font-size: 1.2em;
            margin-top: -20px;
        }
        main {
            display: flex;
            flex-direction: row;
            justify-content: center;
            margin-top: 5vh;
        }
        main div {
            width: 30%;
            flex-shrink: 1;
        }
        .status-container {
            margin-top: 30px;
            font-size: 1.5em;
            font-weight: bold;
            color: #555;
        }
    </style>
</head>
<body>
    <h1>Liberi</h1>
    <main>
        <div>
            <h2 id="temperature">Measuring...</h2>
            <p>Your baby's temperature</p>
        </div>
        <div>
            <h2 id="heartrate">Measuring...</h2>
            <p>Your baby's heart rate</p>
        </div>
        <div>
            <h2 id="oxygen">Measuring...</h2>
            <p>Your baby's oxygen level</p>
        </div>
    </main>
    <div class="status-container" id="status">System: CONNECTING...</div>

    <script>
        const temperature = document.getElementById("temperature");
        const heartrate = document.getElementById("heartrate");
        const oxygen = document.getElementById("oxygen");
        const statusDiv = document.getElementById("status");
        const pageBody = document.body;

        function parseSensorLine(output) {
            const match = output.match(/DATA\s*,\s*(-?\d+(?:\.\d+)?)\s*,\s*(\d+)\s*,\s*(\d+)/i);
            if (!match) return null;
            return {
                temp: parseFloat(match[1]),
                heartrate: parseInt(match[2], 10),
                oxygen: parseInt(match[3], 10)
            };
        }

        function updateUI(output) {
            const sensorData = parseSensorLine(output);
            if (sensorData) {
                if (!isNaN(sensorData.temp) && sensorData.temp > 0) {
                    temperature.textContent = sensorData.temp.toFixed(1) + " ºC";
                } else {
                    temperature.textContent = "N/A";
                }

                if (!isNaN(sensorData.heartrate) && sensorData.heartrate > 0) {
                    heartrate.textContent = sensorData.heartrate + " bpm";
                } else {
                    heartrate.textContent = "N/A";
                }

                if (!isNaN(sensorData.oxygen) && sensorData.oxygen > 0) {
                    oxygen.textContent = sensorData.oxygen + " %";
                } else {
                    oxygen.textContent = "N/A";
                }

                if (sensorData.heartrate > 150 || (sensorData.heartrate < 90 && sensorData.heartrate > 0) || 
                    sensorData.temp > 38 || (sensorData.temp < 35 && sensorData.temp > 0) || 
                    (sensorData.oxygen < 92 && sensorData.oxygen > 0)) {
                    pageBody.style.background = "radial-gradient(ellipse at top, #d0d0d0, transparent), radial-gradient(ellipse at bottom, #fd0000, transparent)";
                } else {
                    pageBody.style.background = "radial-gradient(ellipse at top, #d0d0d0, transparent), radial-gradient(ellipse at bottom, #73ff00, transparent)";
                }
            }
        }

        const source = new EventSource('/events');
        
        source.onopen = function() {
            statusDiv.textContent = "System: CONNECTED";
            statusDiv.style.color = "green";
        };

        source.onerror = function() {
            statusDiv.textContent = "System: DISCONNECTED (Retrying...)";
            statusDiv.style.color = "red";
        };

        source.onmessage = function(event) {
            if (event.data === "STATUS,OFF") {
                statusDiv.textContent = "System: IDLE (OFF)";
                statusDiv.style.color = "gray";
                temperature.textContent = "--";
                heartrate.textContent = "--";
                oxygen.textContent = "--";
            } else if (event.data === "STATUS,ON") {
                statusDiv.textContent = "System: ACTIVE";
                statusDiv.style.color = "green";
            } else {
                updateUI(event.data);
            }
        };

        source.addEventListener('wokwi', function(event) {
            if (event.data === "STATUS,OFF") {
                statusDiv.textContent = "System: IDLE (OFF)";
                statusDiv.style.color = "gray";
                temperature.textContent = "--";
                heartrate.textContent = "--";
                oxygen.textContent = "--";
            } else if (event.data === "STATUS,ON") {
                statusDiv.textContent = "System: ACTIVE";
                statusDiv.style.color = "green";
            } else {
                updateUI(event.data);
            }
        });
    </script>
</body>
</html>
)rawliteral";
