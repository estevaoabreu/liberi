// ── State definitions ────────────────────────────────────────────────────────
const STATES = {
    ERROR: { badge: 'badge-error', color: '#ff2222', glow: 'rgba(255,34,34,0.45)', desc: 'I²C sensor error — check connections.' },
    IDLE: { badge: 'badge-idle', color: '#9e9993', glow: 'rgba(158,153,147,0.25)', desc: 'Waiting for activation…' },
    SAMPLING: { badge: 'badge-sampling', color: '#3059c8', glow: 'rgba(48,89,200,0.4)', desc: 'Collecting baseline data…' },
    CRITICAL_ILLNESS: { badge: 'badge-critical', color: '#b40000', glow: 'rgba(180,0,0,0.55)', desc: 'ALERT: High fever & critically low SpO₂.' },
    SEVERE_HYPOTHERMIA: { badge: 'badge-severe-hypo', color: '#7a0012', glow: 'rgba(122,0,18,0.5)', desc: 'ALERT: Severe hypothermia detected.' },
    EARLY_ILLNESS: { badge: 'badge-early-ill', color: '#ff5000', glow: 'rgba(255,80,0,0.45)', desc: 'Early illness signs: elevated temp & HR.' },
    MILD_HYPOTHERMIA: { badge: 'badge-mild-hypo', color: '#c8a000', glow: 'rgba(200,160,0,0.4)', desc: 'Mild hypothermia: low body temp.' },
    STRESS: { badge: 'badge-stress', color: '#6400b4', glow: 'rgba(100,0,180,0.45)', desc: 'Physiological stress markers detected.' },
    CIRCULATION: { badge: 'badge-circulation', color: '#00b8d9', glow: 'rgba(0,185,217,0.4)', desc: 'Low perfusion index — circulation concern.' },
    HEALTHY: { badge: 'badge-healthy', color: '#00c83c', glow: 'rgba(0,200,60,0.38)', desc: 'All vitals within healthy ranges.' },
};

// ── LED animation ─────────────────────────────────────────────────────────────
let ledCurrent = { r: 0, g: 0, b: 0 };
let ledTarget = { r: 0, g: 0, b: 0 };
let blinkInterval = null;
let currentState = null;

function hexToRgb(hex) {
    return {
        r: parseInt(hex.slice(1, 3), 16),
        g: parseInt(hex.slice(3, 5), 16),
        b: parseInt(hex.slice(5, 7), 16),
    };
}

function lerp(a, b, t) { return a + (b - a) * t; }

function animateLED() {
    const orb = document.getElementById('ledOrb');
    if (orb) {
        ledCurrent.r = lerp(ledCurrent.r, ledTarget.r, 0.08);
        ledCurrent.g = lerp(ledCurrent.g, ledTarget.g, 0.08);
        ledCurrent.b = lerp(ledCurrent.b, ledTarget.b, 0.08);
        orb.style.background =
            `rgb(${Math.round(ledCurrent.r)},${Math.round(ledCurrent.g)},${Math.round(ledCurrent.b)})`;
    }
    requestAnimationFrame(animateLED);
}

function setLEDColor(hex, glowColor) {
    const glow = document.getElementById('ledGlow');
    const { r, g, b } = hexToRgb(hex.padEnd(7, '0'));
    ledTarget = { r, g, b };
    if (glow) {
        glow.style.background = glowColor;
        glow.style.opacity = '1';
    }
}

// ── Background gradient (original radial-gradient approach) ──────────────────
function applyGradient(glowColor) {
    const wash = document.getElementById('gradientWash');
    if (!wash) return;
    const topColor = 'rgba(240,237,232,0.9)';
    wash.style.background = `
        radial-gradient(ellipse 100% 50% at 50% 0%,  ${topColor} 0%, transparent 100%),
        radial-gradient(ellipse 120% 60% at 50% 105%, ${glowColor} 0%, transparent 70%)
    `;
}

