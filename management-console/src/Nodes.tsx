import { useMemo } from 'react';
import { Battery, Signal } from 'lucide-react';

export default function Nodes({ messages }: { messages: any[] }) {
  
  // Unique nodes from messages
  const nodes = useMemo(() => {
    const map = new Map();
    messages.forEach(msg => {
      if (msg.node_id) {
        map.set(msg.node_id, {
          id: msg.node_id,
          sensor: msg.sensor_id || 'Unknown',
          lastSeen: new Date(msg.receivedAt),
          role: 'Sensor', // Assuming sensor since we get telemetry
          battery: msg.readings?.battery_v,
          rssi: msg.readings?.rssi
        });
      }
    });
    // Add a mock gateway
    map.set('GW_001', {
        id: 'GW_001',
        sensor: 'Gateway Hub',
        lastSeen: new Date(),
        role: 'Gateway',
        battery: 5.0,
        rssi: -40
    });
    return Array.from(map.values());
  }, [messages]);

  return (
    <div className="glass-panel" style={{ padding: '2rem' }}>
      <h3 style={{ marginBottom: '1.5rem', fontWeight: 500 }}>Discovered Mesh Nodes</h3>
      
      <div className="node-table-container">
        <table>
          <thead>
            <tr>
              <th>Node ID</th>
              <th>Status</th>
              <th>Role</th>
              <th>Configuration</th>
              <th>Battery</th>
              <th>Signal (RSSI)</th>
              <th>Last Seen</th>
              <th>Action</th>
            </tr>
          </thead>
          <tbody>
            {nodes.map(node => {
              const isOffline = (new Date().getTime() - node.lastSeen.getTime()) > 30000;
              return (
                <tr key={node.id}>
                  <td style={{ fontFamily: 'monospace', fontWeight: 600 }}>{node.id}</td>
                  <td>
                    <div style={{ display: 'flex', alignItems: 'center', gap: '0.5rem' }}>
                      <div style={{ width: 8, height: 8, borderRadius: '50%', background: isOffline ? '#ef4444' : '#10b981' }} />
                      <span className={isOffline ? 'danger-text' : 'emerald-text'}>{isOffline ? 'Offline' : 'Online'}</span>
                    </div>
                  </td>
                  <td>
                    <span className={`badge ${node.role === 'Gateway' ? 'badge-gateway' : 'badge-sensor'}`}>
                      {node.role}
                    </span>
                  </td>
                  <td className="silver-text">{node.sensor}</td>
                  <td>
                    <div style={{ display: 'flex', alignItems: 'center', gap: '0.25rem' }}>
                      <Battery size={16} className={node.battery < 3.7 ? "danger-text" : "emerald-text"} />
                      {node.battery ? `${node.battery.toFixed(2)} V` : '--'}
                    </div>
                  </td>
                  <td>
                    <div style={{ display: 'flex', alignItems: 'center', gap: '0.25rem' }}>
                      <Signal size={16} className={node.rssi < -90 ? "danger-text" : "emerald-text"} />
                      {node.rssi ? `${node.rssi.toFixed(0)} dBm` : '--'}
                    </div>
                  </td>
                  <td className="silver-text">{node.lastSeen.toLocaleTimeString()}</td>
                  <td>
                    <button style={{ 
                        background: 'transparent', 
                        border: '1px solid var(--color-emerald)', 
                        color: 'var(--color-emerald)',
                        padding: '0.25rem 0.75rem',
                        borderRadius: '4px',
                        cursor: 'pointer'
                    }}>
                      Manage
                    </button>
                  </td>
                </tr>
              )
            })}
            {nodes.length === 0 && (
                <tr>
                    <td colSpan={8} style={{textAlign: 'center', color: 'var(--color-silver)'}}>No nodes discovered yet.</td>
                </tr>
            )}
          </tbody>
        </table>
      </div>
    </div>
  );
}
