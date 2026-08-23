import { useMemo } from 'react';
import { CircleMarker, MapContainer, Popup, TileLayer } from 'react-leaflet';
import { Truck } from 'lucide-react';
import { numericReading } from './telemetry';
import type { TelemetryMessage } from './types';
import useNow from './useNow';

export default function Fleet({ messages }: { messages: TelemetryMessage[] }) {
  const now = useNow();
  const fleet = useMemo(() => {
    const latest = new Map<string, TelemetryMessage>();
    messages.filter((message) => message.category === 'Fleet').forEach((message) => latest.set(message.node_id, message));
    return [...latest.values()].flatMap((message) => {
      if (now - message.receivedAt.getTime() > 15 * 60 * 1000) return [];
      const lat = numericReading(message, 'latitude'); const lng = numericReading(message, 'longitude');
      if (lat === undefined || lng === undefined || lat < -90 || lat > 90 || lng < -180 || lng > 180) return [];
      return [{ id: message.node_id, type: message.sensor_id, lat, lng, speed: numericReading(message, 'speed_kmh'), lastSeen: message.receivedAt }];
    });
  }, [messages, now]);

  return <section>
    <div className="glass-panel form-panel"><h3 className="section-heading"><Truck className="emerald-text" /> Active Fleet Operations</h3>
      <div className="fleet-map"><MapContainer center={[39.72, 140.1026]} zoom={14}><TileLayer attribution="&copy; CartoDB" url="https://{s}.basemaps.cartocdn.com/dark_all/{z}/{x}/{y}{r}.png" />
        {fleet.map((vehicle) => <CircleMarker key={vehicle.id} center={[vehicle.lat, vehicle.lng]} radius={10} pathOptions={{ color: '#eab308', fillColor: '#eab308', fillOpacity: 0.9 }}><Popup><strong>{vehicle.type}</strong><br />ID: {vehicle.id}<br />Speed: {vehicle.speed?.toFixed(1) ?? '—'} km/h</Popup></CircleMarker>)}
      </MapContainer></div>
    </div>
    <div className="node-table-container glass-panel table-panel"><table><thead><tr><th>Vehicle ID</th><th>Type</th><th>Speed</th><th>Coordinates</th><th>Last report</th></tr></thead><tbody>
      {fleet.map((vehicle) => <tr key={vehicle.id}><td className="mono">{vehicle.id}</td><td>{vehicle.type}</td><td className="emerald-text">{vehicle.speed?.toFixed(1) ?? '—'} km/h</td><td>{vehicle.lat.toFixed(5)}, {vehicle.lng.toFixed(5)}</td><td>{vehicle.lastSeen.toLocaleString()}</td></tr>)}
      {fleet.length === 0 && <tr><td colSpan={5} className="empty-cell">No active fleet data.</td></tr>}
    </tbody></table></div>
  </section>;
}