// ── State application ─────────────────────────────────────────────────────────
function applyState(stateName) {
    const cfg = STATES[stateName] || STATES.IDLE;
    if (stateName === currentState) return;

    if (blinkInterval) { clearInterval(blinkInterval); blinkInterval = null; }

    // LED blink for ERROR / SAMPLING
    if (stateName === 'ERROR') {
        let on = true;
        blinkInterval = setInterval(() => {
            on = !on;
            ledTarget = on ? hexToRgb('#ff2222') : { r: 0, g: 0, b: 0 };
        }, 250);
    } else if (stateName === 'SAMPLING') {
        let on = true;
        blinkInterval = setInterval(() => {
            on = !on;
            ledTarget = on ? hexToRgb('#3059c8') : { r: 5, g: 20, b: 60 };
        }, 500);
    } else {
        setLEDColor(cfg.color, cfg.glow);
    }

    // Background gradient (mirrors original's colour-coded body background)
    applyGradient(cfg.glow);

    // CSS variable for accent colour used by bars and state label
    document.documentElement.style.setProperty('--state-color', cfg.color);
    document.documentElement.style.setProperty('--state-glow', cfg.glow);

    // Badge / Label
    const label = document.getElementById('stateLabel');
    if (label) label.textContent = stateName.replace(/_/g, ' ');

    // Description
    const desc = document.getElementById('stateDesc');
    if (desc) desc.textContent = cfg.desc;

    addLog(stateName, cfg.color);
    currentState = stateName;
}

// ── Vitals update ─────────────────────────────────────────────────────────────
function setAlert(elId, alertClass) {
    const el = document.getElementById(elId);
    if (!el) return;
    el.classList.remove('alert-red', 'alert-amber');
    if (alertClass) el.classList.add(alertClass);
}

function updateVitals(temp, hr, spo2, pi, hrv, state) {
    // Temperature
    const tempEl = document.getElementById('temperature');
    if (tempEl) tempEl.textContent = temp > 0 ? temp.toFixed(1) + ' ºC' : '-- ºC';

    // Heart rate
    const hrEl = document.getElementById('heartrate');
    if (hrEl) hrEl.textContent = hr > 0 ? hr + ' bpm' : '-- bpm';

    // SpO2
    const spo2El = document.getElementById('oxygen');
    if (spo2El) spo2El.textContent = spo2 > 0 ? spo2 + ' %' : '-- %';

    // Perfusion Index
    const piEl = document.getElementById('pi');
    if (piEl) piEl.textContent = pi > 0 ? pi.toFixed(2) : '--';

    // HRV
    const hrvEl = document.getElementById('hrv');
    const hrvDisplay = hrv < 999 ? hrv + ' ms' : '-- ms';
    if (hrvEl) hrvEl.textContent = hrvDisplay;

    applyState(state);
}

// ── State log ─────────────────────────────────────────────────────────────────
function addLog(stateName, color) {
    const log = document.getElementById('stateLog');
    if (!log) return;
    const time = new Date().toLocaleTimeString('en-GB', { hour: '2-digit', minute: '2-digit', second: '2-digit' });
    const li = document.createElement('li');
    li.innerHTML = `
        <span class="log-time">${time}</span>
        <span class="log-state" style="color:${color}">${stateName.replace(/_/g, ' ')}</span>
    `;
    log.prepend(li);
    while (log.children.length > 30) log.removeChild(log.lastChild);
}

// ── Theme toggle ──────────────────────────────────────────────────────────────
document.addEventListener('DOMContentLoaded', () => {
    requestAnimationFrame(animateLED);

    const root = document.documentElement;

    // ── SSE connection ────────────────────────────────────────────────────────
    applyState('IDLE');

    const eventSource = new EventSource('/events');

    eventSource.onmessage = function (event) {
        try {
            const msg = JSON.parse(event.data);
            if (msg.type !== 'wokwi') return;

            const line = msg.content.trim();

            if (line === 'SYSTEM_BOOTED' || line === 'STATUS,OFF') {
                applyState('IDLE');
                return;
            }
            if (line === 'STATUS,ON') {
                applyState('SAMPLING');
                return;
            }

            // DATA,temp,hr,spo2,pi,hrv,STATE
            const m = line.match(/^DATA,([\d.]+),(\d+),(\d+),([\d.]+),(\d+),([A-Z_]+)$/);
            if (m) {
                updateVitals(
                    parseFloat(m[1]),
                    parseInt(m[2]),
                    parseInt(m[3]),
                    parseFloat(m[4]),
                    parseInt(m[5]),
                    m[6]
                );
            }
        } catch (e) {
            console.error('Error parsing SSE frame:', e);
        }
    };

    eventSource.onerror = function () {
        applyState('ERROR');
    };
});
