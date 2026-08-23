import { lazy, Suspense, useCallback, useEffect, useRef, useState } from 'react';
import { BrowserRouter, Link, Route, Routes, useLocation } from 'react-router-dom';
import { Activity, Building2, LogOut, Radio, Server, SlidersHorizontal, Truck } from 'lucide-react';
import mqtt, { type MqttClient } from 'mqtt';
import { onAuthStateChanged, signOut, type User } from 'firebase/auth';
import { collection, doc, limit, onSnapshot, orderBy, query, serverTimestamp, setDoc, updateDoc } from 'firebase/firestore';
import { auth, db } from './firebase';
import { getRuntimeConfig } from './config';
import { buildCommandTopic, commandIdFromAcknowledgementTopic, createCommandEnvelope, parseControlAcknowledgement, type ControlAcknowledgement } from './commands';
import { mergeTelemetry, parseMqttMessage, parseTelemetryPayload } from './telemetry';
import type { CommandRequest, ConnectionState, TelemetryMessage } from './types';
import Login from './Login';

const Dashboard = lazy(() => import('./Dashboard'));
const Nodes = lazy(() => import('./Nodes'));
const Fleet = lazy(() => import('./Fleet'));
const Infrastructure = lazy(() => import('./Infrastructure'));
const Control = lazy(() => import('./Control'));

const config = getRuntimeConfig();
const AUTHORIZED_ROLES = new Set(['operator', 'admin']);
const COMMAND_TIMEOUT_MS = 60_000;
const MQTT_TOKEN_REFRESH_MS = 50 * 60_000;

interface PendingCommand {
  assetId: string;
  resolve: (acknowledgement: ControlAcknowledgement) => void;
  reject: (error: Error) => void;
  timer: ReturnType<typeof setTimeout>;
}

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
    <aside className="sidebar glass-panel">
      <div>
        <h1>ASCS <span className="emerald-text">Unified Ops</span></h1>
        <nav className="nav-menu" aria-label="Primary navigation">
          {menu.map((item) => (
            <Link to={item.path} key={item.name}>
              <span className={`nav-item ${location.pathname === item.path ? 'active' : ''}`}>
                {item.icon}{item.name}
              </span>
            </Link>
          ))}
        </nav>
      </div>
      <button className="secondary-button disconnect-button" onClick={() => void signOut(auth)}>
        <LogOut size={16} /> Disconnect
      </button>
    </aside>
  );
}

function connectionLabel(state: ConnectionState): string {
  if (state === 'connected') return 'MQTT Connected';
  if (state === 'connecting') return 'MQTT Connecting';
  if (state === 'error') return 'MQTT Error';
  return 'MQTT Disconnected';
}

