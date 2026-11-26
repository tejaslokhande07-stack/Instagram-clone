import { useEffect, useState } from 'react';
import { Link } from 'react-router-dom';
import Avatar from './Avatar';
import Modal from './Modal';
import Spinner from './Spinner';
import { useAuth } from '../context/AuthContext';
import type { User } from '../lib/types';

interface UserListModalProps {
  open: boolean;
  onClose: () => void;
  title: string;
  endpoint: string;
  keyName?: string;
}

export default function UserListModal({ open, onClose, title, endpoint, keyName = 'users' }: UserListModalProps) {
  const { request } = useAuth();
  const [users, setUsers] = useState<User[]>([]);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState(false);
  const [retry, setRetry] = useState(0);

  useEffect(() => {
    if (!open) return;
    let active = true;
    setLoading(true);
    setError(false);
    request<Record<string, User[]>>(endpoint)
      .then((data) => {
        if (active) setUsers(data[keyName] || []);
      })
      .catch(() => { if (active) { setUsers([]); setError(true); } })
      .finally(() => { if (active) setLoading(false); });
    return () => { active = false; };
  }, [open, endpoint, keyName, request, retry]);

  return (
    <Modal open={open} onClose={onClose} title={title}>
      {loading ? <Spinner /> : error ? (
        <div className="p-8 text-center text-sm">
          <p className="text-ig-muted">Could not load {title.toLowerCase()}.</p>
          <button type="button" onClick={() => setRetry((value) => value + 1)} className="mt-3 font-semibold text-ig-accent">Try again</button>
        </div>
      ) : users.length === 0 ? (
        <p className="p-8 text-center text-sm text-ig-muted">Nobody here yet.</p>
      ) : (
        <ul className="divide-y divide-ig-border dark:divide-darkig-border">
          {users.map((user) => (
            <li key={user.id}>
              <Link to={`/profile/${user.id}`} onClick={onClose} className="flex items-center gap-3 px-5 py-3 hover:bg-ig-hover dark:hover:bg-darkig-hover">
                <Avatar src={user.profilePicture} name={user.username} size={44} />
                <div className="min-w-0 flex-1">
                  <div className="flex items-center gap-1 text-sm font-semibold">
                    <span className="truncate">{user.username}</span>
                    {user.isVerified && <span className="text-ig-accent">✓</span>}
                  </div>
                  <div className="truncate text-xs text-ig-muted dark:text-darkig-muted">{user.fullName}</div>
                </div>
              </Link>
            </li>
          ))}
        </ul>
      )}
    </Modal>
  );
}
