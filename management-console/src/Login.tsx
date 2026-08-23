import { useState } from 'react';
import { signInWithEmailAndPassword } from 'firebase/auth';
import { auth } from './firebase';
import { Lock } from 'lucide-react';

export default function Login({ authorizationError = '' }: { authorizationError?: string }) {
  const [email, setEmail] = useState('');
  const [password, setPassword] = useState('');
  const [error, setError] = useState('');
  const [submitting, setSubmitting] = useState(false);

  const handleLogin = async (e: React.FormEvent) => {
    e.preventDefault();
    if (submitting) return;
    setSubmitting(true);
    setError('');
    try {
      await signInWithEmailAndPassword(auth, email, password);
    } catch {
      setError('Authentication failed. Verify your email, password, and operator access.');
    } finally {
      setSubmitting(false);
    }
  };

  return (
    <div style={{ display: 'flex', height: '100vh', alignItems: 'center', justifyContent: 'center', background: 'var(--bg-gradient)' }}>
      <div className="glass-panel" style={{ padding: '3rem', width: '100%', maxWidth: '400px', textAlign: 'center' }}>
        <Lock size={48} className="emerald-text" style={{ marginBottom: '1.5rem' }} />
        <h2 style={{ marginBottom: '0.5rem', fontWeight: 600 }}>ASCS Unified Ops</h2>
        <p className="silver-text" style={{ marginBottom: '2rem' }}>Authorized Personnel Only</p>
        
        <form onSubmit={handleLogin} style={{ display: 'flex', flexDirection: 'column', gap: '1rem' }}>
          <input 
            type="email" 
            placeholder="Operator Email" 
            value={email}
            onChange={(e) => setEmail(e.target.value)}
            autoComplete="username"
            style={{ padding: '1rem', borderRadius: '8px', border: '1px solid var(--glass-border)', background: 'rgba(0,0,0,0.5)', color: '#fff' }}
            required
          />
          <input 
            type="password" 
            placeholder="Password" 
            value={password}
            onChange={(e) => setPassword(e.target.value)}
            autoComplete="current-password"
            style={{ padding: '1rem', borderRadius: '8px', border: '1px solid var(--glass-border)', background: 'rgba(0,0,0,0.5)', color: '#fff' }}
            required
          />
          {(error || authorizationError) && <div className="danger-text" style={{ fontSize: '0.875rem' }} role="alert">{error || authorizationError}</div>}
          <button type="submit" disabled={submitting} style={{ padding: '1rem', background: 'var(--color-emerald)', color: '#fff', border: 'none', borderRadius: '8px', fontWeight: 600, cursor: 'pointer', marginTop: '1rem' }}>
            {submitting ? 'Authenticating…' : 'Acknowledge & Authenticate'}
          </button>
        </form>
      </div>
    </div>
  );
}
