import { useState, useEffect } from 'react';
import { BrowserRouter, Routes, Route, Link, useLocation } from 'react-router-dom';
import { Activity, Server, Radio, Truck, Building2, SlidersHorizontal, LogOut } from 'lucide-react';
import Dashboard from './Dashboard';
import Nodes from './Nodes';
import Fleet from './Fleet';
import Infrastructure from './Infrastructure';
import Control from './Control';
import Login from './Login';
import mqtt from 'mqtt';
import { onAuthStateChanged, signOut } from 'firebase/auth';
import { collection, addDoc, onSnapshot, query, orderBy, limit } from 'firebase/firestore';
import { auth, db } from './firebase';

function Sidebar() {
  const location = useLocation();
  const menu = [
    { name: 'Dashboard', path: '/', icon: <Activity size={20} /> },
    { name: 'Fleet Ops', path: '/fleet', icon: <Truck size={20} /> },
    { name: 'Infrastructure', path: '/infrastructure', icon: <Building2 size={20} /> },
    { name: 'Command & Control', path: '/control', icon: <SlidersHorizontal size={20} /> },
    { name: 'Network Nodes', path: '/nodes', icon: <Server size={20} /> },
  ];

  return (
    <div className="sidebar glass-panel" style={{ borderRadius: 0, borderTop: 'none', borderBottom: 'none', borderLeft: 'none', display: 'flex', flexDirection: 'column' }}>
      <div>
        <h1>ASCS <span className="emerald-text">Unified Ops</span></h1>
        <div className="nav-menu">
          {menu.map(item => (
            <Link to={item.path} key={item.name} style={{ textDecoration: 'none' }}>
              <div className={`nav-item ${location.pathname === item.path ? 'active' : ''}`}>
                {item.icon}
                {item.name}
              </div>
            </Link>
          ))}
        </div>
      </div>
      <div style={{ marginTop: 'auto' }}>
        <button 
          onClick={() => signOut(auth)} 
          style={{ width: '100%', padding: '0.75rem', background: 'transparent', border: '1px solid var(--color-silver)', color: 'var(--color-silver)', borderRadius: '8px', cursor: 'pointer', display: 'flex', alignItems: 'center', gap: '0.5rem', justifyContent: 'center' }}
        >
          <LogOut size={16} /> Disconnect
        </button>
      </div>
    </div>
  );
}

