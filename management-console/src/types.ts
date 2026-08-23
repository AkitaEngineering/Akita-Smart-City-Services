export type TelemetryCategory = 'Environment' | 'Fleet' | 'Infrastructure' | 'Gateway' | 'Unknown';
export type ReadingValue = number;

export interface TelemetryMessage {
  id: string;
  topic: string;
  node_id: string;
  sensor_id: string;
  category: TelemetryCategory;
  timestamp_utc?: number;
  sequence_num?: number;
  readings: Record<string, ReadingValue>;
  receivedAt: Date;
}

export interface AssetRecord {
  id: string;
  type: string;
  latitude: number;
  longitude: number;
}

export type ConnectionState = 'disconnected' | 'connecting' | 'connected' | 'error';

export interface CommandRequest {
  assetId: string;
  action: string;
  value: number | boolean;
}

export type PublishCommand = (request: CommandRequest) => Promise<void>;
