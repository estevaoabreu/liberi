const net = require('net');
const http = require('http');
const fs = require('fs');
const path = require('path');

const PORT = 3000;
const WOKWI_PORT = 4001; // RFC2217 serial port (not 4000 which is GDB)

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

        // Gracefully catch connection errors to prevent backend terminal crashes
        client.on('error', (err) => {
            console.log(`[Wokwi Bridge] Connection issue on port ${WOKWI_PORT}: ${err.message}`);
        });

        client.connect(WOKWI_PORT, '127.0.0.1', () => {
            console.log(`[Wokwi Bridge] Successfully connected to simulation on port ${WOKWI_PORT}`);
        });

        client.on('data', (data) => {
            serialBuffer += data.toString();
            const lines = serialBuffer.split(/\r?\n/);
            serialBuffer = lines.pop() || '';
            lines.forEach(line => {
                if (line.trim()) {
                    res.write(`data: ${JSON.stringify({ type: 'wokwi', content: line.trim() })}\n\n`);
                }
            });
        });

        req.on('close', () => client.destroy());
    } else {
        let filePath = (req.url === '/') ? './index.html' : '.' + req.url;

        // Explicitly map content types for safe browser execution
        const extname = path.extname(filePath);
        let contentType = 'text/html';
        if (extname === '.js') contentType = 'text/javascript';
        if (extname === '.css') contentType = 'text/css';

        fs.readFile(filePath, (err, content) => {
            if (err) {
                res.writeHead(404, { 'Content-Type': 'text/plain' });
                res.end('404 Not Found');
            } else {
                // Apply strict anti-caching headers for clean live updates
                res.writeHead(200, {
                    'Content-Type': contentType,
                    'Cache-Control': 'no-store, no-cache, must-revalidate, proxy-revalidate',
                    'Pragma': 'no-cache',
                    'Expires': '0'
                });
                res.end(content);
            }
        });
    }
});

server.listen(PORT, () => console.log(`Server running at http://localhost:${PORT}/`));