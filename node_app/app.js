const net = require('net');

const WOKWI_PORT = 4000;
const WOKWI_HOST = '127.0.0.1';

const client = new net.Socket();
let dataBuffer = ""; // To store partial messages

client.connect(WOKWI_PORT, WOKWI_HOST, () => {
    console.log('✅ Connected to Simulated Arduino Uno!');
    console.log('Waiting for data... (Make sure to press the button in Wokwi)');
});

client.on('data', (chunk) => {
    // Append the new chunk of data to our buffer
    dataBuffer += chunk.toString();

    // Check if we have at least one complete line (ending in \n)
    if (dataBuffer.includes('\n')) {
        let lines = dataBuffer.split('\n');

        // The last element might be a partial line, keep it in the buffer
        dataBuffer = lines.pop();

        lines.forEach((line) => {
            const cleanLine = line.trim();

            // Only process lines that start with our "DATA" tag
            if (cleanLine.startsWith("DATA,")) {
                const parts = cleanLine.split(",");

                // Check if we have all 4 parts: DATA, Temp, Vital1, Vital2
                if (parts.length === 4) {
                    const sensorData = {
                        temp: parts[1],
                        vital1: parts[2],
                        vital2: parts[3],
                        timestamp: new Date().toLocaleTimeString()
                    };

                    console.log("----------------------------");
                    console.log(`[${sensorData.timestamp}] RECEIVING VITALS:`);
                    console.log(`🌡️  Temperature: ${sensorData.temp}°C`);
                    console.log(`💓 Vital 1:      ${sensorData.vital1}`);
                    console.log(`🫁 Vital 2:      ${sensorData.vital2}`);
                }
            } else if (cleanLine.startsWith("STATUS,")) {
                console.log(`System Notification: ${cleanLine}`);
            }
        });
    }
});

client.on('error', (err) => {
    console.error('❌ Connection error:', err.message);
    console.log('Make sure Wokwi is running and rfc2217ServerPort is set to 4000.');
});

client.on('close', () => {
    console.log('📡 Connection to Wokwi closed.');
});