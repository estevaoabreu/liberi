const eventSource = new EventSource('/events');
const tempEl = document.getElementById('temperature');
const heartEl = document.getElementById('heartrate');
const oxyEl = document.getElementById('oxygen');

eventSource.onmessage = function (event) {
    const data = JSON.parse(event.data);
    if (data.type === 'wokwi') {
        const match = data.content.match(/DATA,(-?\d+),(\d+),(\d+)/i);
        if (match) {
            tempEl.textContent = `${match[1]} ºC`;
            heartEl.textContent = `${match[2]} bpm`;
            oxyEl.textContent = `${match[3]} %`;

            // Visual alert logic
            document.body.style.background = (match[2] > 160 || match[1] > 38) ? '#ff000033' : '#73ff0033';
        }
    }
};