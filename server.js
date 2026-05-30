const http = require("http");
const fs = require("fs");
const path = require("path");

const PORT = 3000;

// Maintain list of SSE clients (browsers)
let sseClients = [];

// Set up HTTP Server to serve static files, API endpoint, and SSE
const server = http.createServer((req, res) => {
  // 1. Endpoint for ESP32 to POST vital signs data
  if (req.url === "/api/data" && req.method === "POST") {
    let body = "";
    req.on("data", chunk => {
      body += chunk.toString();
    });
    req.on("end", () => {
      console.log(`[Server] Received from ESP32: ${body}`);
      
      // Broadcast this sensor reading to all connected browser dashboards
      sseClients.forEach(client => {
        client.write(`data: ${body}\n\n`);
      });
      
      res.writeHead(200, { "Content-Type": "text/plain" });
      res.end("OK");
    });
    return;
  }

  // 2. Server-Sent Events endpoint for browsers
  if (req.url === "/events") {
    res.writeHead(200, {
      "Content-Type": "text/event-stream",
      "Cache-Control": "no-cache",
      "Connection": "keep-alive",
      "Access-Control-Allow-Origin": "*",
    });

    // Send an initial system message
    res.write("data: STATUS,ON\n\n");
    sseClients.push(res);

    console.log(`[Server] Browser client connected. Total clients: ${sseClients.length}`);

    // Remove client when it disconnects
    req.on("close", () => {
      sseClients = sseClients.filter(client => client !== res);
      console.log(`[Server] Browser client disconnected. Total clients: ${sseClients.length}`);
    });
    return;
  }

  // 3. Serve static files
  let filePath = "." + req.url;
  if (filePath === "./") {
    filePath = "./index.html";
  }

  const extname = path.extname(filePath);
  let contentType = "text/html";
  switch (extname) {
    case ".js":
      contentType = "text/javascript";
      break;
    case ".css":
      contentType = "text/css";
      break;
    case ".json":
      contentType = "application/json";
      break;
    case ".png":
      contentType = "image/png";
      break;
    case ".jpg":
      contentType = "image/jpg";
      break;
  }

  fs.readFile(filePath, (error, content) => {
    if (error) {
      if (error.code == "ENOENT") {
        res.writeHead(404);
        res.end("File not found");
      } else {
        res.writeHead(500);
        res.end("Server Error: " + error.code);
      }
    } else {
      res.writeHead(200, { "Content-Type": contentType });
      res.end(content, "utf-8");
    }
  });
});

server.listen(PORT, () => {
  console.log(`\n==============================================`);
  console.log(`Server running at http://localhost:${PORT}/`);
  console.log(`Accepting wireless ESP32 data POSTs at /api/data`);
  console.log(`Open http://localhost:${PORT}/ in your browser to view the dashboard.`);
  console.log(`==============================================\n`);
});
