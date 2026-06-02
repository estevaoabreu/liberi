// estado
const appData = {
  babyName: "",
  babyAge: 0,
  notificationsEnabled: false,
};

let lastNotificationTime = 0;
let currentAlertState = "normal";

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
    
    // tem de inserir nome
    if (nameInput === "") {
      alert("Please enter your baby's name before moving on! ❤️");
      return; 
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

  // update nome antes de recolher dados
  if (stepNumber === 6) {
    const dashboardWelcome = document.getElementById("dashboardWelcome");
    if (dashboardWelcome) {
      dashboardWelcome.textContent = `${appData.babyName}'s levels are...`;
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

  if (choice && "Notification" in window) {
    Notification.requestPermission().then(permission => {
      console.log("Notification permission:", permission);
    });
  }

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
    const connectionControls = document.querySelector(".connection-controls");
    const dashboardFooter = document.querySelector(".dashboard-footer");
    if (connectionControls) connectionControls.style.display = "none";
    if (dashboardFooter) dashboardFooter.style.display = "flex";
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
    // temp fixa
    const isTempOff = sensorData.temp > 38 || sensorData.temp < 35;

    //bpm dinamico dependente da idade
    const ageMonths = parseInt(appData.babyAge, 10) || 1;
    let minHeart = 80;
    let maxHeart = 190;

    if (ageMonths === 1) {
      minHeart = 90;
      maxHeart = 205;
    } else if (ageMonths >= 13) {
      minHeart = 65;
      maxHeart = 140;
    }
    const isHeartOff = sensorData.heartrate > maxHeart || sensorData.heartrate < minHeart;
    
    //sat fixa
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

    pageBody.classList.remove("state-normal", "state-abnormal", "state-concerning");

    if (offCount === 3) {
      // estado concerning: 3 valores fora da normalidade
      pageBody.classList.add("state-concerning");
      if (statusTitle) statusTitle.textContent = "Concerning";
      if (statusBlob) statusBlob.src = "assets/concerning.svg";
      if (statusDescription) {
        statusDescription.innerHTML = `${currentName}'s levels are <strong>unhealthy</strong>.<br>We advise you to <strong>call emergency services</strong>.`;
      }
    } 
    else if (offCount > 0) {
      // estado abnormal: 1 ou 2 valores fora
      pageBody.classList.add("state-abnormal");
      if (statusTitle) statusTitle.textContent = "Abnormal";
      if (statusBlob) statusBlob.src = "assets/abnormal.svg";
      if (statusDescription) {
        let problemList = offMetrics.join(" and ");
        statusDescription.textContent = `${currentName}'s ${problemList} is currently out of normal standards.`;
      }
    } 
    else {
      // estado normal: todos os valores dentro 
      pageBody.classList.add("state-normal");
      if (statusTitle) statusTitle.textContent = "Normal";
      if (statusBlob) statusBlob.src = "assets/normal.svg";
      if (statusDescription) {
        statusDescription.textContent = `${currentName}'s levels are healthy`;
      }
    }

    // --- Web Notifications Logic ---
    let newState = "normal";
    let alertMessage = "";
    if (offCount === 3) {
      newState = "concerning";
      alertMessage = `${currentName}'s levels are unhealthy. Please check immediately!`;
    } else if (offCount > 0) {
      newState = "abnormal";
      let problemList = offMetrics.join(" and ");
      alertMessage = `${currentName}'s ${problemList} is currently out of normal standards.`;
    }

    if (newState !== "normal" && appData.notificationsEnabled && "Notification" in window && Notification.permission === "granted") {
      const now = Date.now();
      // Notify if state just changed to a bad state, OR if it's been in a bad state for more than 2 minutes (120000ms)
      if (newState !== currentAlertState || (now - lastNotificationTime > 120000)) {
        new Notification("Liberi Monitor Alert", {
          body: alertMessage
        });
        lastNotificationTime = now;
      }
    }
    
    currentAlertState = newState;

  } else {
    console.log("System Status:", output);
  }
}


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
      connectBtn.textContent = "Connecting...";
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

    if (statusBlob) statusBlob.src = "assets/neutral.svg"; 
    if (statusTitle) statusTitle.textContent = "Connecting...";
    if (statusDescription) {
      statusDescription.innerHTML = "Please click the button below<br>to pair your monitor device."; 
    }
    
    pageBody.style.background = `
      radial-gradient(ellipse at top, #d0d0d0, transparent),
      radial-gradient(ellipse at bottom, #73ff00, transparent)
    `;

    // WIRELESS RESET VIEWS: Show the button and hide footer icons
    const connectionControls = document.querySelector(".connection-controls");
    const dashboardFooter = document.querySelector(".dashboard-footer");
    if (connectionControls) connectionControls.style.display = "block";
    if (dashboardFooter) dashboardFooter.style.display = "none";
    if (connectBtn) {
      connectBtn.textContent = "Connect to bracelet";
      connectBtn.style.backgroundColor = "#1B1B1B";
    }
  } else if (data === "STATUS,ON") {

  } else {
    updateUI(data);
  }
};

// --- Footer Icon Navigation Infrastructure ---

function openInfoScreen() {
  // Hide all screens and active info layout window
  document.querySelectorAll(".screen").forEach(s => s.classList.remove("active"));
  const target = document.getElementById("info-screen");
  if (target) target.classList.add("active");
}

function openAgeBoundsScreen() {
  document.querySelectorAll(".screen").forEach(s => s.classList.remove("active"));
  
  const heading = document.getElementById("ageBoundsHeading");
  const heartDisplay = document.getElementById("targetHeart");
  const currentName = appData.babyName || "Tommy";
  const ageMonths = parseInt(appData.babyAge, 10) || 1;

  if (heading) {
    heading.textContent = `According to ${currentName}’s age their levels should be:`;
  }

  // ajustado a idade inserida pelos pais
  if (heartDisplay) {
    if (ageMonths === 1) {
      heartDisplay.textContent = "90 - 205 bpm";
    } else if (ageMonths >= 2 && ageMonths <= 12) {
      heartDisplay.textContent = "80 - 190 bpm";
    } else {
      heartDisplay.textContent = "65 - 140 bpm";
    }
  }

  const targetScreen = document.getElementById("age-bounds-screen");
  if (targetScreen) targetScreen.classList.add("active");
}

function backToDashboard() {
  document.querySelectorAll(".screen").forEach(s => s.classList.remove("active"));
  const dashboard = document.getElementById("step-6");
  if (dashboard) dashboard.classList.add("active");
}

function refreshSystem() {
  console.log("Requesting instant data update from ESP32...");
  
  const refreshButton = document.querySelector("button[onclick='refreshSystem()'] img");
  if (refreshButton) {
    refreshButton.style.transition = "transform 0.6s cubic-bezier(0.4, 0, 0.2, 1)";
    refreshButton.style.transform = "rotate(360deg)";
    
    setTimeout(() => {
      refreshButton.style.transition = "none";
      refreshButton.style.transform = "rotate(0deg)";
    }, 600);
  }

  fetch('/refresh') 
    .then(response => {
      console.log("Instant poll request successfully processed by system node.");
    })
    .catch(error => {
      console.warn("Could not dispatch refresh signal over network node:", error);
    });
}