import { useMemo, useState } from 'react';
import { Building2, Plus } from 'lucide-react';

export default function Infrastructure({ messages }: { messages: any[] }) {
  const [showAddForm, setShowAddForm] = useState(false);

  const infra = useMemo(() => {
    const infraMap = new Map();
    messages.filter(m => m.category === 'Infrastructure').forEach(msg => {
      infraMap.set(msg.node_id, {
        id: msg.node_id,
        type: msg.sensor_id,
        lat: msg.readings.latitude,
        lng: msg.readings.longitude,
        state: msg.readings
      });
    });
    return Array.from(infraMap.values());
  }, [messages]);

  return (
    <div>
      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '2rem' }}>
        <h3 style={{ fontWeight: 500, display: 'flex', alignItems: 'center', gap: '0.5rem' }}>
          <Building2 className="emerald-text" /> Municipal Infrastructure Registry
        </h3>
        <button 
            onClick={() => setShowAddForm(!showAddForm)}
            style={{ 
            background: 'var(--color-emerald)', 
            border: 'none', 
            color: 'var(--color-white)',
            padding: '0.75rem 1.5rem',
            borderRadius: '8px',
            cursor: 'pointer',
            display: 'flex',
            alignItems: 'center',
            gap: '0.5rem',
            fontWeight: 600
        }}>
          <Plus size={18} /> Register Asset
        </button>
      </div>

      {showAddForm && (
        <div className="glass-panel" style={{ padding: '2rem', marginBottom: '2rem', border: '1px solid var(--color-emerald)' }}>
            <h4 style={{ marginBottom: '1rem' }}>Register New Asset (Mock)</h4>
            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '1rem' }}>
                <input type="text" placeholder="Asset ID" style={{ padding: '0.75rem', borderRadius: '4px', border: '1px solid #333', background: '#111', color: '#fff' }} />
                <select style={{ padding: '0.75rem', borderRadius: '4px', border: '1px solid #333', background: '#111', color: '#fff' }}>
                    <option>Street Light</option>
                    <option>Park Monitor</option>
                    <option>Pool System</option>
                    <option>People Counter</option>
                </select>
                <input type="text" placeholder="Latitude" style={{ padding: '0.75rem', borderRadius: '4px', border: '1px solid #333', background: '#111', color: '#fff' }} />
                <input type="text" placeholder="Longitude" style={{ padding: '0.75rem', borderRadius: '4px', border: '1px solid #333', background: '#111', color: '#fff' }} />
            </div>
            <button style={{ marginTop: '1rem', background: '#1a1a1a', border: '1px solid var(--color-silver)', color: '#fff', padding: '0.5rem 1rem', borderRadius: '4px', cursor: 'pointer' }} onClick={() => setShowAddForm(false)}>
                Save Asset
            </button>
        </div>
      )}

      <div className="node-table-container glass-panel" style={{ padding: '2rem' }}>
        <table>
          <thead>
            <tr>
              <th>Asset ID</th>
              <th>Category</th>
              <th>Location</th>
              <th>Live Status</th>
            </tr>
          </thead>
          <tbody>
            {infra.map(i => (
              <tr key={i.id}>
                <td style={{ fontFamily: 'monospace' }}>{i.id}</td>
                <td>{i.type}</td>
                <td className="silver-text">{i.lat.toFixed(4)}, {i.lng.toFixed(4)}</td>
                <td>
                  <span className="emerald-text">
                    {i.type === 'Street Light' && `Light: ${i.state.on ? 'ON' : 'OFF'} (${i.state.brightness}%)`}
                    {i.type === 'Park Monitor' && `People Count: ${i.state.people_count}`}
                    {i.type === 'Pool System' && `Pump: ${i.state.pump_active ? 'ON' : 'OFF'} | Temp: ${i.state.temp_c}°C`}
                  </span>
                </td>
              </tr>
            ))}
            {infra.length === 0 && (
                <tr>
                    <td colSpan={4} style={{textAlign: 'center', color: 'var(--color-silver)'}}>No infrastructure data found.</td>
                </tr>
            )}
          </tbody>
        </table>
      </div>
    </div>
  );
}
