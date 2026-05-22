const http = require("http");
const fs = require("fs");
const path = require("path");

const PORT = 3000;

// Set up HTTP Server to serve static files
const server = http.createServer((req, res) => {
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
});

server.listen(PORT, () => {
  console.log(`\n==============================================`);
  console.log(`Server running at http://localhost:${PORT}/`);
  console.log(
    `Open http://localhost:${PORT}/ in your browser to connect to ESP32.`,
  );
  console.log(`==============================================\n`);
});
