import { describe, expect, it } from 'vitest';
import { buildCommandTopic, commandIdFromAcknowledgementTopic, createCommandEnvelope, parseControlAcknowledgement } from './commands';

describe('command safety', () => {
  it('builds a bounded command envelope and topic', () => {
    const envelope = createCommandEnvelope({ assetId: 'a1b2c3d4', action: 'power', value: true }, 'operator', '00000000-0000-4000-8000-000000000000', new Date(0));
    expect(buildCommandTopic('akita/smartcity', envelope.assetId)).toBe('akita/smartcity/control/a1b2c3d4');
    expect(envelope.requestedAt).toBe('1970-01-01T00:00:00.000Z');
    expect(envelope.expiresAtUtc).toBe(60);
  });

  it('rejects wildcard and path injection', () => {
    expect(() => buildCommandTopic('akita/smartcity', '../#')).toThrow();
    expect(() => createCommandEnvelope({ assetId: 'a1b2c3d4', action: 'bad/action', value: true }, 'operator', '00000000-0000-4000-8000-000000000000')).toThrow();
    expect(() => createCommandEnvelope({ assetId: 'a1b2c3d4', action: 'power', value: 'true' } as never, 'operator', '00000000-0000-4000-8000-000000000000')).toThrow();
    expect(() => createCommandEnvelope({ assetId: 'A1B2C3D4', action: 'power', value: true }, 'operator', '00000000-0000-4000-8000-000000000000')).toThrow();
    expect(() => createCommandEnvelope({ assetId: 'a1b2c3d4', action: 'power', value: 1_000_000_001 }, 'operator', '00000000-0000-4000-8000-000000000000')).toThrow();
    expect(() => createCommandEnvelope({ assetId: 'a1b2c3d4', action: 'power', value: true }, 'bad\nuid', '00000000-0000-4000-8000-000000000000')).toThrow();
  });

  it('validates execution acknowledgements', () => {
    const bytes = new TextEncoder().encode(JSON.stringify({ commandId: '00000000-0000-4000-8000-000000000000', nodeId: 'a1b2c3d4', status: 'executed', detail: 'ok' }));
    expect(parseControlAcknowledgement(bytes)?.status).toBe('executed');
    expect(parseControlAcknowledgement(new TextEncoder().encode('{}'))).toBeNull();
    expect(parseControlAcknowledgement(new TextEncoder().encode(JSON.stringify({
      commandId: '00000000-0000-4000-8000-000000000000', nodeId: 'a1b2c3d4', status: 'executed', detail: 'ok', unexpected: true,
    })))).toBeNull();
  });

  it('accepts acknowledgements only on their exact command topic', () => {
    const commandId = '00000000-0000-4000-8000-000000000000';
    expect(commandIdFromAcknowledgementTopic('akita/smartcity', `akita/smartcity/control/ack/${commandId}`)).toBe(commandId);
    expect(commandIdFromAcknowledgementTopic('akita/smartcity', `akita/smartcity/control/ack/${commandId}/extra`)).toBeNull();
    expect(commandIdFromAcknowledgementTopic('akita/smartcity', `other/control/ack/${commandId}`)).toBeNull();
  });
});
