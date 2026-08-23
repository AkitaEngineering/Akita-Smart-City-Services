export interface RuntimeConfig {
  mqttUrl: string;
  mqttBaseTopic: string;
}

function required(name: keyof ImportMetaEnv): string {
  const value = import.meta.env[name]?.trim();
  if (!value) throw new Error(`Missing required environment variable ${name}`);
  return value;
}

function validateBaseTopic(topic: string): string {
  const value = topic.replace(/^\/+|\/+$/g, '');
  if (value !== topic || !value || value.length > 128 || value.includes('#') || value.includes('+') ||
      value.includes('//') || /[\u0000-\u0020\u007f]/.test(value)) {
    throw new Error('VITE_MQTT_BASE_TOPIC must be a concrete MQTT topic without wildcards');
  }
  return value;
}

function validateBrokerUrl(rawUrl: string): string {
  const url = new URL(rawUrl);
  if (!['ws:', 'wss:'].includes(url.protocol)) {
    throw new Error('VITE_MQTT_BROKER_URL must use ws:// or wss://');
  }
  if (!url.hostname || url.username || url.password) {
    throw new Error('VITE_MQTT_BROKER_URL must not contain embedded credentials');
  }
  if (import.meta.env.PROD && url.protocol !== 'wss:') {
    throw new Error('Production MQTT connections must use wss://');
  }
  return url.toString();
}

export function getRuntimeConfig(): RuntimeConfig {
  return {
    mqttUrl: validateBrokerUrl(required('VITE_MQTT_BROKER_URL')),
    mqttBaseTopic: validateBaseTopic(required('VITE_MQTT_BASE_TOPIC')),
  };
}
