import { useCallback, useEffect, useState } from 'react';
import { Link, useNavigate } from 'react-router-dom';
import { Bell, CheckCheck, Heart, MessageCircle, UserPlus } from 'lucide-react';
import Avatar from '../components/Avatar';
import Spinner from '../components/Spinner';
import { useAuth } from '../context/AuthContext';
import type { NotificationItem } from '../lib/types';
import { cx, timeAgo } from '../lib/utils';

const iconForType = (type: string) => {
  if (type.includes('message')) return MessageCircle;
  if (type.includes('follow')) return UserPlus;
  return Heart;
};

export default function Notifications() {
  const { request } = useAuth();
  const navigate = useNavigate();
  const [items, setItems] = useState<NotificationItem[]>([]);
  const [loading, setLoading] = useState(true);

  const load = useCallback(() => {
    request<{ notifications: NotificationItem[] }>('/notifications')
      .then((data) => setItems(data.notifications))
      .catch(() => undefined)
      .finally(() => setLoading(false));
  }, [request]);

  useEffect(() => { load(); }, [load]);

  const read = async (item: NotificationItem) => {
    if (!item.isRead) {
      try {
        await request(`/notifications/${item.id}/read`, { method: 'POST' });
        setItems((current) => current.map((n) => n.id === item.id ? { ...n, isRead: true } : n));
        window.dispatchEvent(new Event('ig:notifications-changed'));
      } catch { /* ignore */ }
    }
    if (item.type.includes('follow')) navigate(`/profile/${item.actorId}`);
    else if (item.type.includes('message')) navigate('/messages');
    else if (item.entityId) navigate(`/post/${item.entityId}`);
  };

  const readAll = async () => {
    try {
      await request('/notifications/read-all', { method: 'POST' });
      setItems((current) => current.map((item) => ({ ...item, isRead: true })));
      window.dispatchEvent(new Event('ig:notifications-changed'));
    } catch { /* ignore */ }
  };

  if (loading) return <Spinner label="Loading notifications…" />;

  return (
    <div className="mx-auto max-w-2xl space-y-4">
      <div className="flex items-center justify-between">
        <div>
          <h1 className="text-xl font-bold">Notifications</h1>
          <p className="text-xs text-ig-muted">Likes, comments, follows and messages</p>
        </div>
        <button type="button" onClick={() => void readAll()} className="btn-secondary flex items-center gap-2 py-2 text-xs">
          <CheckCheck size={16} /> Mark all read
        </button>
      </div>

      <div className="card divide-y divide-ig-border dark:divide-darkig-border">
        {items.length === 0 ? (
          <div className="flex flex-col items-center gap-2 p-14 text-center">
            <Bell size={40} strokeWidth={1.2} className="text-ig-muted" />
            <p className="font-semibold">You’re all caught up</p>
            <p className="text-sm text-ig-muted">New activity will appear here.</p>
          </div>
        ) : items.map((item) => {
          const Icon = iconForType(item.type);
          return (
            <button key={item.id} type="button" onClick={() => void read(item)} className={cx('flex w-full items-center gap-3 p-4 text-left transition hover:bg-ig-hover dark:hover:bg-darkig-hover', !item.isRead && 'bg-sky-50/60 dark:bg-sky-950/20')}>
              <div className="relative">
                <Avatar src={item.actorAvatar} name={item.actorUsername} size={46} />
                <span className="absolute -bottom-1 -right-1 flex h-6 w-6 items-center justify-center rounded-full border-2 border-ig-panel bg-ig-accent text-white dark:border-darkig-panel">
                  <Icon size={12} fill={item.type.includes('like') ? 'currentColor' : 'none'} />
                </span>
              </div>
              <div className="min-w-0 flex-1">
                <p className="text-sm"><Link to={`/profile/${item.actorId}`} className="font-semibold" onClick={(event) => event.stopPropagation()}>{item.actorUsername}</Link> {item.body.replace(`${item.actorUsername} `, '')}</p>
                <p className="mt-1 text-xs text-ig-muted">{timeAgo(item.createdAt)}</p>
              </div>
              {!item.isRead && <span className="h-2 w-2 shrink-0 rounded-full bg-ig-accent" />}
            </button>
          );
        })}
      </div>
    </div>
  );
}
