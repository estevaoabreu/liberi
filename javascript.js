// estado
const appData = {
  babyName: "",
  babyAge: 0,
  notificationsEnabled: false,
};

const connectBtn = document.getElementById("connectBtn");
const temperature = document.getElementById("temperature");
const heartrate = document.getElementById("heartrate");
const oxygen = document.getElementById("oxygen");
const pageBody = document.body;

// onboarding

document.addEventListener("DOMContentLoaded", () => {
  const ageSelect = document.getElementById("babyAge");
  if (ageSelect) {
    for (let i = 1; i <= 24; i++) {
      let option = document.createElement("option");
      option.value = i;
      option.text = `${i} months`;
      ageSelect.appendChild(option);
    }
  }
});

function nextStep(stepNumber) {
  // nome e idade input
  if (stepNumber === 4) {
    const nameInput = document.getElementById("babyName").value.trim();
    
    // VALIDATION POP-UP: Check if the name field is empty
    if (nameInput === "") {
      alert("Please enter your baby's name before moving on! ❤️");
      return; // Stops the function right here, preventing the screen change!
    }
    
    appData.babyName = nameInput;
    const ageHeading = document.querySelector("#step-4 h2");
    if (ageHeading) {
      ageHeading.textContent = `How old is ${appData.babyName}?`;
    }
  }

  if (stepNumber === 5) {
    const ageSelect = document.getElementById("babyAge");
    if (ageSelect) {
      appData.babyAge = ageSelect.value;
    }
  }

  // This part only runs if the validation check passes
  const screens = document.querySelectorAll(".screen");
  screens.forEach((screen) => {
    screen.classList.remove("active");
  });

  const targetScreen = document.getElementById(`step-${stepNumber}`);
  if (targetScreen) {
    targetScreen.classList.add("active");
  }
}

function setNotifications(choice) {
  appData.notificationsEnabled = choice;
  console.log("Onboarding complete. Collected Data:", appData);
  nextStep(6); // Forward user directly to dashboard
}

// --- ESP32 Sensor Processing Logic ---

function parseSensorLine(output) {
  // Accept DATA lines even if there is leading/trailing noise around the payload.
  const match = output.match(
    /DATA\s*,\s*(-?\d+(?:\.\d+)?)\s*,\s*(\d+)\s*,\s*(\d+)/i,
  );
  if (!match) return null;

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

    pageBody.style.background = "";

    if (!isNaN(sensorData.temp)) {
      temperature.textContent = `${sensorData.temp.toFixed(1)}ºC`;
    } else {
      temperature.textContent = "N/A";
    }

    if (!isNaN(sensorData.heartrate)) {
      heartrate.textContent = `${sensorData.heartrate}`;
    } else {
      heartrate.textContent = "N/A";
    }

    if (!isNaN(sensorData.oxygen)) {
      oxygen.textContent = `${sensorData.oxygen}%`;
    } else {
      oxygen.textContent = "N/A";
    }

    // 2. Compute Health Boundary Metrics
    const isTempOff = sensorData.temp > 38 || sensorData.temp < 35;
    const isHeartOff = sensorData.heartrate > 150 || sensorData.heartrate < 90;
    const isOxygenOff = sensorData.oxygen < 92;

    let offCount = 0;
    let offMetrics = [];

    if (isTempOff) { offCount++; offMetrics.push("temperature"); }
    if (isHeartOff) { offCount++; offMetrics.push("heart rate"); }
    if (isOxygenOff) { offCount++; offMetrics.push("oxygen level"); }

    // Gather Status Screen Nodes
    const statusTitle = document.getElementById("statusTitle");
    const statusDescription = document.getElementById("statusDescription");
    const statusBlob = document.getElementById("statusBlob");
    const dashboardWelcome = document.getElementById("dashboardWelcome");
    const currentName = appData.babyName || "Tommy";

    // Set greeting name context
    if (dashboardWelcome) {
      dashboardWelcome.textContent = `${currentName}'s levels are...`;
    }

    // Flush old custom background engine flags
    pageBody.classList.remove("state-normal", "state-abnormal", "state-concerning");

    // 3. Three-Tier State Machine Router
    if (offCount === 3) {
      // CONCERNING STATE: All values are completely off
      pageBody.classList.add("state-concerning");
      if (statusTitle) statusTitle.textContent = "Concerning";
      if (statusBlob) statusBlob.src = "assets/concerning.svg";
      if (statusDescription) {
        statusDescription.innerHTML = `${currentName}'s levels are <strong>unhealthy</strong>.<br>We advise you to <strong>call emergency services</strong>.`;
      }
    } 
    else if (offCount > 0) {
      // ABNORMAL STATE: One or two values are outside normal profiles
      pageBody.classList.add("state-abnormal");
      if (statusTitle) statusTitle.textContent = "Abnormal";
      if (statusBlob) statusBlob.src = "assets/abnormal.svg";
      if (statusDescription) {
        let problemList = offMetrics.join(" and ");
        statusDescription.textContent = `${currentName}'s ${problemList} is currently out of normal standards.`;
      }
    } 
    else {
      // NORMAL STATE: Tracking clean data lines
      pageBody.classList.add("state-normal");
      if (statusTitle) statusTitle.textContent = "Normal";
      if (statusBlob) statusBlob.src = "assets/normal.svg";
      if (statusDescription) {
        statusDescription.textContent = `${currentName}'s levels are healthy`;
      }
    }
  } else {
    console.log("System Status:", output);
  }
}

// --- Web Serial API Streams Architecture ---

let port;
let reader;
let inputDone;
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

if (connectBtn) {
  connectBtn.addEventListener("click", connectSerial);
}

const source = new EventSource("/events");

source.onopen = function () {

};

source.onerror = function () {
};

source.onmessage = function (event) {
  let data = event.data;

  // If the data is JSON-formatted (e.g. from the Wokwi/serial bridge), parse it.
  if (data.startsWith("{")) {
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
    temperature.textContent = "Off";
    heartrate.textContent = "Off";
    oxygen.textContent = "Off";
    
    pageBody.classList.remove("state-normal", "state-abnormal", "state-concerning");
    
    const statusBlob = document.getElementById("statusBlob");
    const statusTitle = document.getElementById("statusTitle");
    const statusDescription = document.getElementById("statusDescription");

    if (statusBlob) statusBlob.src = "assets/happy.svg"; 
    if (statusTitle) statusTitle.textContent = "Connecting...";
    if (statusDescription) {
      statusDescription.innerHTML = "Please click the button below<br>to pair your monitor device."; 
    }
    
    pageBody.style.background = `
      radial-gradient(ellipse at top, #d0d0d0, transparent),
      radial-gradient(ellipse at bottom, #73ff00, transparent)
    `;
  } else if (data === "STATUS,ON") {

  } else {
    updateUI(data);
  }
};