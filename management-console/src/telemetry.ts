import type { ReadingValue, TelemetryCategory, TelemetryMessage } from './types';

const MAX_ID_LENGTH = 128;
const MAX_READING_KEYS = 64;
const NODE_ID = /^[0-9A-Fa-f]{8}$/;
const IDENTIFIER = /^[A-Za-z0-9_.-]{1,64}$/;
const INFRASTRUCTURE_SENSOR_IDS = new Set(['street_light', 'park_monitor', 'pool_system', 'people_counter']);

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === 'object' && value !== null && !Array.isArray(value);
}

function boundedString(value: unknown, fallback = ''): string {
  return typeof value === 'string' && value.length <= MAX_ID_LENGTH ? value : fallback;
}

function unsignedInteger(value: unknown): number | undefined {
  return typeof value === 'number' && Number.isInteger(value) && value >= 0 && value <= 0xFFFFFFFF ? value : undefined;
}

function parseCategory(value: unknown, topic: string, sensorId: string): TelemetryCategory {
  if (value === 'Environment' || value === 'Fleet' || value === 'Infrastructure' || value === 'Gateway') return value;
  const normalizedSensorId = sensorId.toLowerCase();
  if (INFRASTRUCTURE_SENSOR_IDS.has(normalizedSensorId) || normalizedSensorId.startsWith('infra.')) return 'Infrastructure';
  if (normalizedSensorId.startsWith('fleet.')) return 'Fleet';
  if (normalizedSensorId === 'gateway' || normalizedSensorId.startsWith('gateway.')) return 'Gateway';
  if (topic.includes('/fleet/')) return 'Fleet';
  if (topic.includes('/infra/')) return 'Infrastructure';
  if (topic.includes('/gateway/')) return 'Gateway';
  if (topic.includes('/sensor/')) return 'Environment';
  return 'Unknown';
}

function parseReadings(value: unknown): Record<string, ReadingValue> | null {
  if (!isRecord(value)) return null;
  const entries = Object.entries(value);
  if (entries.length === 0 || entries.length > MAX_READING_KEYS) return null;

  const readings: Record<string, ReadingValue> = {};
  for (const [key, reading] of entries) {
    if (!IDENTIFIER.test(key) || typeof reading !== 'number' || !Number.isFinite(reading)) return null;
    readings[key] = reading;
  }
  return readings;
}

function parseDate(value: unknown, fallback: Date): Date {
  if (value instanceof Date && Number.isFinite(value.getTime())) return value;
  if (typeof value === 'string' || typeof value === 'number') {
    const parsed = new Date(value);
    if (Number.isFinite(parsed.getTime())) return parsed;
  }
  if (isRecord(value) && typeof value.toDate === 'function') {
    const parsed = value.toDate();
    if (parsed instanceof Date && Number.isFinite(parsed.getTime())) return parsed;
  }
  return fallback;
}

export function parseTelemetryPayload(topic: string, payload: unknown, receivedAt = new Date(), id = ''): TelemetryMessage | null {
  if (!isRecord(payload) || topic.length > 512) return null;
  const nodeId = boundedString(payload.node_id);
  const sensorId = boundedString(payload.sensor_id);
  const readings = parseReadings(payload.readings);
  if (!NODE_ID.test(nodeId) || !IDENTIFIER.test(sensorId) || readings === null) return null;

  const timestamp = unsignedInteger(payload.timestamp_utc);
  const sequence = unsignedInteger(payload.sequence_num);
  if (payload.timestamp_utc !== undefined && timestamp === undefined) return null;
  if (payload.sequence_num !== undefined && sequence === undefined) return null;
  return {
    id: id || `${nodeId}:${sensorId}:${sequence ?? timestamp ?? receivedAt.getTime()}`,
    topic,
    node_id: nodeId,
    sensor_id: sensorId,
    category: parseCategory(payload.category, topic, sensorId),
    timestamp_utc: timestamp,
    sequence_num: sequence,
    readings,
    receivedAt: parseDate(payload.receivedAt, receivedAt),
  };
}

export function parseMqttMessage(topic: string, bytes: Uint8Array, receivedAt = new Date()): TelemetryMessage | null {
  if (bytes.byteLength > 64 * 1024) return null;
  try {
    const message = parseTelemetryPayload(topic, JSON.parse(new TextDecoder().decode(bytes)), receivedAt);
    if (!message) return null;
    const marker = '/sensor/';
    const markerIndex = topic.lastIndexOf(marker);
    if (markerIndex <= 0) return null;
    const parts = topic.slice(markerIndex + marker.length).split('/');
    if (parts.length !== 3 || !/^[1-9][0-9]{0,9}$/.test(parts[0])) return null;
    const serviceId = Number(parts[0]);
    if (!Number.isInteger(serviceId) || serviceId > 0xFFFFFFFF ||
        parts[1].toLowerCase() !== message.node_id.toLowerCase() || parts[2] !== message.sensor_id) return null;
    return message;
  } catch {
    return null;
  }
}

export function mergeTelemetry(current: TelemetryMessage[], incoming: TelemetryMessage[], maximum = 500): TelemetryMessage[] {
  const byId = new Map(current.map((message) => [message.id, message]));
  incoming.forEach((message) => byId.set(message.id, message));
  return [...byId.values()]
    .sort((left, right) => left.receivedAt.getTime() - right.receivedAt.getTime())
    .slice(-maximum);
}

export function numericReading(message: TelemetryMessage, key: string): number | undefined {
  const value = message.readings[key];
  return typeof value === 'number' && Number.isFinite(value) ? value : undefined;
}

export function booleanReading(message: TelemetryMessage, key: string): boolean | undefined {
  const value = message.readings[key];
  if (value === 0 || value === 1) return Boolean(value);
  return undefined;
}
