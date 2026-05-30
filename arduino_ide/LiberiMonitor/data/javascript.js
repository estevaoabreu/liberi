const connectBtn = document.getElementById("connectBtn");
const temperature = document.getElementById("temperature");
const heartrate = document.getElementById("heartrate");
const oxygen = document.getElementById("oxygen");
const pageBody = document.body;

function parseSensorLine(output) {
  // Accept DATA lines even if there is leading/trailing noise around the payload.
  const match = output.match(
    /DATA\s*,\s*(-?\d+(?:\.\d+)?)\s*,\s*(\d+)\s*,\s*(\d+)/i,
  );
  if (!match) {
    return null;
  }

  return {
    temp: parseFloat(match[1]),
    heartrate: parseInt(match[2], 10),
    oxygen: parseInt(match[3], 10),
  };
}

function updateUI(output) {
  const sensorData = parseSensorLine(output);
  if (sensorData) {
    console.log("Parsed Sensor Data:", sensorData);

    if (!isNaN(sensorData.temp)) {
      temperature.textContent = `${sensorData.temp.toFixed(1)} ºC`;
    } else {
      temperature.textContent = "N/A";
    }

    if (!isNaN(sensorData.heartrate)) {
      heartrate.textContent = `${sensorData.heartrate} bpm`;
    } else {
      heartrate.textContent = "N/A";
    }

    if (!isNaN(sensorData.oxygen)) {
      oxygen.textContent = `${sensorData.oxygen} %`;
    } else {
      oxygen.textContent = "N/A";
    }

    if (
      sensorData.heartrate > 150 ||
      sensorData.heartrate < 90 ||
      sensorData.temp > 38 ||
      sensorData.temp < 35 ||
      sensorData.oxygen < 92
    ) {
      pageBody.style.background = `
    radial-gradient(ellipse at top, #d0d0d0, transparent),
    radial-gradient(ellipse at bottom, #fd0000, transparent)
      `;
    } else {
      pageBody.style.background = `
    radial-gradient(ellipse at top, #d0d0d0, transparent),
    radial-gradient(ellipse at bottom, #73ff00, transparent)
      `;
    }
  } else {
    console.log("System Status:", output);
  }
}

// Web Serial API implementation
let port;
let reader;
let inputDone;
let outputStream;
let inputStream;

class LineBreakTransformer {
  constructor() {
    this.chunks = "";
  }
  transform(chunk, controller) {
    this.chunks += chunk;
    const lines = this.chunks.split("\n");
    this.chunks = lines.pop();
    lines.forEach((line) => controller.enqueue(line));
  }
  flush(controller) {
    if (this.chunks) {
      controller.enqueue(this.chunks);
    }
  }
}

async function connectSerial() {
  if ("serial" in navigator) {
    try {
      port = await navigator.serial.requestPort();
      await port.open({ baudRate: 115200 });

      connectBtn.textContent = "Connected";
      connectBtn.style.backgroundColor = "#28a745";

      const decoder = new TextDecoderStream();
      inputDone = port.readable.pipeTo(decoder.writable);
      inputStream = decoder.readable.pipeThrough(
        new TransformStream(new LineBreakTransformer()),
      );
      reader = inputStream.getReader();

      while (true) {
        const { value, done } = await reader.read();
        if (done) {
          reader.releaseLock();
          break;
        }
        if (value) {
          updateUI(value.trim());
        }
      }
    } catch (err) {
      console.error("There was an error opening the serial port:", err);
      connectBtn.textContent = "Connection Failed";
      connectBtn.style.backgroundColor = "#dc3545";
    }
  } else {
    console.error("Web Serial API not supported in this browser.");
    alert(
      "Web Serial API is not supported in this browser. Please use Chrome or Edge.",
    );
  }
}

connectBtn.addEventListener("click", connectSerial);

// Automatically connect to Server-Sent Events from the local Node.js server
const statusDiv = document.createElement("div");
statusDiv.id = "status";
statusDiv.className = "status-container";
statusDiv.textContent = "System: CONNECTING WIRELESSLY...";
statusDiv.style.marginTop = "15px";
statusDiv.style.fontWeight = "bold";
statusDiv.style.color = "#555";
connectBtn.parentNode.appendChild(statusDiv);

const source = new EventSource('/events');

source.onopen = function() {
  statusDiv.textContent = "System: CONNECTED WIRELESSLY";
  statusDiv.style.color = "green";
};

source.onerror = function() {
  statusDiv.textContent = "System: DISCONNECTED (Retrying...)";
  statusDiv.style.color = "red";
};

source.onmessage = function(event) {
  let data = event.data;
  
  // If the data is JSON-formatted (e.g. from the Wokwi/serial bridge), parse it.
  if (data.startsWith('{')) {
    try {
      const parsed = JSON.parse(data);
      if (parsed.content) {
        data = parsed.content;
      } else if (parsed.message) {
        console.log("Server Message:", parsed.message);
        return;
      }
    } catch (e) {
      console.error("Error parsing JSON EventSource data:", e);
    }
  }

  if (data === "STATUS,OFF") {
    statusDiv.textContent = "System: IDLE (OFF)";
    statusDiv.style.color = "gray";
    temperature.textContent = "--";
    heartrate.textContent = "--";
    oxygen.textContent = "--";
    pageBody.style.background = `
      radial-gradient(ellipse at top, #d0d0d0, transparent),
      radial-gradient(ellipse at bottom, #73ff00, transparent)
    `;
  } else if (data === "STATUS,ON") {
    statusDiv.textContent = "System: ACTIVE";
    statusDiv.style.color = "green";
  } else {
    updateUI(data);
  }
};
