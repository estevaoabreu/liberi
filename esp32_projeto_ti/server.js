const net = require('net');
const http = require('http');
const fs = require('fs');
const path = require('path');

const PORT = 3000;
const WOKWI_PORT = 4000;

const server = http.createServer((req, res) => {
    if (req.url === '/events') {
        res.writeHead(200, {
            'Content-Type': 'text/event-stream',
            'Cache-Control': 'no-cache',
            'Connection': 'keep-alive',
            'Access-Control-Allow-Origin': '*'
        });

        let serialBuffer = '';
        const client = new net.Socket();
        client.connect(WOKWI_PORT, '127.0.0.1');

        client.on('data', (data) => {
            serialBuffer += data.toString();
            const lines = serialBuffer.split(/\r?\n/);
            serialBuffer = lines.pop() || '';
            lines.forEach(line => {
                if (line.trim()) res.write(`data: ${JSON.stringify({ type: 'wokwi', content: line.trim() })}\n\n`);
            });
        });

        req.on('close', () => client.destroy());
    } else {
        let filePath = (req.url === '/') ? './index.html' : '.' + req.url;
        fs.readFile(filePath, (err, content) => {
            if (err) res.writeHead(404), res.end();
            else res.writeHead(200), res.end(content);
        });
    }
});

server.listen(PORT, () => console.log(`Server running at http://localhost:${PORT}/`));