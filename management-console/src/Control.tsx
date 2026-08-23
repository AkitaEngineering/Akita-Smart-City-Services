import { useMemo, useState } from 'react';
import { SlidersHorizontal } from 'lucide-react';
import type { ConnectionState, PublishCommand, TelemetryMessage } from './types';
import useNow from './useNow';

interface ControlProps {
  messages: TelemetryMessage[];
  connection: ConnectionState;
  publishCommand: PublishCommand;
}

interface ControllableAsset {
  id: string;
  type: string;
}

const ACTIONS: Record<string, { action: string; label: string; kind: 'boolean' | 'percentage' }[]> = {
  street_light: [
    { action: 'power', label: 'Power', kind: 'boolean' },
    { action: 'brightness', label: 'Brightness', kind: 'percentage' },
  ],
  pool_system: [{ action: 'pump', label: 'Filtration pump', kind: 'boolean' }],
  park_monitor: [{ action: 'sprinklers', label: 'Sprinklers', kind: 'boolean' }],
};

const ASSET_LABELS: Record<string, string> = {
  street_light: 'Street Light',
  pool_system: 'Pool System',
  park_monitor: 'Park Monitor',
};

export default function Control({ messages, connection, publishCommand }: ControlProps) {
  const now = useNow();
  const assets = useMemo(() => {
    const discovered = new Map<string, ControllableAsset>();
    messages.filter((message) => message.category === 'Infrastructure').forEach((message) => {
      if (now - message.receivedAt.getTime() <= 15 * 60 * 1000 && ACTIONS[message.sensor_id]) {
        discovered.set(message.node_id, { id: message.node_id, type: message.sensor_id });
      }
    });
    return [...discovered.values()].sort((left, right) => left.id.localeCompare(right.id));
  }, [messages, now]);

  const [assetId, setAssetId] = useState('');
  const [action, setAction] = useState('');
  const [booleanValue, setBooleanValue] = useState('true');
  const [percentage, setPercentage] = useState(80);
  const [submitting, setSubmitting] = useState(false);
  const [status, setStatus] = useState('');
  const [error, setError] = useState('');

  const selectedAsset = assets.find((asset) => asset.id === assetId) ?? assets[0];
  const availableActions = selectedAsset ? ACTIONS[selectedAsset.type] : [];
  const selectedAction = availableActions.find((item) => item.action === action) ?? availableActions[0];

  const submit = async (event: React.FormEvent) => {
    event.preventDefault();
    if (!selectedAsset || !selectedAction) return;
    const displayValue = selectedAction.kind === 'boolean' ? booleanValue === 'true' : percentage;
    if (!window.confirm(`Send ${selectedAction.label}=${String(displayValue)} to ${selectedAsset.id}?`)) return;

    setSubmitting(true);
    setError('');
    setStatus('');
    try {
      await publishCommand({ assetId: selectedAsset.id, action: selectedAction.action, value: displayValue });
      setStatus('The target device reported that the command executed successfully.');
    } catch (submissionError) {
      const detail = submissionError instanceof Error ? submissionError.message : 'Unknown command failure';
      setError(`The command did not complete successfully: ${detail}. Verify the physical device state before retrying.`);
    } finally {
      setSubmitting(false);
    }
  };

  return (
    <section>
      <h3 className="section-heading"><SlidersHorizontal className="emerald-text" /> Command &amp; Control (C2)</h3>
      <div className="glass-panel form-panel">
        {assets.length === 0 ? (
          <p className="silver-text">No controllable infrastructure has reported telemetry.</p>
        ) : (
          <form onSubmit={submit} className="control-form">
            <label>Asset
              <select value={selectedAsset?.id ?? ''} onChange={(event) => { setAssetId(event.target.value); setAction(''); }}>
                {assets.map((asset) => <option key={asset.id} value={asset.id}>{asset.id} — {ASSET_LABELS[asset.type] ?? asset.type}</option>)}
              </select>
            </label>
            <label>Action
              <select value={selectedAction?.action ?? ''} onChange={(event) => setAction(event.target.value)}>
                {availableActions.map((item) => <option key={item.action} value={item.action}>{item.label}</option>)}
              </select>
            </label>
            {selectedAction?.kind === 'boolean' ? (
              <label>Requested state
                <select value={booleanValue} onChange={(event) => setBooleanValue(event.target.value)}>
                  <option value="true">On</option><option value="false">Off</option>
                </select>
              </label>
            ) : (
              <label>Requested level: {percentage}%
                <input type="range" min="0" max="100" value={percentage} onChange={(event) => setPercentage(Number(event.target.value))} />
              </label>
            )}
            <button className="primary-button" type="submit" disabled={submitting || connection !== 'connected'}>
              {submitting ? 'Publishing…' : 'Review and publish command'}
            </button>
          </form>
        )}
        {connection !== 'connected' && <p className="status-error" role="alert">Commands are disabled until MQTT is connected.</p>}
        {error && <p className="status-error" role="alert">{error}</p>}
        {status && <p className="status-success" role="status">{status}</p>}
      </div>
    </section>
  );
}