function App() {
  const [isAuthenticated, setIsAuthenticated] = useState(false);
  const [isLoadingAuth, setIsLoadingAuth] = useState(true);
  const [, setMqttClient] = useState<mqtt.MqttClient | null>(null);
  const [messages, setMessages] = useState<any[]>([]);
  const [connected, setConnected] = useState(false);

  // Auth Listener
  useEffect(() => {
    const unsubscribe = onAuthStateChanged(auth, (user) => {
      setIsAuthenticated(!!user);
      setIsLoadingAuth(false);
    });
    return () => unsubscribe();
  }, []);

  // Telemetry & Simulator logic
  useEffect(() => {
    if (!isAuthenticated) return;

    const brokerUrl = import.meta.env.VITE_MQTT_BROKER_URL || 'wss://test.mosquitto.org:8081';
    const enableSimulator = import.meta.env.VITE_ENABLE_SIMULATOR === 'true';

    // 1. Fetch historical data from Firestore (last 200 items)
    const q = query(collection(db, 'telemetry'), orderBy('receivedAt', 'desc'), limit(200));
    const unsubscribeFirestore = onSnapshot(q, (snapshot) => {
        const history: any[] = [];
        snapshot.forEach(doc => history.push({ id: doc.id, ...doc.data() }));
        // Reverse to get chronological order for charting
        setMessages(history.reverse());
    }, (error) => {
        console.warn('Firestore fetch failed (likely due to mock config). Falling back to memory state.', error);
    });

    // 2. Connect to MQTT for Live Data
    const client = mqtt.connect(brokerUrl);

    client.on('connect', () => {
      setConnected(true);
      client.subscribe('akita/smartcity/#');
      setMqttClient(client);
    });

    client.on('message', async (topic, payload) => {
      try {
        const data = JSON.parse(payload.toString());
        const newMsg = { topic, ...data, receivedAt: new Date().toISOString() };
        
        // Write live data to Firestore (which will auto-update state via onSnapshot)
        try {
            await addDoc(collection(db, 'telemetry'), newMsg);
        } catch (e) {
            // If firestore fails (mock mode), just update local memory
            setMessages(prev => [...prev.slice(-190), newMsg]);
        }
      } catch (e) {
        // ignore invalid
      }
    });

    // 3. Simulator (Only run if enabled via .env)
    let simInterval: any;
    if (enableSimulator) {
        const vehicles = [
          { id: 'BUS-101', type: 'Bus', lat: 39.7180, lng: 140.1000, heading: 0.001 },
          { id: 'TRK-GW1', type: 'Garbage Truck', lat: 39.7220, lng: 140.1050, heading: -0.001 },
          { id: 'PLW-01', type: 'Snow Plow', lat: 39.7150, lng: 140.1080, heading: 0.0005 },
          { id: 'CITY-V4', type: 'Municipal Vehicle', lat: 39.7250, lng: 140.0950, heading: -0.0008 }
        ];

        const staticInfra = [
          { id: 'SL-Downtown', type: 'Street Light', lat: 39.7205, lng: 140.1030, state: { on: true, brightness: 80 } },
          { id: 'PRK-Central', type: 'Park Monitor', lat: 39.7190, lng: 140.1010, state: { people_count: 45, sprinklers: false } },
          { id: 'POL-North', type: 'Pool System', lat: 39.7240, lng: 140.1060, state: { pump_active: true, temp_c: 26.5 } }
        ];

        simInterval = setInterval(() => {
          const isAnomaly = Math.random() > 0.95;
          const envData = {
            node_id: "a1b2c3d4",
            sensor_id: "Env-Downtown-01",
            category: "Environment",
            timestamp_utc: Math.floor(Date.now() / 1000),
            readings: {
              temperature_c: isAnomaly ? 38 + Math.random() * 5 : 20 + Math.random() * 5,
              humidity_pct: 40 + Math.random() * 10,
              pressure_pa: 101000 + Math.random() * 500,
              aqi: isAnomaly ? 160 + Math.random() * 50 : 20 + Math.random() * 30,
              noise_db: 55 + Math.random() * 20,
              traffic_count: Math.floor(Math.random() * 15),
              battery_v: 3.6 + Math.random() * 0.2,
              rssi: -60 - Math.random() * 20,
              latitude: 39.7200,
              longitude: 140.1026
            }
          };

          vehicles.forEach(v => {
            v.lat += v.heading;
            v.lng += (Math.random() - 0.5) * 0.001;
            if (v.lat > 39.73 || v.lat < 39.71) v.heading *= -1;
          });

          staticInfra.forEach(i => {
            if (i.type === 'Park Monitor') (i.state as any).people_count = Math.max(0, (i.state as any).people_count + Math.floor((Math.random() - 0.5) * 5));
          });

          const allEmissions = [
            { topic: 'akita/smartcity/sensor/env', ...envData },
            ...vehicles.map(v => ({ topic: 'akita/smartcity/fleet/' + v.id, category: 'Fleet', node_id: v.id, sensor_id: v.type, readings: { latitude: v.lat, longitude: v.lng, speed_kmh: 30 + Math.random()*20 }})),
            ...staticInfra.map(i => ({ topic: 'akita/smartcity/infra/' + i.id, category: 'Infrastructure', node_id: i.id, sensor_id: i.type, readings: { latitude: i.lat, longitude: i.lng, ...i.state }}))
          ].map(e => ({ ...e, receivedAt: new Date().toISOString() }));

          // For simulation, just push straight to local memory to avoid spamming the mock firestore project
          setMessages(prev => [...prev.slice(-190), ...allEmissions]);
        }, 2500);
    }

    return () => {
      client.end();
      if (simInterval) clearInterval(simInterval);
      unsubscribeFirestore();
    };
  }, [isAuthenticated]);

  if (isLoadingAuth) return <div style={{ display: 'flex', height: '100vh', alignItems: 'center', justifyContent: 'center' }}>Secure Connection Establishing...</div>;
  if (!isAuthenticated) return <Login onLogin={() => setIsAuthenticated(true)} />;

  // Messages parsing for dates since Firestore/Simulator uses ISO string
  const parsedMessages = messages.map(m => ({
      ...m,
      receivedAt: new Date(m.receivedAt)
  }));

  return (
    <BrowserRouter>
      <div className="app-container">
        <Sidebar />
        <div className="main-content">
          <div className="header">
            <div>
              <h2 className="emerald-text">Management Console</h2>
              <p className="silver-text">Akita Smart City Services</p>
            </div>
            <div className={`glass-panel ${connected ? 'emerald-text' : 'silver-text'}`} style={{ padding: '0.5rem 1rem', display: 'flex', alignItems: 'center', gap: '0.5rem' }}>
              <Radio size={16} />
              {connected ? 'MQTT Connected' : 'Connecting...'}
            </div>
          </div>
          
          <Routes>
            <Route path="/" element={<Dashboard messages={parsedMessages} />} />
            <Route path="/fleet" element={<Fleet messages={parsedMessages} />} />
            <Route path="/infrastructure" element={<Infrastructure messages={parsedMessages} />} />
            <Route path="/control" element={<Control />} />
            <Route path="/nodes" element={<Nodes messages={parsedMessages} />} />
          </Routes>
        </div>
      </div>
    </BrowserRouter>
  );
}

export default App;
