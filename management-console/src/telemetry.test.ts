import { describe, expect, it } from 'vitest';
import { mergeTelemetry, parseMqttMessage, parseStoredTelemetry, parseTelemetryPayload } from './telemetry';

describe('telemetry validation', () => {
  it('accepts a valid gateway payload', () => {
    const message = parseTelemetryPayload('akita/smartcity/sensor/1/node', {
      node_id: 'a1b2c3d4', sensor_id: 'BME280-1', timestamp_utc: 123, sequence_num: 4,
      readings: { temperature_c: 21.5, pump_active: 1 },
    });
    expect(message?.category).toBe('Environment');
    expect(message?.readings.temperature_c).toBe(21.5);
  });

  it('rejects malformed, non-finite, and oversized payloads', () => {
    expect(parseTelemetryPayload('topic', { node_id: 'a1b2c3d4', sensor_id: 's', readings: { value: Infinity } })).toBeNull();
    expect(parseTelemetryPayload('topic', { node_id: 'not-a-node', sensor_id: 's', readings: { value: 1 } })).toBeNull();
    expect(parseTelemetryPayload('topic', { node_id: 'a1b2c3d4', sensor_id: 'bad/topic', readings: { value: 1 } })).toBeNull();
    expect(parseTelemetryPayload('topic', { node_id: 'a1b2c3d4', sensor_id: 's', readings: {} })).toBeNull();
    expect(parseMqttMessage('topic', new Uint8Array(65 * 1024))).toBeNull();
    expect(parseMqttMessage('topic', new TextEncoder().encode('{bad json'))).toBeNull();
  });

  it('deduplicates and keeps the newest bounded history', () => {
    const base = parseTelemetryPayload('topic', { node_id: 'a1b2c3d4', sensor_id: 's', sequence_num: 1, readings: { value: 1 } }, new Date(1));
    const newer = parseTelemetryPayload('topic', { node_id: 'a1b2c3d4', sensor_id: 's', sequence_num: 2, readings: { value: 2 } }, new Date(2));
    expect(mergeTelemetry(base ? [base] : [], [base, newer].filter((item) => item !== null), 1)).toEqual([newer]);
  });

  it('classifies canonical infrastructure identifiers', () => {
    const message = parseTelemetryPayload('akita/smartcity/sensor/1/a1b2c3d4/street_light', {
      node_id: 'a1b2c3d4', sensor_id: 'street_light', readings: { on: 1 },
    });
    expect(message?.category).toBe('Infrastructure');
  });

  it('requires live payload identity to match the exact gateway topic', () => {
    const payload = new TextEncoder().encode(JSON.stringify({
      node_id: 'a1b2c3d4', sensor_id: 'street_light', timestamp_utc: 1, sequence_num: 2, readings: { on: 1 },
    }));
    expect(parseMqttMessage('akita/smartcity/sensor/1/a1b2c3d4/street_light', payload)?.node_id).toBe('a1b2c3d4');
    expect(parseMqttMessage('akita/smartcity/sensor/1/deadbeef/street_light', payload)).toBeNull();
    expect(parseMqttMessage('akita/smartcity/sensor/0/a1b2c3d4/street_light', payload)).toBeNull();
    expect(parseMqttMessage('akita/smartcity/sensor/1/a1b2c3d4/street_light/extra', payload)).toBeNull();
  });

  it('requires the complete live gateway schema without extensions', () => {
    const encode = (value: unknown) => new TextEncoder().encode(JSON.stringify(value));
    const base = { node_id: 'a1b2c3d4', sensor_id: 's', timestamp_utc: 1, sequence_num: 2, readings: { value: 1 } };
    expect(parseMqttMessage('akita/smartcity/sensor/1/a1b2c3d4/s', encode(base))).not.toBeNull();
    expect(parseMqttMessage('akita/smartcity/sensor/1/a1b2c3d4/s', encode({ ...base, sequence_num: undefined }))).toBeNull();
    expect(parseMqttMessage('akita/smartcity/sensor/1/a1b2c3d4/s', encode({ ...base, extra: true }))).toBeNull();
  });

  it('accepts only timestamped, topic-bound Firestore history and deduplicates it with live data', () => {
    const receivedAt = { toDate: () => new Date(2_000) };
    const stored = parseStoredTelemetry({
      topic: 'akita/smartcity/sensor/1/a1b2c3d4/s', node_id: 'a1b2c3d4', sensor_id: 's',
      timestamp_utc: 1, sequence_num: 2, readings: { value: 1 }, receivedAt,
    }, 'firestore-document', new Date(3_000));
    const live = parseMqttMessage('akita/smartcity/sensor/1/a1b2c3d4/s', new TextEncoder().encode(JSON.stringify({
      node_id: 'a1b2c3d4', sensor_id: 's', timestamp_utc: 1, sequence_num: 2, readings: { value: 1 },
    })), new Date(1_000));
    expect(stored?.id).toBe(live?.id);
    expect(mergeTelemetry(live ? [live] : [], stored ? [stored] : [])).toHaveLength(1);
    expect(parseStoredTelemetry({ node_id: 'a1b2c3d4', sensor_id: 's', readings: { value: 1 } }, 'bad')).toBeNull();
    expect(parseStoredTelemetry({
      topic: 'akita/smartcity/sensor/1/a1b2c3d4/s', node_id: 'a1b2c3d4', sensor_id: 's',
      timestamp_utc: 1, sequence_num: 2, readings: { value: 1 }, receivedAt, unexpected: true,
    }, 'extended', new Date(3_000))).toBeNull();
  });
});
