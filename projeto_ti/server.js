const net = require('net');
const http = require('http');
const fs = require('fs');
const path = require('path');

const PORT = 3000;
const WOKWI_PORT = 4000;

// Set up HTTP Server to serve static files and SSE
const server = http.createServer((req, res) => {
    if (req.url === '/events') {
        // Server-Sent Events endpoint
        res.writeHead(200, {
            'Content-Type': 'text/event-stream',
            'Cache-Control': 'no-cache',
            'Connection': 'keep-alive',
            'Access-Control-Allow-Origin': '*'
        });

        // Send an initial message
        res.write(`data: {"type": "info", "message": "Connected to SSE"}\n\n`);

        let serialBuffer = '';

        const handleData = (data) => {
            serialBuffer += data.toString();

            // TCP data may arrive in partial chunks; emit complete lines only.
            const lines = serialBuffer.split(/\r?\n/);
            serialBuffer = lines.pop() || '';

            for (const line of lines) {
                const trimmed = line.trim();
                if (trimmed) {
                    res.write(`data: ${JSON.stringify({ type: 'wokwi', content: trimmed })}\n\n`);
                }
            }
        };

        // Connect to Wokwi
        const client = new net.Socket();
        client.connect(WOKWI_PORT, '127.0.0.1', () => {
            console.log('Connected to Wokwi Serial Simulator');
        });

        client.on('data', handleData);

        client.on('error', (err) => {
            console.error('Connection error. Is Wokwi running? Error:', err.message);
        });

        req.on('close', () => {
            const remaining = serialBuffer.trim();
            if (remaining) {
                res.write(`data: ${JSON.stringify({ type: 'wokwi', content: remaining })}\n\n`);
            }
            client.destroy();
        });

    } else {
        // Serve static files
        let filePath = '.' + req.url;
        if (filePath === './') {
            filePath = './index.html';
        }

        const extname = path.extname(filePath);
        let contentType = 'text/html';
        switch (extname) {
            case '.js':
                contentType = 'text/javascript';
                break;
            case '.css':
                contentType = 'text/css';
                break;
            case '.json':
                contentType = 'application/json';
                break;
            case '.png':
                contentType = 'image/png';
                break;
            case '.jpg':
                contentType = 'image/jpg';
                break;
        }

        fs.readFile(filePath, (error, content) => {
            if (error) {
                if(error.code == 'ENOENT'){
                    res.writeHead(404);
                    res.end('File not found');
                } else {
                    res.writeHead(500);
                    res.end('Server Error: '+error.code);
                }
            } else {
                res.writeHead(200, { 'Content-Type': contentType });
                res.end(content, 'utf-8');
            }
        });
    }
});

server.listen(PORT, () => {
    console.log(`\n==============================================`);
    console.log(`Server running at http://localhost:${PORT}/`);
    console.log(`Make sure Wokwi simulation is running!`);
    console.log(`==============================================\n`);
});
