import { useMemo } from 'react';
import { CartesianGrid, Line, LineChart, ResponsiveContainer, Tooltip, XAxis, YAxis } from 'recharts';
import { AlertTriangle, Car, Thermometer, Volume2, Wind } from 'lucide-react';
import { CircleMarker, MapContainer, Popup, TileLayer } from 'react-leaflet';
import { numericReading } from './telemetry';
import type { TelemetryCategory, TelemetryMessage } from './types';
import useNow from './useNow';

interface MapNode {
  id: string;
  lat: number;
  lng: number;
  type: string;
  category: TelemetryCategory;
  aqi?: number;
}

interface Alert {
  id: string;
  message: string;
  time: Date;
}

function markerColor(node: MapNode): string {
  if (node.category === 'Fleet') return '#eab308';
  if (node.category === 'Infrastructure') return '#3b82f6';
  if ((node.aqi ?? 0) > 150) return '#ef4444';
  return '#10b981';
}

function metric(value: number | undefined, digits: number): string {
  return value === undefined ? '—' : value.toFixed(digits);
}

export default function Dashboard({ messages }: { messages: TelemetryMessage[] }) {
  const now = useNow();
  const environmental = useMemo(() => messages.filter((message) => message.category === 'Environment'), [messages]);
  const newestEnvironmental = environmental[environmental.length - 1];
  const latest = newestEnvironmental && now - newestEnvironmental.receivedAt.getTime() <= 15 * 60 * 1000 ? newestEnvironmental : undefined;
  const temperature = latest ? numericReading(latest, 'temperature_c') : undefined;
  const aqi = latest ? numericReading(latest, 'aqi') : undefined;
  const noise = latest ? numericReading(latest, 'noise_db') : undefined;
  const traffic = latest ? numericReading(latest, 'traffic_count') : undefined;

  const chartData = useMemo(() => environmental.slice(-200).map((message) => ({
    time: message.receivedAt.toLocaleTimeString(),
    temp: numericReading(message, 'temperature_c'),
    aqi: numericReading(message, 'aqi'),
  })), [environmental]);

  const alerts = useMemo(() => {
    const newestByNode = new Map<string, TelemetryMessage>();
    messages.forEach((message) => newestByNode.set(message.node_id, message));
    const active: Alert[] = [];
    newestByNode.forEach((message) => {
      if (now - message.receivedAt.getTime() > 15 * 60 * 1000) return;
      const nodeAqi = numericReading(message, 'aqi');
      const nodeTemperature = numericReading(message, 'temperature_c');
      const battery = numericReading(message, 'battery_v');
      if (nodeAqi !== undefined && nodeAqi > 150) active.push({ id: `${message.node_id}:aqi`, message: `High AQI (${nodeAqi.toFixed(0)}) at ${message.sensor_id}`, time: message.receivedAt });
      if (nodeTemperature !== undefined && nodeTemperature > 35) active.push({ id: `${message.node_id}:temp`, message: `Temperature anomaly (${nodeTemperature.toFixed(1)}°C) at ${message.sensor_id}`, time: message.receivedAt });
      if (battery !== undefined && battery < 3.7) active.push({ id: `${message.node_id}:battery`, message: `Low battery on node ${message.node_id}`, time: message.receivedAt });
    });
    return active.sort((left, right) => right.time.getTime() - left.time.getTime());
  }, [messages, now]);

  const mapNodes = useMemo(() => {
    const newest = new Map<string, MapNode>();
    messages.forEach((message) => {
      if (now - message.receivedAt.getTime() > 15 * 60 * 1000) return;
      const lat = numericReading(message, 'latitude'); const lng = numericReading(message, 'longitude');
      if (lat !== undefined && lng !== undefined && lat >= -90 && lat <= 90 && lng >= -180 && lng <= 180) {
        newest.set(message.node_id, { id: message.node_id, lat, lng, type: message.sensor_id, category: message.category, aqi: numericReading(message, 'aqi') });
      }
    });
    return [...newest.values()];
  }, [messages, now]);

  return <section>
    <div className="metrics-grid">
      <div className={`metric-card glass-panel ${temperature !== undefined && temperature > 35 ? 'alert' : ''}`}><div className="metric-header"><Thermometer size={20} /><span>Temperature</span></div><div className="metric-value">{metric(temperature, 1)}<span className="metric-unit">°C</span></div></div>
      <div className={`metric-card glass-panel ${aqi !== undefined && aqi > 150 ? 'alert' : ''}`}><div className="metric-header"><Wind size={20} /><span>Air Quality</span></div><div className="metric-value">{metric(aqi, 0)}<span className="metric-unit">AQI</span></div></div>
      <div className="metric-card glass-panel"><div className="metric-header"><Volume2 size={20} /><span>Ambient Noise</span></div><div className="metric-value">{metric(noise, 1)}<span className="metric-unit">dB</span></div></div>
      <div className="metric-card glass-panel"><div className="metric-header"><Car size={20} /><span>Traffic Flow</span></div><div className="metric-value">{metric(traffic, 0)}<span className="metric-unit">cars/min</span></div></div>
    </div>
    <div className="dashboard-grid">
      <div className="map-container glass-panel"><MapContainer center={[39.72, 140.1026]} zoom={15}><TileLayer attribution='&copy; <a href="https://carto.com/">CartoDB</a>' url="https://{s}.basemaps.cartocdn.com/dark_all/{z}/{x}/{y}{r}.png" />
        {mapNodes.map((node) => <CircleMarker key={node.id} center={[node.lat, node.lng]} radius={node.category === 'Fleet' ? 10 : 8} pathOptions={{ color: markerColor(node), fillColor: markerColor(node), fillOpacity: 0.8 }}><Popup><strong>{node.type}</strong><br />ID: {node.id}<br />Category: {node.category}{node.aqi === undefined ? '' : <><br />AQI: {node.aqi.toFixed(0)}</>}</Popup></CircleMarker>)}
      </MapContainer></div>
      <div className="alerts-panel glass-panel"><h3 className="section-heading"><AlertTriangle size={18} className="danger-text" /> Active Incidents</h3>
        {alerts.length === 0 ? <div className="empty-state silver-text">No active alerts</div> : alerts.map((alert) => <div key={alert.id} className="alert-item"><div className="alert-time">{alert.time.toLocaleString()}</div><div className="alert-message">{alert.message}</div></div>)}
      </div>
    </div>
    <div className="chart-container glass-panel"><h3>Environmental Telemetry Trends</h3><ResponsiveContainer width="100%" height="90%"><LineChart data={chartData}><CartesianGrid strokeDasharray="3 3" stroke="rgba(255,255,255,0.1)" /><XAxis dataKey="time" stroke="#c0c0c0" /><YAxis stroke="#c0c0c0" yAxisId="left" /><YAxis stroke="#c0c0c0" yAxisId="right" orientation="right" /><Tooltip contentStyle={{ backgroundColor: '#1a1a1a', border: '1px solid #10b981' }} /><Line yAxisId="left" type="monotone" dataKey="temp" stroke="#10b981" dot={false} name="Temperature (°C)" /><Line yAxisId="right" type="monotone" dataKey="aqi" stroke="#ef4444" dot={false} name="AQI" /></LineChart></ResponsiveContainer></div>
  </section>;
}
