import { useMemo } from 'react';
import { LineChart, Line, XAxis, YAxis, CartesianGrid, Tooltip, ResponsiveContainer } from 'recharts';
import { Thermometer, Wind, Volume2, Car, AlertTriangle } from 'lucide-react';
import { MapContainer, TileLayer, Popup, CircleMarker } from 'react-leaflet';
import L from 'leaflet';

// Fix for default leaflet icons
delete (L.Icon.Default.prototype as any)._getIconUrl;
L.Icon.Default.mergeOptions({
  iconRetinaUrl: 'https://cdnjs.cloudflare.com/ajax/libs/leaflet/1.7.1/images/marker-icon-2x.png',
  iconUrl: 'https://cdnjs.cloudflare.com/ajax/libs/leaflet/1.7.1/images/marker-icon.png',
  shadowUrl: 'https://cdnjs.cloudflare.com/ajax/libs/leaflet/1.7.1/images/marker-shadow.png',
});

export default function Dashboard({ messages }: { messages: any[] }) {
  
  // Extract latest readings
  const latest = useMemo(() => {
    if (messages.length === 0) return null;
    return messages[messages.length - 1];
  }, [messages]);

  // Format data for chart
  const chartData = useMemo(() => {
    return messages.map(msg => ({
      time: new Date(msg.receivedAt).toLocaleTimeString(),
      temp: msg.readings?.temperature_c,
      aqi: msg.readings?.aqi
    }));
  }, [messages]);

  // Compute Active Alerts
  const alerts = useMemo(() => {
    const activeAlerts = [];
    if (latest) {
      if (latest.readings?.aqi > 150) {
        activeAlerts.push({ id: 1, type: 'critical', msg: `High AQI detected (${latest.readings.aqi.toFixed(0)}) at ${latest.sensor_id}`, time: new Date(latest.receivedAt) });
      }
      if (latest.readings?.temperature_c > 35) {
        activeAlerts.push({ id: 2, type: 'warning', msg: `Temperature Anomaly (${latest.readings.temperature_c.toFixed(1)}°C) at ${latest.sensor_id}`, time: new Date(latest.receivedAt) });
      }
      if (latest.readings?.battery_v < 3.7) {
        activeAlerts.push({ id: 3, type: 'info', msg: `Low Battery on Node ${latest.node_id}`, time: new Date(latest.receivedAt) });
      }
    }
    return activeAlerts.sort((a,b) => b.time.getTime() - a.time.getTime());
  }, [latest]);

  // Map node locations
  const mapNodes = useMemo(() => {
    const nodeMap = new Map();
    messages.forEach(msg => {
      if (msg.readings?.latitude && msg.readings?.longitude) {
        nodeMap.set(msg.node_id, {
          id: msg.node_id,
          lat: msg.readings.latitude,
          lng: msg.readings.longitude,
          type: msg.sensor_id,
          category: msg.category || 'Environment',
          aqi: msg.readings.aqi
        });
      }
    });
    return Array.from(nodeMap.values());
  }, [messages]);

  const mapCenter = [39.7200, 140.1026] as [number, number]; // Akita City

  const getMarkerColor = (node: any) => {
      if (node.category === 'Fleet') return '#eab308'; // Yellow for fleet
      if (node.category === 'Infrastructure') return '#3b82f6'; // Blue for static infra
      if (node.aqi > 150) return '#ef4444'; // Red for environmental anomaly
      return '#10b981'; // Green for normal environmental
  };

  return (
    <div>
      <div className="metrics-grid">
        <div className={`metric-card glass-panel ${latest?.readings?.temperature_c > 35 ? 'alert' : ''}`}>
          <div className="metric-header">
            <Thermometer size={20} className={latest?.readings?.temperature_c > 35 ? "danger-text" : "emerald-text"} />
            <span>Temperature</span>
          </div>
          <div className="metric-value">
            {latest?.readings?.temperature_c ? latest.readings.temperature_c.toFixed(1) : '--'}
            <span className="metric-unit">°C</span>
          </div>
        </div>

        <div className={`metric-card glass-panel ${latest?.readings?.aqi > 150 ? 'alert' : ''}`}>
          <div className="metric-header">
            <Wind size={20} className={latest?.readings?.aqi > 150 ? "danger-text" : "emerald-text"} />
            <span>Air Quality (AQI)</span>
          </div>
          <div className="metric-value">
            {latest?.readings?.aqi ? latest.readings.aqi.toFixed(0) : '--'}
            <span className="metric-unit">Index</span>
          </div>
        </div>

        <div className="metric-card glass-panel">
          <div className="metric-header">
            <Volume2 size={20} className="emerald-text" />
            <span>Ambient Noise</span>
          </div>
          <div className="metric-value">
            {latest?.readings?.noise_db ? latest.readings.noise_db.toFixed(1) : '--'}
            <span className="metric-unit">dB</span>
          </div>
        </div>

        <div className="metric-card glass-panel">
          <div className="metric-header">
            <Car size={20} className="emerald-text" />
            <span>Traffic Flow</span>
          </div>
          <div className="metric-value">
            {latest?.readings?.traffic_count !== undefined ? latest.readings.traffic_count : '--'}
            <span className="metric-unit">cars/min</span>
          </div>
        </div>
      </div>

      <div className="dashboard-grid">
        <div className="map-container glass-panel">
          <MapContainer center={mapCenter} zoom={15} style={{ height: '100%', width: '100%' }}>
            <TileLayer
              attribution='&copy; <a href="https://carto.com/">CartoDB</a>'
              url="https://{s}.basemaps.cartocdn.com/dark_all/{z}/{x}/{y}{r}.png"
            />
            {mapNodes.map(node => (
              <CircleMarker 
                key={node.id} 
                center={[node.lat, node.lng]} 
                radius={node.category === 'Fleet' ? 10 : 8}
                pathOptions={{ 
                    color: getMarkerColor(node), 
                    fillColor: getMarkerColor(node), 
                    fillOpacity: node.category === 'Fleet' ? 0.9 : 0.7 
                }}
              >
                <Popup>
                  <div style={{ color: '#000' }}>
                    <strong>{node.type} ({node.category})</strong><br/>
                    ID: {node.id}<br/>
                    {node.category === 'Environment' && `AQI: ${node.aqi?.toFixed(0)}`}
                  </div>
                </Popup>
              </CircleMarker>
            ))}
          </MapContainer>
        </div>

        <div className="alerts-panel glass-panel">
          <h3 style={{ marginBottom: '1rem', fontWeight: 500, display: 'flex', alignItems: 'center', gap: '0.5rem' }}>
            <AlertTriangle size={18} className="danger-text" />
            Active Incidents
          </h3>
          {alerts.length === 0 ? (
            <div className="silver-text" style={{ textAlign: 'center', marginTop: '2rem' }}>No active alerts</div>
          ) : (
            alerts.map((alert, i) => (
              <div key={i} className="alert-item">
                <div className="alert-time">{alert.time.toLocaleTimeString()}</div>
                <div className="alert-message">{alert.msg}</div>
              </div>
            ))
          )}
        </div>
      </div>

      <div className="chart-container glass-panel">
        <h3 style={{ marginBottom: '1.5rem', fontWeight: 500 }}>Telemetry Trends</h3>
        <ResponsiveContainer width="100%" height="100%">
          <LineChart data={chartData}>
            <CartesianGrid strokeDasharray="3 3" stroke="rgba(255,255,255,0.1)" />
            <XAxis dataKey="time" stroke="#c0c0c0" />
            <YAxis stroke="#c0c0c0" yAxisId="left" />
            <YAxis stroke="#c0c0c0" yAxisId="right" orientation="right" />
            <Tooltip 
              contentStyle={{ backgroundColor: '#1a1a1a', border: '1px solid #10b981', borderRadius: '8px' }}
              itemStyle={{ color: '#fff' }}
            />
            <Line yAxisId="left" type="monotone" dataKey="temp" stroke="#10b981" strokeWidth={3} dot={false} name="Temp (°C)" />
            <Line yAxisId="right" type="monotone" dataKey="aqi" stroke="#ef4444" strokeWidth={2} dot={false} name="AQI" />
          </LineChart>
        </ResponsiveContainer>
      </div>
    </div>
  );
}
