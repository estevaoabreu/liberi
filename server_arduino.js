const { SerialPort } = require("serialport");
const { ReadlineParser } = require("@serialport/parser-readline");
const http = require("http");
const fs = require("fs");
const path = require("path");

const PORT = 3000;
const SERIAL_PORT = "COM7"; // Change this if your Arduino is on a different port
const BAUD_RATE = 115200;

// Set up HTTP Server to serve static files and SSE
const server = http.createServer((req, res) => {
  if (req.url === "/events") {
    // Server-Sent Events endpoint
    res.writeHead(200, {
      "Content-Type": "text/event-stream",
      "Cache-Control": "no-cache",
      Connection: "keep-alive",
      "Access-Control-Allow-Origin": "*",
    });

    // Send an initial message
    res.write(`data: {"type": "info", "message": "Connected to SSE"}\n\n`);

    // Open serial connection to the physical Arduino
    const serial = new SerialPort({
      path: SERIAL_PORT,
      baudRate: BAUD_RATE,
      autoOpen: false,
    });

    // ReadlineParser splits incoming data into complete lines automatically
    const parser = serial.pipe(new ReadlineParser({ delimiter: "\n" }));

    serial.open((err) => {
      if (err) {
        console.error("Failed to open serial port:", err.message);
        res.write(
          `data: ${JSON.stringify({ type: "info", message: "Serial port error: " + err.message })}\n\n`,
        );
        return;
      }
      console.log(
        `Connected to Arduino on ${SERIAL_PORT} at ${BAUD_RATE} baud`,
      );
    });

    parser.on("data", (line) => {
      const trimmed = line.trim();
      if (trimmed) {
        res.write(
          `data: ${JSON.stringify({ type: "wokwi", content: trimmed })}\n\n`,
        );
      }
    });

    serial.on("error", (err) => {
      console.error("Serial port error:", err.message);
    });

    // Clean up serial port when the browser disconnects
    req.on("close", () => {
      if (serial.isOpen) {
        serial.close(() => {
          console.log("Serial port closed.");
        });
      }
    });
  } else {
    // Serve static files
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
  }
});

server.listen(PORT, () => {
  console.log(`\n==============================================`);
  console.log(`Server running at http://localhost:${PORT}/`);
  console.log(`Listening on serial port ${SERIAL_PORT} at ${BAUD_RATE} baud`);
  console.log(`Make sure your Arduino is connected and running!`);
  console.log(`==============================================\n`);
});
