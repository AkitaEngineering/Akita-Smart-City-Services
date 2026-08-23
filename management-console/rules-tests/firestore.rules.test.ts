import { afterAll, beforeAll, beforeEach, describe, it } from 'vitest';
import { assertFails, assertSucceeds, initializeTestEnvironment, type RulesTestEnvironment } from '@firebase/rules-unit-testing';
import { doc, getDoc, serverTimestamp, setDoc, updateDoc } from 'firebase/firestore';
import rules from '../../firestore.rules?raw';

const processEnvironment = (globalThis as typeof globalThis & {
  process?: { env?: Record<string, string | undefined> };
}).process?.env;
const emulatorHost = processEnvironment?.FIRESTORE_EMULATOR_HOST;
if (!emulatorHost) {
  throw new Error('FIRESTORE_EMULATOR_HOST is required for Firestore Rules tests');
}

let environment: RulesTestEnvironment;

function commandData(requestedBy: string, overrides: Record<string, unknown> = {}) {
  return {
    assetId: 'a1b2c3d4',
    action: 'power',
    value: true,
    commandId: '00000000-0000-4000-8000-000000000000',
    requestedAt: serverTimestamp(),
    expiresAtUtc: Math.floor(Date.now() / 1000) + 60,
    requestedBy,
    status: 'pending',
    ...overrides,
  };
}

describe('Firestore production rules', () => {
  beforeAll(async () => {
    const [host, portText] = emulatorHost.split(':');
    environment = await initializeTestEnvironment({
      projectId: 'demo-ascs',
      firestore: { host, port: Number(portText), rules },
    });
  });

  beforeEach(async () => environment.clearFirestore());
  afterAll(async () => environment.cleanup());

  it('allows bounded operator commands and rejects unauthorized or unsafe requests', async () => {
    const operator = environment.authenticatedContext('operator-1', { role: 'operator' }).firestore();
    const viewer = environment.authenticatedContext('viewer-1', { role: 'viewer' }).firestore();
    const commandPath = 'commands/00000000-0000-4000-8000-000000000000';

    await assertSucceeds(setDoc(doc(operator, commandPath), commandData('operator-1')));
    await environment.clearFirestore();
    await assertFails(setDoc(doc(viewer, commandPath), commandData('viewer-1')));
    await assertFails(setDoc(doc(operator, commandPath), commandData('operator-1', {
      expiresAtUtc: Math.floor(Date.now() / 1000) + 600,
    })));
    await assertFails(setDoc(doc(operator, commandPath), commandData('operator-1', { value: Number.NaN })));
  });

  it('allows only the requesting operator to complete an immutable command audit', async () => {
    const owner = environment.authenticatedContext('operator-1', { role: 'operator' }).firestore();
    const other = environment.authenticatedContext('operator-2', { role: 'operator' }).firestore();
    const commandPath = 'commands/00000000-0000-4000-8000-000000000000';
    await assertSucceeds(setDoc(doc(owner, commandPath), commandData('operator-1')));
    await assertFails(updateDoc(doc(other, commandPath), {
      status: 'executed', detail: 'forged', completedAt: serverTimestamp(),
    }));
    await assertSucceeds(updateDoc(doc(owner, commandPath), {
      status: 'executed', detail: 'Device reported success', completedAt: serverTimestamp(),
    }));
    await assertFails(updateDoc(doc(owner, commandPath), { status: 'failed' }));
  });

  it('denies client telemetry writes while allowing operator reads', async () => {
    const operator = environment.authenticatedContext('operator-1', { role: 'operator' }).firestore();
    const telemetryPath = 'telemetry/sample';
    const telemetry = { node_id: 'a1b2c3d4', sensor_id: 'BME280', readings: { temperature_c: 20 } };
    await assertFails(setDoc(doc(operator, telemetryPath), telemetry));
    await environment.withSecurityRulesDisabled(async (context) => {
      await setDoc(doc(context.firestore(), telemetryPath), telemetry);
    });
    await assertSucceeds(getDoc(doc(operator, telemetryPath)));
  });

  it('requires server timestamps for new assets', async () => {
    const operator = environment.authenticatedContext('operator-1', { role: 'operator' }).firestore();
    const asset = { assetId: 'a1b2c3d4', type: 'Street Light', latitude: 39.72, longitude: 140.1 };
    await assertSucceeds(setDoc(doc(operator, 'assets/a1b2c3d4'), { ...asset, createdAt: serverTimestamp() }));
    await environment.clearFirestore();
    await assertFails(setDoc(doc(operator, 'assets/a1b2c3d4'), { ...asset, createdAt: new Date().toISOString() }));
  });
});
