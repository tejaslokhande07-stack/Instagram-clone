import { useState, type FormEvent } from 'react';
import { Link, useNavigate } from 'react-router-dom';
import { useAuth } from '../context/AuthContext';
import { useToast } from '../context/ToastContext';

export default function Register() {
  const { register } = useAuth();
  const { showToast } = useToast();
  const navigate = useNavigate();
  const [form, setForm] = useState({ username: '', email: '', fullName: '', password: '' });
  const [busy, setBusy] = useState(false);

  const update = (key: keyof typeof form) => (event: React.ChangeEvent<HTMLInputElement>) => {
    setForm((current) => ({ ...current, [key]: event.target.value }));
  };

  const submit = async (event: FormEvent) => {
    event.preventDefault();
    if (busy) return;
    setBusy(true);
    try {
      await register({
        username: form.username.trim(),
        email: form.email.trim(),
        fullName: form.fullName.trim() || form.username.trim(),
        password: form.password,
      });
      showToast('Account created. Welcome!');
      navigate('/');
    } catch (error) {
      showToast(error instanceof Error ? error.message : 'Registration failed', 'error');
    } finally {
      setBusy(false);
    }
  };

  return (
    <div className="flex min-h-screen items-center justify-center bg-ig-bg px-4 py-10 dark:bg-darkig-bg">
      <div className="w-full max-w-[440px]">
        <div className="card p-8">
          <h1 className="text-center text-3xl font-bold italic tracking-tight">Instagram</h1>
          <p className="mt-2 text-center text-sm text-ig-muted dark:text-darkig-muted">Sign up to share your moments.</p>
          <form onSubmit={(event) => void submit(event)} className="mt-7 space-y-3">
            <input className="input" placeholder="Username (3–30 letters, digits, . or _)" value={form.username} onChange={update('username')} required minLength={3} maxLength={30} />
            <input className="input" type="email" placeholder="Email address" value={form.email} onChange={update('email')} required />
            <input className="input" placeholder="Full name" value={form.fullName} onChange={update('fullName')} maxLength={100} />
            <input className="input" type="password" placeholder="Password (8+ characters)" value={form.password} onChange={update('password')} required minLength={8} />
            <button type="submit" disabled={busy} className="btn-primary w-full py-3">
              {busy ? 'Creating account…' : 'Sign up'}
            </button>
          </form>
          <p className="mt-5 text-center text-[11px] leading-5 text-ig-muted dark:text-darkig-muted">
            Passwords are hashed with Argon2id. By signing up you agree to the demo terms of this academic project.
          </p>
        </div>
        <p className="mt-4 text-center text-sm">
          Already have an account? <Link to="/login" className="font-semibold text-ig-accent">Log in</Link>
        </p>
      </div>
    </div>
  );
}
