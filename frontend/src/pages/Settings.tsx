import { useEffect, useState, type FormEvent } from 'react';
import { Link } from 'react-router-dom';
import { LogOut, Moon, Shield, Sun, UserX } from 'lucide-react';
import Avatar from '../components/Avatar';
import { useAuth } from '../context/AuthContext';
import { useToast } from '../context/ToastContext';
import type { User } from '../lib/types';

export default function Settings() {
  const { user, theme, toggleTheme, request, refreshUser, logout } = useAuth();
  const { showToast } = useToast();
  const [form, setForm] = useState({
    fullName: user?.fullName || '',
    bio: user?.bio || '',
    website: user?.website || '',
    email: user?.email || '',
    isPrivate: user?.isPrivate || false,
    profilePicture: user?.profilePicture || '',
  });
  const [blocked, setBlocked] = useState<User[]>([]);
  const [forgotEmail, setForgotEmail] = useState(user?.email || '');
  const [devToken, setDevToken] = useState('');
  const [resetPassword, setResetPassword] = useState('');

  useEffect(() => {
    request<{ users: User[] }>('/users/blocked')
      .then((data) => setBlocked(data.users))
      .catch(() => undefined);
  }, [request]);

  useEffect(() => {
    if (!user) return;
    setForm({
      fullName: user.fullName,
      bio: user.bio,
      website: user.website,
      email: user.email || '',
      isPrivate: user.isPrivate,
      profilePicture: user.profilePicture,
    });
  }, [user]);

  const save = async (event: FormEvent) => {
    event.preventDefault();
    try {
      await request(`/users/${user?.id}`, { method: 'PUT', body: form });
      await refreshUser();
      showToast('Settings saved');
    } catch (error) {
      showToast(error instanceof Error ? error.message : 'Could not save settings', 'error');
    }
  };

  const unblock = async (id: number) => {
    try {
      await request(`/users/${id}/block`, { method: 'DELETE' });
      setBlocked((current) => current.filter((item) => item.id !== id));
      showToast('User unblocked');
    } catch {
      showToast('Could not unblock user', 'error');
    }
  };

  const requestReset = async () => {
    try {
      const data = await request<{ message: string; devToken?: string }>('/auth/forgot-password', {
        method: 'POST',
        body: { email: forgotEmail },
      });
      setDevToken(data.devToken || '');
      setResetPassword('');
      showToast(data.message);
    } catch {
      showToast('Could not request reset token', 'error');
    }
  };

  const submitReset = async () => {
    if (!devToken || resetPassword.length < 8) {
      showToast('Enter the reset token and a password of at least 8 characters', 'error');
      return;
    }
    try {
      await request('/auth/reset-password', {
        method: 'POST',
        body: { token: devToken, password: resetPassword },
      });
      showToast('Password updated. Please log in again.');
      setDevToken('');
      setResetPassword('');
      await logout();
    } catch (error) {
      showToast(error instanceof Error ? error.message : 'Could not reset password', 'error');
    }
  };

  return (
    <div className="mx-auto max-w-2xl space-y-5">
      <div>
        <h1 className="text-xl font-bold">Settings</h1>
        <p className="text-xs text-ig-muted">Manage your account, privacy and demo preferences</p>
      </div>

      <form onSubmit={save} className="card space-y-4 p-6">
        <div className="flex items-center gap-4 border-b border-ig-border pb-5 dark:border-darkig-border">
          <Avatar src={form.profilePicture} name={user?.username} size={70} />
          <div>
            <h2 className="font-semibold">Profile details</h2>
            <Link to={`/profile/${user?.id}`} className="text-sm text-ig-accent">View public profile</Link>
          </div>
        </div>
        <label className="block text-sm"><span className="mb-1 block font-semibold">Profile picture URL</span><input className="input" value={form.profilePicture} onChange={(e) => setForm({ ...form, profilePicture: e.target.value })} /></label>
        <label className="block text-sm"><span className="mb-1 block font-semibold">Full name</span><input className="input" value={form.fullName} onChange={(e) => setForm({ ...form, fullName: e.target.value })} /></label>
        <label className="block text-sm"><span className="mb-1 block font-semibold">Email</span><input className="input" type="email" value={form.email} onChange={(e) => setForm({ ...form, email: e.target.value })} /></label>
        <label className="block text-sm"><span className="mb-1 block font-semibold">Bio</span><textarea className="input min-h-[90px]" value={form.bio} onChange={(e) => setForm({ ...form, bio: e.target.value })} /></label>
        <label className="block text-sm"><span className="mb-1 block font-semibold">Website</span><input className="input" value={form.website} onChange={(e) => setForm({ ...form, website: e.target.value })} /></label>
        <label className="flex items-center justify-between rounded-lg bg-ig-hover p-3 text-sm dark:bg-darkig-hover">
          <span className="font-semibold">Private account</span>
          <input type="checkbox" checked={form.isPrivate} onChange={(e) => setForm({ ...form, isPrivate: e.target.checked })} className="h-5 w-5 accent-sky-600" />
        </label>
        <button type="submit" className="btn-primary">Save changes</button>
      </form>

      <div className="card divide-y divide-ig-border dark:divide-darkig-border">
        <button type="button" onClick={toggleTheme} className="flex w-full items-center gap-3 p-5 text-left hover:bg-ig-hover dark:hover:bg-darkig-hover">
          {theme === 'dark' ? <Sun size={20} /> : <Moon size={20} />}
          <span className="flex-1 font-semibold">Appearance</span>
          <span className="text-sm text-ig-muted">{theme === 'dark' ? 'Dark' : 'Light'} · tap to switch</span>
        </button>
        <div className="p-5">
          <div className="mb-3 flex items-center gap-3 font-semibold"><Shield size={20} /> Password reset</div>
          <div className="flex gap-2">
            <input className="input flex-1" type="email" placeholder="Your email" value={forgotEmail} onChange={(e) => setForgotEmail(e.target.value)} />
            <button type="button" onClick={() => void requestReset()} className="btn-secondary">Get token</button>
          </div>
          {devToken && (
            <div className="mt-3 space-y-3 rounded-lg bg-ig-hover p-3 text-xs dark:bg-darkig-hover">
              <p className="font-semibold">Development token (no email service configured):</p>
              <code className="block break-all">{devToken}</code>
              <input
                className="input"
                type="password"
                placeholder="New password (8+ characters)"
                value={resetPassword}
                onChange={(event) => setResetPassword(event.target.value)}
                minLength={8}
              />
              <button type="button" onClick={() => void submitReset()} className="btn-primary w-full py-2">
                Set new password
              </button>
              <p className="text-ig-muted">Resetting signs you out so the new password can be used to log in.</p>
            </div>
          )}
        </div>
        <div className="p-5">
          <div className="mb-3 flex items-center gap-3 font-semibold"><UserX size={20} /> Blocked accounts</div>
          {blocked.length === 0 ? <p className="text-sm text-ig-muted">You haven’t blocked anyone.</p> : (
            <div className="space-y-3">
              {blocked.map((item) => (
                <div key={item.id} className="flex items-center gap-3">
                  <Avatar src={item.profilePicture} name={item.username} size={36} />
                  <span className="flex-1 text-sm font-semibold">{item.username}</span>
                  <button type="button" onClick={() => void unblock(item.id)} className="btn-secondary py-1.5 text-xs">Unblock</button>
                </div>
              ))}
            </div>
          )}
        </div>
        <button type="button" onClick={() => void logout()} className="flex w-full items-center gap-3 p-5 text-left text-rose-500 hover:bg-rose-50 dark:hover:bg-rose-950/20">
          <LogOut size={20} /> <span className="font-semibold">Log out</span>
        </button>
      </div>

      <p className="text-center text-[11px] text-ig-muted">React + TypeScript · C++17 Crow · MySQL 8 · Custom DSA</p>
    </div>
  );
}