function App() {
  const [user, setUser] = useState<User | null>(null);
  const [authLoading, setAuthLoading] = useState(true);
  const [authError, setAuthError] = useState('');
  const [messages, setMessages] = useState<TelemetryMessage[]>([]);
  const [connection, setConnection] = useState<ConnectionState>('disconnected');
  const [connectionError, setConnectionError] = useState('');
  const [mqttCredentialVersion, setMqttCredentialVersion] = useState(0);
  const mqttClient = useRef<MqttClient | null>(null);
  const pendingCommands = useRef(new Map<string, PendingCommand>());

  useEffect(() => {
    let revision = 0;
    const unsubscribe = onAuthStateChanged(auth, async (nextUser) => {
      const currentRevision = ++revision;
      setAuthError('');
      if (!nextUser) {
        setUser(null);
        setAuthLoading(false);
        return;
      }
      try {
        const token = await nextUser.getIdTokenResult();
        if (currentRevision !== revision) return;
        const role = typeof token.claims.role === 'string' ? token.claims.role : '';
        if (!AUTHORIZED_ROLES.has(role)) {
          setAuthError('Your account is not assigned an ASCS operator role.');
          setUser(null);
          await signOut(auth);
        } else {
          setUser(nextUser);
        }
      } catch {
        if (currentRevision !== revision) return;
        setAuthError('Unable to verify operator authorization.');
        setUser(null);
        await signOut(auth).catch(() => undefined);
      } finally {
        if (currentRevision === revision) setAuthLoading(false);
      }
    });
    return () => {
      ++revision;
      unsubscribe();
    };
  }, []);

  useEffect(() => {
    if (!user) return undefined;
    const telemetryQuery = query(collection(db, 'telemetry'), orderBy('receivedAt', 'desc'), limit(500));
    return onSnapshot(telemetryQuery, (snapshot) => {
      const history = snapshot.docs.flatMap((snapshotDoc) => {
        const data = snapshotDoc.data();
        const topic = typeof data.topic === 'string' ? data.topic : `${config.mqttBaseTopic}/sensor/history`;
        const parsed = parseTelemetryPayload(topic, data, new Date(), snapshotDoc.id);
        return parsed ? [parsed] : [];
      });
      setMessages((current) => mergeTelemetry(current, history));
    }, () => setConnectionError('Historical telemetry is unavailable. Check Firestore access and indexes.'));
  }, [user]);

  useEffect(() => {
    if (!user) return undefined;
    let disposed = false;
    let client: MqttClient | null = null;
    let refreshTimer: ReturnType<typeof setTimeout> | undefined;
    setConnection('connecting');
    setConnectionError('');
    const connect = async () => {
      try {
        const idToken = await user.getIdToken(mqttCredentialVersion > 0);
        if (disposed) return;
        client = mqtt.connect(config.mqttUrl, {
          username: user.uid,
          password: idToken,
          clean: true,
          connectTimeout: 10_000,
          reconnectPeriod: 5_000,
          resubscribe: true,
        });
        mqttClient.current = client;

        client.on('connect', () => {
          client?.subscribe([
            `${config.mqttBaseTopic}/sensor/#`,
            `${config.mqttBaseTopic}/control/ack/#`,
          ], { qos: 1 }, (error) => {
            if (disposed) return;
            if (error) {
              setConnection('error');
              setConnectionError('Connected to MQTT, but the scoped subscriptions failed.');
            } else {
              setConnection('connected');
              setConnectionError('');
            }
          });
        });
        client.on('reconnect', () => setConnection('connecting'));
        client.on('offline', () => setConnection('disconnected'));
        client.on('error', () => {
          setConnection('error');
          setConnectionError('MQTT connection failed. Verify broker TLS and Firebase token authentication.');
        });
        client.on('message', (topic, payload) => {
          const topicCommandId = commandIdFromAcknowledgementTopic(config.mqttBaseTopic, topic);
          if (topicCommandId) {
            const acknowledgement = parseControlAcknowledgement(payload);
            if (!acknowledgement || acknowledgement.commandId.toLowerCase() !== topicCommandId.toLowerCase()) return;
            const pending = pendingCommands.current.get(acknowledgement.commandId);
            if (pending && acknowledgement.nodeId.toLowerCase() === pending.assetId.toLowerCase()) {
              clearTimeout(pending.timer);
              pendingCommands.current.delete(acknowledgement.commandId);
              pending.resolve(acknowledgement);
            }
            return;
          }
          if (!topic.startsWith(`${config.mqttBaseTopic}/sensor/`)) return;
          const parsed = parseMqttMessage(topic, payload);
          if (parsed) setMessages((current) => mergeTelemetry(current, [parsed]));
        });

        const refreshCredentials = () => {
          if (disposed) return;
          if (pendingCommands.current.size > 0) {
            refreshTimer = setTimeout(refreshCredentials, COMMAND_TIMEOUT_MS);
          } else {
            setMqttCredentialVersion((version) => version + 1);
          }
        };
        refreshTimer = setTimeout(refreshCredentials, MQTT_TOKEN_REFRESH_MS);
      } catch {
        if (!disposed) {
          setConnection('error');
          setConnectionError('A Firebase identity token could not be obtained for MQTT authentication.');
        }
      }
    };
    void connect();

    return () => {
      disposed = true;
      if (refreshTimer) clearTimeout(refreshTimer);
      pendingCommands.current.forEach((pending) => {
        clearTimeout(pending.timer);
        pending.reject(new Error('MQTT disconnected before device acknowledgement'));
      });
      pendingCommands.current.clear();
      if (mqttClient.current === client) mqttClient.current = null;
      client?.end(true);
      setConnection('disconnected');
    };
  }, [user, mqttCredentialVersion]);

  const publishCommand = useCallback(async (request: CommandRequest) => {
    const client = mqttClient.current;
    if (!user || !client?.connected) throw new Error('MQTT is not connected');
    const commandId = crypto.randomUUID();
    const envelope = createCommandEnvelope(request, user.uid, commandId);
    const commandRef = doc(db, 'commands', commandId);

    await setDoc(commandRef, { ...envelope, requestedAt: serverTimestamp(), status: 'pending' });
    let acknowledgement: ControlAcknowledgement;
    try {
      const deviceAcknowledgement = new Promise<ControlAcknowledgement>((resolve, reject) => {
        const timer = setTimeout(() => {
          pendingCommands.current.delete(commandId);
          reject(new Error('Timed out waiting for device acknowledgement'));
        }, COMMAND_TIMEOUT_MS);
        pendingCommands.current.set(commandId, { assetId: request.assetId, resolve, reject, timer });
      });
      await new Promise<void>((resolve, reject) => {
        client.publish(buildCommandTopic(config.mqttBaseTopic, request.assetId), JSON.stringify(envelope), { qos: 1, retain: false }, (error) => {
          if (error) reject(error); else resolve();
        });
      });
      acknowledgement = await deviceAcknowledgement;
    } catch (error) {
      const pending = pendingCommands.current.get(commandId);
      if (pending) clearTimeout(pending.timer);
      pendingCommands.current.delete(commandId);
      const detail = error instanceof Error ? error.message.slice(0, 96) : 'Command failed';
      await updateDoc(commandRef, { status: 'failed', detail, completedAt: serverTimestamp() });
      throw error;
    }

    const detail = acknowledgement.detail.slice(0, 96);
    await updateDoc(commandRef, {
      status: acknowledgement.status,
      detail,
      completedAt: serverTimestamp(),
    });
    if (acknowledgement.status !== 'executed') {
      throw new Error(detail || `Device ${acknowledgement.status} the command`);
    }
  }, [user]);

  if (authLoading) return <main className="centered-page">Secure connection establishing…</main>;
  if (!user) return <Login authorizationError={authError} />;

  return (
    <BrowserRouter>
      <div className="app-container">
        <Sidebar />
        <main className="main-content">
          <header className="header">
            <div><h2 className="emerald-text">Management Console</h2><p className="silver-text">Akita Smart City Services</p></div>
            <div className={`glass-panel connection-pill ${connection === 'connected' ? 'emerald-text' : 'silver-text'}`}>
              <Radio size={16} />{connectionLabel(connection)}
            </div>
          </header>
          {connectionError && <div className="status-error" role="alert">{connectionError}</div>}
          <Suspense fallback={<div className="glass-panel empty-state">Loading console module…</div>}>
            <Routes>
              <Route path="/" element={<Dashboard messages={messages} />} />
              <Route path="/fleet" element={<Fleet messages={messages} />} />
              <Route path="/infrastructure" element={<Infrastructure messages={messages} />} />
              <Route path="/control" element={<Control messages={messages} connection={connection} publishCommand={publishCommand} />} />
              <Route path="/nodes" element={<Nodes messages={messages} />} />
              <Route path="*" element={<div className="glass-panel empty-state">The requested console page does not exist.</div>} />
            </Routes>
          </Suspense>
        </main>
      </div>
    </BrowserRouter>
  );
}

export default App;
