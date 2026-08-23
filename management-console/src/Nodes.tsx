import { useMemo } from 'react';
import { Battery, Signal } from 'lucide-react';
import { numericReading } from './telemetry';
import type { TelemetryMessage } from './types';
import useNow from './useNow';

export default function Nodes({ messages }: { messages: TelemetryMessage[] }) {
  const now = useNow();
  const nodes = useMemo(() => {
    const discovered = new Map<string, TelemetryMessage>();
    messages.forEach((message) => discovered.set(message.node_id, message));
    return [...discovered.values()].sort((left, right) => left.node_id.localeCompare(right.node_id));
  }, [messages]);

  return (
    <section className="glass-panel table-panel">
      <h3 className="section-heading">Discovered Mesh Nodes</h3>
      <div className="node-table-container"><table><thead><tr><th>Node ID</th><th>Status</th><th>Role</th><th>Configuration</th><th>Battery</th><th>Signal</th><th>Last Seen</th></tr></thead>
        <tbody>
          {nodes.map((node) => {
            const battery = numericReading(node, 'battery_v');
            const rssi = numericReading(node, 'rssi');
            const offline = now - node.receivedAt.getTime() > 15 * 60 * 1000;
            return <tr key={node.node_id}>
              <td className="mono">{node.node_id}</td><td><span className={offline ? 'danger-text' : 'emerald-text'}>{offline ? 'Offline' : 'Online'}</span></td>
              <td><span className={`badge ${node.category === 'Gateway' ? 'badge-gateway' : 'badge-sensor'}`}>{node.category === 'Gateway' ? 'Gateway' : 'Sensor'}</span></td>
              <td className="silver-text">{node.sensor_id}</td>
              <td><Battery size={16} /> {battery === undefined ? '—' : `${battery.toFixed(2)} V`}</td>
              <td><Signal size={16} /> {rssi === undefined ? '—' : `${rssi.toFixed(0)} dBm`}</td>
              <td className="silver-text">{node.receivedAt.toLocaleString()}</td>
            </tr>;
          })}
          {nodes.length === 0 && <tr><td colSpan={7} className="empty-cell">No nodes discovered yet.</td></tr>}
        </tbody>
      </table></div>
    </section>
  );
}
