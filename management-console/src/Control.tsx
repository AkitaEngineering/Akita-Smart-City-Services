import { useState } from 'react';
import { SlidersHorizontal, Lightbulb, Droplets } from 'lucide-react';

export default function Control() {
  const [streetLightOn, setStreetLightOn] = useState(true);
  const [brightness, setBrightness] = useState(80);
  const [poolPumpOn, setPoolPumpOn] = useState(true);

  const toggleAction = (asset: string, currentState: boolean, setter: (val: boolean) => void) => {
    // Quick confirmation dialog to balance speed and safety for critical city infra
    if (window.confirm(`Are you sure you want to turn ${currentState ? 'OFF' : 'ON'} the ${asset}?`)) {
      setter(!currentState);
      // In a real app, this would publish an MQTT message here
      // mqttClient.publish(`akita/smartcity/control/${asset.toLowerCase().replace(' ', '_')}`, JSON.stringify({ state: !currentState }));
    }
  };

  return (
    <div>
      <h3 style={{ marginBottom: '2rem', fontWeight: 500, display: 'flex', alignItems: 'center', gap: '0.5rem' }}>
        <SlidersHorizontal className="emerald-text" /> Command & Control (C2)
      </h3>

      <div className="dashboard-grid">
        
        {/* Street Light Control */}
        <div className="glass-panel" style={{ padding: '2rem' }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: '0.75rem', marginBottom: '1.5rem' }}>
                <Lightbulb className={streetLightOn ? 'emerald-text' : 'silver-text'} size={24} />
                <h4 style={{ fontSize: '1.25rem' }}>Downtown Street Lighting</h4>
            </div>
            
            <div style={{ marginBottom: '2rem' }}>
                <span className="silver-text">Master Override Switch</span>
                <button 
                    onClick={() => toggleAction('Downtown Street Lighting', streetLightOn, setStreetLightOn)}
                    style={{ 
                        display: 'block',
                        marginTop: '0.5rem',
                        width: '100%',
                        padding: '1rem',
                        background: streetLightOn ? 'rgba(16, 185, 129, 0.2)' : 'rgba(239, 68, 68, 0.2)',
                        color: streetLightOn ? 'var(--color-emerald)' : 'var(--color-danger)',
                        border: `1px solid ${streetLightOn ? 'var(--color-emerald)' : 'var(--color-danger)'}`,
                        borderRadius: '8px',
                        cursor: 'pointer',
                        fontWeight: 600,
                        fontSize: '1.1rem'
                    }}
                >
                    {streetLightOn ? 'POWER ON' : 'POWER OFF'}
                </button>
            </div>

            <div>
                <span className="silver-text">Brightness Level ({brightness}%)</span>
                <input 
                    type="range" 
                    min="0" max="100" 
                    value={brightness} 
                    onChange={(e) => setBrightness(Number(e.target.value))}
                    style={{ width: '100%', marginTop: '0.5rem', cursor: 'pointer' }}
                />
            </div>
        </div>

        {/* Pool Control */}
        <div className="glass-panel" style={{ padding: '2rem' }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: '0.75rem', marginBottom: '1.5rem' }}>
                <Droplets className={poolPumpOn ? 'emerald-text' : 'silver-text'} size={24} />
                <h4 style={{ fontSize: '1.25rem' }}>North Pool Filtration Pump</h4>
            </div>
            
            <div style={{ marginBottom: '2rem' }}>
                <span className="silver-text">Primary Pump Relay</span>
                <button 
                    onClick={() => toggleAction('Pool Filtration Pump', poolPumpOn, setPoolPumpOn)}
                    style={{ 
                        display: 'block',
                        marginTop: '0.5rem',
                        width: '100%',
                        padding: '1rem',
                        background: poolPumpOn ? 'rgba(16, 185, 129, 0.2)' : 'rgba(239, 68, 68, 0.2)',
                        color: poolPumpOn ? 'var(--color-emerald)' : 'var(--color-danger)',
                        border: `1px solid ${poolPumpOn ? 'var(--color-emerald)' : 'var(--color-danger)'}`,
                        borderRadius: '8px',
                        cursor: 'pointer',
                        fontWeight: 600,
                        fontSize: '1.1rem'
                    }}
                >
                    {poolPumpOn ? 'PUMP ACTIVE' : 'PUMP OFFLINE'}
                </button>
            </div>
            <p className="silver-text" style={{ fontSize: '0.9rem' }}>
                Note: Changing pump status will override local automated chemical dosing schedules. Use with caution.
            </p>
        </div>

      </div>
    </div>
  );
}
