import { useEffect, useMemo, useState } from 'react';
import { Building2, Plus } from 'lucide-react';
import { collection, doc, getDoc, onSnapshot, setDoc } from 'firebase/firestore';
import { db } from './firebase';
import { booleanReading, numericReading } from './telemetry';
import type { AssetRecord, TelemetryMessage } from './types';
import useNow from './useNow';

const ASSET_ID = /^[0-9A-Fa-f]{8}$/;
const ASSET_TYPES = ['Street Light', 'Park Monitor', 'Pool System', 'People Counter'];

function describeStatus(message: TelemetryMessage | undefined, now: number): string {
  if (!message) return 'Awaiting telemetry';
  if (now - message.receivedAt.getTime() > 15 * 60 * 1000) return `Telemetry stale since ${message.receivedAt.toLocaleString()}`;
  if (message.sensor_id === 'street_light') return `Light: ${booleanReading(message, 'on') === true ? 'ON' : 'OFF'} (${numericReading(message, 'brightness') ?? 0}%)`;
  if (message.sensor_id === 'park_monitor') return `People: ${numericReading(message, 'people_count') ?? '—'} · Sprinklers: ${booleanReading(message, 'sprinklers') ? 'ON' : 'OFF'}`;
  if (message.sensor_id === 'pool_system') return `Pump: ${booleanReading(message, 'pump_active') ? 'ON' : 'OFF'} · ${numericReading(message, 'temp_c') ?? '—'}°C`;
  return `Last report: ${message.receivedAt.toLocaleString()}`;
}

export default function Infrastructure({ messages }: { messages: TelemetryMessage[] }) {
  const now = useNow();
  const [assets, setAssets] = useState<AssetRecord[]>([]);
  const [showForm, setShowForm] = useState(false);
  const [assetId, setAssetId] = useState('');
  const [assetType, setAssetType] = useState(ASSET_TYPES[0]);
  const [latitude, setLatitude] = useState('');
  const [longitude, setLongitude] = useState('');
  const [error, setError] = useState('');
  const [saving, setSaving] = useState(false);

  useEffect(() => onSnapshot(collection(db, 'assets'), (snapshot) => {
    const records = snapshot.docs.flatMap((item) => {
      const data = item.data();
      return typeof data.assetId === 'string' && typeof data.type === 'string' && typeof data.latitude === 'number' && typeof data.longitude === 'number'
        ? [{ id: data.assetId, type: data.type, latitude: data.latitude, longitude: data.longitude }]
        : [];
    });
    setAssets(records.sort((left, right) => left.id.localeCompare(right.id)));
  }, () => setError('Unable to load the infrastructure registry.')), []);

  const latestByAsset = useMemo(() => {
    const latest = new Map<string, TelemetryMessage>();
    messages.filter((message) => message.category === 'Infrastructure').forEach((message) => latest.set(message.node_id, message));
    return latest;
  }, [messages]);

  const saveAsset = async (event: React.FormEvent) => {
    event.preventDefault();
    const normalizedAssetId = assetId.toLowerCase();
    const lat = Number(latitude);
    const lng = Number(longitude);
    if (!ASSET_ID.test(normalizedAssetId)) return setError('Asset ID must be the device’s eight-digit hexadecimal mesh node ID.');
    if (!Number.isFinite(lat) || lat < -90 || lat > 90 || !Number.isFinite(lng) || lng < -180 || lng > 180) return setError('Enter valid latitude and longitude values.');
    setSaving(true);
    setError('');
    try {
      const assetRef = doc(db, 'assets', normalizedAssetId);
      if ((await getDoc(assetRef)).exists()) throw new Error('duplicate');
      await setDoc(assetRef, { assetId: normalizedAssetId, type: assetType, latitude: lat, longitude: lng, createdAt: new Date().toISOString() });
      setAssetId(''); setLatitude(''); setLongitude(''); setShowForm(false);
    } catch (saveError) {
      setError(saveError instanceof Error && saveError.message === 'duplicate' ? 'An asset with this ID already exists.' : 'The asset could not be registered.');
    } finally {
      setSaving(false);
    }
  };

  return (
    <section>
      <div className="section-toolbar">
        <h3 className="section-heading"><Building2 className="emerald-text" /> Municipal Infrastructure Registry</h3>
        <button className="primary-button" onClick={() => setShowForm((visible) => !visible)}><Plus size={18} /> Register asset</button>
      </div>
      {showForm && (
        <form className="glass-panel form-panel asset-form" onSubmit={saveAsset}>
          <label>Asset ID<input required value={assetId} onChange={(event) => setAssetId(event.target.value.trim())} /></label>
          <label>Category<select value={assetType} onChange={(event) => setAssetType(event.target.value)}>{ASSET_TYPES.map((type) => <option key={type}>{type}</option>)}</select></label>
          <label>Latitude<input required inputMode="decimal" value={latitude} onChange={(event) => setLatitude(event.target.value)} /></label>
          <label>Longitude<input required inputMode="decimal" value={longitude} onChange={(event) => setLongitude(event.target.value)} /></label>
          <button className="primary-button" type="submit" disabled={saving}>{saving ? 'Saving…' : 'Save asset'}</button>
        </form>
      )}
      {error && <p className="status-error" role="alert">{error}</p>}
      <div className="node-table-container glass-panel table-panel">
        <table><thead><tr><th>Asset ID</th><th>Category</th><th>Location</th><th>Live status</th></tr></thead>
          <tbody>
            {assets.map((asset) => <tr key={asset.id}><td className="mono">{asset.id}</td><td>{asset.type}</td><td className="silver-text">{asset.latitude.toFixed(5)}, {asset.longitude.toFixed(5)}</td><td className="emerald-text">{describeStatus(latestByAsset.get(asset.id.toLowerCase()), now)}</td></tr>)}
            {assets.length === 0 && <tr><td colSpan={4} className="empty-cell">No registered infrastructure assets.</td></tr>}
          </tbody>
        </table>
      </div>
    </section>
  );
}
