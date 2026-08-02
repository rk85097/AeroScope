import React, { useEffect, useState } from 'react';
import { createRoot } from 'react-dom/client';
import { MapPin, Radio, Settings, Shield, RefreshCw } from 'lucide-react';
import './styles.css';

function App() {
  const [status, setStatus] = useState(null);
  const [location, setLocation] = useState({ lat: 32.0853, lon: 34.7818, name: 'Tel Aviv' });

  async function refresh() {
    try {
      const res = await fetch('/api/status');
      setStatus(await res.json());
    } catch {
      setStatus({ providerStatus: 'offline in local preview', rangeNm: 20, mapEnabled: true, aircraftVisible: 0 });
    }
  }

  useEffect(() => {
    refresh();
    const id = setInterval(refresh, 5000);
    return () => clearInterval(id);
  }, []);

  function useBrowserLocation() {
    navigator.geolocation?.getCurrentPosition((pos) => {
      setLocation({ ...location, lat: pos.coords.latitude, lon: pos.coords.longitude });
    });
  }

  return (
    <main>
      <header>
        <div>
          <h1>AeroScope</h1>
          <p>Local control panel</p>
        </div>
        <button onClick={refresh} title="Refresh"><RefreshCw size={18} /></button>
      </header>

      <section className="grid">
        <article>
          <h2><Radio size={18} /> Radar</h2>
          <dl>
            <dt>Range</dt><dd>{status?.rangeNm ?? '--'} NM</dd>
            <dt>Map</dt><dd>{status?.mapEnabled ? 'On' : 'Off'}</dd>
            <dt>Aircraft</dt><dd>{status?.aircraftVisible ?? '--'}</dd>
            <dt>Provider</dt><dd>{status?.providerStatus ?? 'Loading'}</dd>
          </dl>
          <div className="actions">
            <button onClick={() => fetch('/api/range/next', { method: 'POST' }).then(refresh)}>Next range</button>
            <button onClick={() => fetch('/api/map/toggle', { method: 'POST' }).then(refresh)}>Toggle map</button>
          </div>
        </article>

        <article>
          <h2><MapPin size={18} /> Location</h2>
          <label>Name<input value={location.name} onChange={e => setLocation({ ...location, name: e.target.value })} /></label>
          <label>Latitude<input type="number" step="0.000001" value={location.lat} onChange={e => setLocation({ ...location, lat: Number(e.target.value) })} /></label>
          <label>Longitude<input type="number" step="0.000001" value={location.lon} onChange={e => setLocation({ ...location, lon: Number(e.target.value) })} /></label>
          <div className="actions">
            <button onClick={useBrowserLocation}>Use browser location</button>
            <button>Save and apply</button>
          </div>
        </article>

        <article>
          <h2><Settings size={18} /> Display</h2>
          <label>Theme<select><option>Auto sunrise/sunset</option><option>Dark</option><option>Light</option></select></label>
          <label>Units<select><option>Nautical</option><option>Metric</option></select></label>
          <label>Brightness<input type="range" min="1" max="255" defaultValue="210" /></label>
          <label>Label density<input type="range" min="0" max="10" defaultValue="5" /></label>
        </article>

        <article>
          <h2><Shield size={18} /> Maintenance</h2>
          <button>Export redacted config</button>
          <button>Retry map download</button>
          <button>Check OTA manifest</button>
          <button>Factory reset</button>
        </article>
      </section>
    </main>
  );
}

createRoot(document.getElementById('root')).render(<App />);
