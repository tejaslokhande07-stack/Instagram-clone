import { useState, type FormEvent } from 'react';
import { Link, useNavigate } from 'react-router-dom';
import { Eye, EyeOff, Instagram } from 'lucide-react';
import { useAuth } from '../context/AuthContext';
import { useToast } from '../context/ToastContext';

export default function Login() {
  const { login } = useAuth();
  const { showToast } = useToast();
  const navigate = useNavigate();
  const [username, setUsername] = useState('');
  const [password, setPassword] = useState('');
  const [showPassword, setShowPassword] = useState(false);
  const [busy, setBusy] = useState(false);

  const submit = async (event: FormEvent) => {
    event.preventDefault();
    if (busy) return;
    setBusy(true);
    try {
      await login(username.trim(), password);
      showToast(`Welcome back, ${username}!`);
      navigate('/');
    } catch (error) {
      showToast(error instanceof Error ? error.message : 'Login failed', 'error');
    } finally {
      setBusy(false);
    }
  };

  const fillDemo = (name: string) => {
    setUsername(name);
    setPassword('Pass@123');
  };

  return (
    <div className="flex min-h-screen flex-col items-center justify-center bg-ig-bg px-4 py-10 dark:bg-darkig-bg md:flex-row md:gap-16">
      <div className="mb-8 hidden w-[380px] md:block">
        <div className="gradient-brand flex h-[560px] items-center justify-center rounded-[36px] p-1 shadow-modal">
          <div className="flex h-full w-full flex-col items-center justify-center rounded-[32px] bg-ig-bg/95 p-10 text-center dark:bg-darkig-bg/95">
            <Instagram size={76} strokeWidth={1.2} />
            <h2 className="mt-8 text-2xl font-semibold">A place for your moments</h2>
            <p className="mt-3 text-sm leading-6 text-ig-muted dark:text-darkig-muted">
              Share photos, watch reels, chat with friends and discover creators you love — all powered by a C++ backend and custom data structures.
            </p>
            <div className="mt-10 flex gap-2 text-3xl">📸 🎬 💬 ✨</div>
          </div>
        </div>
      </div>

      <div className="w-full max-w-[400px]">
        <div className="card p-8">
          <div className="mb-8 text-center">
            <h1 className="text-3xl font-bold italic tracking-tight">Instagram</h1>
            <p className="mt-2 text-sm text-ig-muted dark:text-darkig-muted">Sign in to see photos and videos from your friends.</p>
          </div>
          <form onSubmit={(event) => void submit(event)} className="space-y-3">
            <input className="input" placeholder="Username or email" value={username} onChange={(event) => setUsername(event.target.value)} autoComplete="username" required />
            <div className="relative">
              <input className="input pr-12" type={showPassword ? 'text' : 'password'} placeholder="Password" value={password} onChange={(event) => setPassword(event.target.value)} autoComplete="current-password" required />
              <button type="button" onClick={() => setShowPassword((v) => !v)} className="absolute right-3 top-1/2 -translate-y-1/2 text-ig-muted" aria-label="Toggle password visibility">
                {showPassword ? <EyeOff size={18} /> : <Eye size={18} />}
              </button>
            </div>
            <button type="submit" disabled={busy} className="btn-primary w-full py-3">
              {busy ? 'Signing in…' : 'Log in'}
            </button>
          </form>

          <div className="mt-6 rounded-xl border border-ig-border bg-ig-hover p-4 text-center text-xs dark:border-darkig-border dark:bg-darkig-hover">
            <p className="font-semibold">Demo accounts · password <code>Pass@123</code></p>
            <div className="mt-2 flex flex-wrap justify-center gap-2">
              {['alice', 'bob', 'charlie', 'diana', 'admin'].map((name) => (
                <button key={name} type="button" onClick={() => fillDemo(name)} className="rounded-full border border-ig-border bg-ig-panel px-3 py-1 hover:border-ig-accent dark:border-darkig-border dark:bg-darkig-panel">
                  {name}
                </button>
              ))}
            </div>
          </div>
        </div>
        <p className="mt-4 text-center text-sm">
          Don’t have an account? <Link to="/register" className="font-semibold text-ig-accent">Sign up</Link>
        </p>
      </div>
    </div>
  );
}
