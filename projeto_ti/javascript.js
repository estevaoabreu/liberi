// Connect to the local server's SSE endpoint
const eventSource = new EventSource('/events');

const temperature = document.getElementById('temperature');
const heartrate = document.getElementById('heartrate');
const oxygen = document.getElementById('oxygen');
const pageBody = document.body;

function parseSensorLine(output) {
  // Accept DATA lines even if there is leading/trailing noise around the payload.
  const match = output.match(/DATA\s*,\s*(-?\d+(?:\.\d+)?)\s*,\s*(\d+)\s*,\s*(\d+)/i);
  if (!match) {
    return null;
  }

  return {
    temp: parseFloat(match[1]),
    heartrate: parseInt(match[2], 10),
    oxygen: parseInt(match[3], 10)
  };
}

eventSource.onmessage = function (event) {
  try {
    const data = JSON.parse(event.data);

    if (data.type === 'wokwi') {
      const output = data.content;

      const sensorData = parseSensorLine(output);
      if (sensorData) {

        console.log('Parsed Sensor Data:', sensorData);

        if (!isNaN(sensorData.temp)) {
          temperature.textContent = `${sensorData.temp.toFixed(1)} ºC`;
        } else {
          temperature.textContent = 'N/A';
        }

        if (!isNaN(sensorData.heartrate)) {
          heartrate.textContent = `${sensorData.heartrate} bpm`;
        } else {
          heartrate.textContent = 'N/A';
        }

        if (!isNaN(sensorData.oxygen)) {
          oxygen.textContent = `${sensorData.oxygen} %`;
        } else {
          oxygen.textContent = 'N/A';
        }

        if (sensorData.heartrate > 160 || sensorData.heartrate < 80 || sensorData.temp > 38 || sensorData.oxygen < 92) {
          pageBody.style.background = `
    radial-gradient(ellipse at top, #d0d0d0, transparent),
    radial-gradient(ellipse at bottom, #fd0000, transparent)
          `;
        } else {
          pageBody.style.background = `
    radial-gradient(ellipse at top, #d0d0d0, transparent),
    radial-gradient(ellipse at bottom, #73ff00, transparent)
          `;
        }

      } else {
        console.log('System Status:', output);
      }
    } else {
      console.log('Server info:', data.message);
    }
  } catch (err) {
    console.error('Error parsing SSE data:', err);
  }
};

eventSource.onerror = function (err) {
  console.error('EventSource connection error:', err);
};