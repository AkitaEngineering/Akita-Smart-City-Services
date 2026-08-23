import { defineConfig, loadEnv } from 'vite'
import react from '@vitejs/plugin-react'

const REQUIRED_ENVIRONMENT = [
  'VITE_MQTT_BROKER_URL', 'VITE_MQTT_BASE_TOPIC', 'VITE_FIREBASE_API_KEY', 'VITE_FIREBASE_AUTH_DOMAIN',
  'VITE_FIREBASE_PROJECT_ID', 'VITE_FIREBASE_STORAGE_BUCKET', 'VITE_FIREBASE_MESSAGING_SENDER_ID', 'VITE_FIREBASE_APP_ID',
];

export default defineConfig(({ command, mode }) => {
  const environment = loadEnv(mode, process.cwd(), '');
  if (command === 'build') {
    const missing = REQUIRED_ENVIRONMENT.filter((name) => !environment[name]?.trim());
    if (missing.length > 0) throw new Error(`Missing required build environment: ${missing.join(', ')}`);
    const brokerUrl = new URL(environment.VITE_MQTT_BROKER_URL);
    if (!['ws:', 'wss:'].includes(brokerUrl.protocol) || !brokerUrl.hostname || brokerUrl.username || brokerUrl.password) {
      throw new Error('VITE_MQTT_BROKER_URL must be a WebSocket URL without embedded credentials');
    }
    if (mode === 'production' && brokerUrl.protocol !== 'wss:') {
      throw new Error('Production VITE_MQTT_BROKER_URL must use wss://');
    }
    const baseTopic = environment.VITE_MQTT_BASE_TOPIC;
    if (baseTopic.length > 128 || baseTopic.startsWith('/') || baseTopic.endsWith('/') || baseTopic.includes('//') ||
        /[#+\u0000-\u0020\u007f]/.test(baseTopic)) {
      throw new Error('VITE_MQTT_BASE_TOPIC is invalid');
    }
  }

  return {
    plugins: [react()],
    build: {
      rolldownOptions: {
        output: {
          manualChunks(id: string) {
            if (id.includes('/node_modules/@firebase/firestore')) return 'firebase-firestore';
            if (id.includes('/node_modules/@firebase/auth')) return 'firebase-auth';
            if (id.includes('/node_modules/firebase/') || id.includes('/node_modules/@firebase/')) return 'firebase-core';
            if (id.includes('/node_modules/mqtt/') || id.includes('/node_modules/mqtt-packet/')) return 'mqtt';
            if (id.includes('/node_modules/react/') || id.includes('/node_modules/react-dom/') || id.includes('/node_modules/react-router')) return 'react';
            return undefined;
          },
        },
      },
    },
  };
});
