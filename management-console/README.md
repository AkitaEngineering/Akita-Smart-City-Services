# ASCS Management Console (Unified Operations Center)

The ASCS Management Console is the frontend UI designed to monitor and manage the Akita Smart City Services network. It provides a Unified Operations Center experience, bridging real-world Meshtastic hardware telemetry with web-based Command & Control (C2).

## Features

1. **Global Geospatial Map (God's Eye View):** Real-time mapping of all city assets.
    - 🟢 **Environment Nodes** (Sensors)
    - 🔴 **Anomalies** (Threshold breaches like high AQI)
    - 🟡 **Fleet Ops** (Moving municipal vehicles)
    - 🔵 **Infrastructure** (Static municipal assets)
2. **Active Incidents Feed:** Auto-generates alerts for environmental hazards and node diagnostics (e.g., low battery, gateway offline).
3. **Fleet Operations:** Live tracking of municipal fleets (Buses, Garbage Trucks, Snow Plows) with coordinate interpolation and speed metrics.
4. **Command & Control (C2):** Two-way MQTT interfaces to remotely toggle physical infrastructure (Street lights, Pool Pumps) complete with safety confirmation dialogs.
5. **Hardware Diagnostics:** Displays deep mesh telemetry including RSSI (Signal Strength) and battery voltage for deployed physical nodes.

## Technology Stack

- **Framework:** React 18 (Vite)
- **Language:** TypeScript
- **Styling:** Custom CSS (Glassmorphism, Dark Theme)
- **Mapping:** Leaflet & React-Leaflet
- **Charting:** Recharts
- **Icons:** Lucide-React
- **Networking:** MQTT.js over WebSockets

## Installation & Setup

1. Make sure you have Node.js and `npm` installed.
2. Navigate to the `management-console` directory:
   ```bash
   cd management-console
   ```
3. Install dependencies:
   ```bash
   npm install
   ```
4. Start the development server:
   ```bash
   npm run dev
   ```
5. Open your browser to the local Vite URL (typically `http://localhost:5173`).

## Simulator Mode

By default, the `App.tsx` file includes a robust MQTT simulator that mocks environmental data, interpolates fleet vehicle coordinates, and simulates static infrastructure. This allows for full UI testing even if the physical Meshtastic hardware gateway is offline.
