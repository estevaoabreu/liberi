const connectButton = document.getElementById("connect-btn");
const statusPill = document.getElementById("connection-status");
const bpmValueEl = document.getElementById("bpm-value");
const tempValueEl = document.getElementById("temp-value");
const wsUrlInput = document.getElementById("ws-url");

let activeSocket = null;
let textBuffer = "";

function setConnectionStatus(connected) {
  statusPill.textContent = connected ? "Connected" : "Disconnected";
  statusPill.classList.toggle("connected", connected);
  statusPill.classList.toggle("disconnected", !connected);
}

function resetValues() {
  bpmValueEl.textContent = "--";
  tempValueEl.textContent = "--";
}

function updateMetrics(data) {
  if (typeof data.bpm === "number") {
    bpmValueEl.textContent = Math.round(data.bpm).toString();
  }

  if (typeof data.temp === "number") {
    tempValueEl.textContent = data.temp.toFixed(1);
  }
}

function processIncomingText(chunk) {
  textBuffer += chunk;
  const lines = textBuffer.split(/\r?\n/);
  textBuffer = lines.pop() ?? "";

  for (const line of lines) {
    const trimmed = line.trim();
    if (!trimmed) {
      continue;
    }

    try {
      const payload = JSON.parse(trimmed);
      updateMetrics(payload);
    } catch (error) {
      console.error("Failed to parse JSON line:", trimmed, error);
    }
  }
}

function closeActiveSocket() {
  if (activeSocket && activeSocket.readyState === WebSocket.OPEN) {
    activeSocket.close(1000, "User disconnected");
  }

  activeSocket = null;
  setConnectionStatus(false);
  connectButton.disabled = false;
  connectButton.textContent = "Connect to Wokwi";
  wsUrlInput.disabled = false;
}

function onSocketMessage(event) {
  if (typeof event.data === "string") {
    processIncomingText(event.data);
    return;
  }

  if (event.data instanceof Blob) {
    event.data.text().then(processIncomingText).catch((error) => {
      console.error("Failed to decode blob message:", error);
    });
  }
}

function connectToWokwi() {
  const wsUrl = wsUrlInput.value.trim();

  if (!wsUrl) {
    console.error("WebSocket URL is empty.");
    alert("Please provide the local Wokwi WebSocket URL.");
    return;
  }

  connectButton.disabled = true;
  wsUrlInput.disabled = true;
  connectButton.textContent = "Connecting...";
  textBuffer = "";

  try {
    const socket = new WebSocket(wsUrl);
    activeSocket = socket;

    socket.addEventListener("open", () => {
      setConnectionStatus(true);
      connectButton.disabled = false;
      connectButton.textContent = "Disconnect";
    });

    socket.addEventListener("message", onSocketMessage);

    socket.addEventListener("error", (error) => {
      console.error("WebSocket connection error:", error);
    });

    socket.addEventListener("close", (event) => {
      if (event.code !== 1000) {
        console.error("Wokwi socket disconnected unexpectedly:", event);
      }

      if (activeSocket === socket) {
        closeActiveSocket();
      }
    });
  } catch (error) {
    console.error("Failed to create WebSocket:", error);
    closeActiveSocket();
  }
}

connectButton.addEventListener("click", () => {
  if (activeSocket) {
    closeActiveSocket();
  } else {
    connectToWokwi();
  }
});

resetValues();
setConnectionStatus(false);
