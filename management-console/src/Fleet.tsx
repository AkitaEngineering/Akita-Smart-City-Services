import { useMemo } from 'react';
import { MapContainer, TileLayer, CircleMarker, Popup } from 'react-leaflet';
import { Truck } from 'lucide-react';

export default function Fleet({ messages }: { messages: any[] }) {
  const fleets = useMemo(() => {
    const fleetMap = new Map();
    messages.filter(m => m.category === 'Fleet').forEach(msg => {
      fleetMap.set(msg.node_id, {
        id: msg.node_id,
        type: msg.sensor_id,
        lat: msg.readings.latitude,
        lng: msg.readings.longitude,
        speed: msg.readings.speed_kmh
      });
    });
    return Array.from(fleetMap.values());
  }, [messages]);

  return (
    <div>
      <div className="glass-panel" style={{ padding: '2rem', marginBottom: '2rem' }}>
        <h3 style={{ marginBottom: '1.5rem', fontWeight: 500, display: 'flex', alignItems: 'center', gap: '0.5rem' }}>
          <Truck className="emerald-text" /> Active Fleet Operations
        </h3>
        
        <div style={{ height: '400px', borderRadius: '12px', overflow: 'hidden' }}>
          <MapContainer center={[39.7200, 140.1026]} zoom={14} style={{ height: '100%', width: '100%' }}>
            <TileLayer
              attribution='&copy; CartoDB'
              url="https://{s}.basemaps.cartocdn.com/dark_all/{z}/{x}/{y}{r}.png"
            />
            {fleets.map(f => (
              <CircleMarker 
                key={f.id} 
                center={[f.lat, f.lng]} 
                radius={10}
                pathOptions={{ color: '#eab308', fillColor: '#eab308', fillOpacity: 0.9 }}
              >
                <Popup>
                  <div style={{ color: '#000' }}>
                    <strong>{f.type}</strong><br/>
                    ID: {f.id}<br/>
                    Speed: {f.speed?.toFixed(1)} km/h
                  </div>
                </Popup>
              </CircleMarker>
            ))}
          </MapContainer>
        </div>
      </div>

      <div className="node-table-container glass-panel" style={{ padding: '2rem' }}>
        <table>
          <thead>
            <tr>
              <th>Vehicle ID</th>
              <th>Type</th>
              <th>Current Speed</th>
              <th>Coordinates</th>
            </tr>
          </thead>
          <tbody>
            {fleets.map(f => (
              <tr key={f.id}>
                <td style={{ fontFamily: 'monospace' }}>{f.id}</td>
                <td>{f.type}</td>
                <td className="emerald-text">{f.speed?.toFixed(1)} km/h</td>
                <td className="silver-text">{f.lat.toFixed(4)}, {f.lng.toFixed(4)}</td>
              </tr>
            ))}
            {fleets.length === 0 && (
                <tr>
                    <td colSpan={4} style={{textAlign: 'center', color: 'var(--color-silver)'}}>No active fleet data.</td>
                </tr>
            )}
          </tbody>
        </table>
      </div>
    </div>
  );
}
