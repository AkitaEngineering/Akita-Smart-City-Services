import type { CommandRequest } from './types';

const NODE_ID = /^[0-9a-f]{8}$/;
const ACTION = /^[A-Za-z][A-Za-z0-9_-]{0,31}$/;
const UUID = /^[0-9A-Fa-f]{8}-[0-9A-Fa-f]{4}-[1-8][0-9A-Fa-f]{3}-[89ABab][0-9A-Fa-f]{3}-[0-9A-Fa-f]{12}$/;
const MAX_NUMERIC_COMMAND = 1_000_000_000;

export interface CommandEnvelope extends CommandRequest {
  commandId: string;
  requestedAt: string;
  expiresAtUtc: number;
  requestedBy: string;
}

export function buildCommandTopic(baseTopic: string, assetId: string): string {
  if (!NODE_ID.test(assetId)) throw new Error('Controllable asset IDs must be eight-digit mesh node IDs');
  return `${baseTopic}/control/${assetId}`;
}

export function commandIdFromAcknowledgementTopic(baseTopic: string, topic: string): string | null {
  const prefix = `${baseTopic}/control/ack/`;
  if (!topic.startsWith(prefix)) return null;
  const commandId = topic.slice(prefix.length);
  return UUID.test(commandId) ? commandId : null;
}

export function createCommandEnvelope(request: CommandRequest, requestedBy: string, commandId: string, now = new Date()): CommandEnvelope {
  if (!NODE_ID.test(request.assetId)) throw new Error('Controllable asset IDs must be eight-digit mesh node IDs');
  if (!ACTION.test(request.action)) throw new Error('Command action is invalid');
  if (!requestedBy || requestedBy.length > 128 || /[\u0000-\u001f\u007f]/.test(requestedBy) || !UUID.test(commandId)) {
    throw new Error('Command identity is invalid');
  }
  if (typeof request.value !== 'boolean' && (typeof request.value !== 'number' || !Number.isFinite(request.value) ||
      Math.abs(request.value) > MAX_NUMERIC_COMMAND)) {
    throw new Error('Command value must be a bounded finite number or boolean');
  }
  return { ...request, commandId, requestedAt: now.toISOString(), expiresAtUtc: Math.floor(now.getTime() / 1000) + 60, requestedBy };
}

export interface ControlAcknowledgement {
  commandId: string;
  nodeId: string;
  status: 'executed' | 'failed' | 'rejected';
  detail: string;
}

export function parseControlAcknowledgement(bytes: Uint8Array): ControlAcknowledgement | null {
  if (bytes.byteLength > 2048) return null;
  try {
    const value: unknown = JSON.parse(new TextDecoder().decode(bytes));
    if (typeof value !== 'object' || value === null) return null;
    const record = value as Record<string, unknown>;
    const keys = Object.keys(record);
    if (keys.length !== 4 || !keys.every((key) => ['commandId', 'nodeId', 'status', 'detail'].includes(key))) return null;
    if (typeof record.commandId !== 'string' || !UUID.test(record.commandId)) return null;
    if (typeof record.nodeId !== 'string' || !NODE_ID.test(record.nodeId)) return null;
    if (record.status !== 'executed' && record.status !== 'failed' && record.status !== 'rejected') return null;
    if (typeof record.detail !== 'string' || record.detail.length > 96) return null;
    return { commandId: record.commandId, nodeId: record.nodeId, status: record.status, detail: record.detail };
  } catch {
    return null;
  }
}
