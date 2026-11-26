import { useEffect, useState } from 'react';
import { NavLink, Outlet, useLocation, useNavigate } from 'react-router-dom';
import {
  Bell, Compass, Film, Heart, Home, ImagePlus, Menu, MessageCircle,
  Moon, Plus, Search, Settings, Sun, User, X,
} from 'lucide-react';
import Avatar from './Avatar';
import { useAuth } from '../context/AuthContext';
import { cx } from '../lib/utils';

const navItems = [
  { to: '/', label: 'Home', icon: Home },
  { to: '/search', label: 'Search', icon: Search },
  { to: '/explore', label: 'Explore', icon: Compass },
  { to: '/reels', label: 'Reels', icon: Film },
  { to: '/messages', label: 'Messages', icon: MessageCircle },
  { to: '/notifications', label: 'Notifications', icon: Bell },
];

export default function Layout() {
  const { user, theme, toggleTheme, request, logout } = useAuth();
  const navigate = useNavigate();
  const location = useLocation();
  const [search, setSearch] = useState('');
  const [unread, setUnread] = useState(0);
  const [unreadMessages, setUnreadMessages] = useState(0);
  const [createOpen, setCreateOpen] = useState(false);
  const [sidebarOpen, setSidebarOpen] = useState(false);

  useEffect(() => {
    let active = true;
    const loadNotifications = () => request<{ count: number }>('/notifications/unread')
      .then((data) => { if (active) setUnread(data.count); })
      .catch(() => undefined);
    const loadMessages = () => request<{ count: number }>('/messages/unread')
      .then((data) => { if (active) setUnreadMessages(data.count); })
      .catch(() => undefined);
    void loadNotifications();
    void loadMessages();
    const onNotificationsChanged = () => void loadNotifications();
    const onMessagesChanged = () => void loadMessages();
    window.addEventListener('ig:notifications-changed', onNotificationsChanged);
    window.addEventListener('ig:messages-changed', onMessagesChanged);
    const timer = window.setInterval(() => {
      void loadNotifications();
      void loadMessages();
    }, 15000);
    return () => {
      active = false;
      window.clearInterval(timer);
      window.removeEventListener('ig:notifications-changed', onNotificationsChanged);
      window.removeEventListener('ig:messages-changed', onMessagesChanged);
    };
  }, [request, location.pathname]);

  useEffect(() => {
    setSidebarOpen(false);
    setCreateOpen(false);
  }, [location.pathname]);

  const navClass = ({ isActive }: { isActive: boolean }) => cx(
    'flex items-center gap-4 rounded-lg px-3 py-3 text-[15px] transition hover:bg-ig-hover dark:hover:bg-darkig-hover',
    isActive ? 'font-bold' : 'font-normal',
  );

  const create = (kind: 'post' | 'story' | 'reel') => {
    setCreateOpen(false);
    setSidebarOpen(false);
    if (kind === 'reel') navigate('/reels?create=reel');
    else navigate(`/?create=${kind}`);
  };

  const submitSearch = (event: React.FormEvent) => {
    event.preventDefault();
    if (search.trim()) navigate(`/search?q=${encodeURIComponent(search.trim())}`);
  };

  return (
    <div className="min-h-screen">
      <header className="sticky top-0 z-30 flex h-16 items-center border-b border-ig-border bg-ig-panel/95 px-3 backdrop-blur dark:border-darkig-border dark:bg-darkig-panel/95 md:hidden">
        <button type="button" className="icon-btn" onClick={() => setSidebarOpen(true)} aria-label="Open menu">
          <Menu size={23} />
        </button>
        <button type="button" onClick={() => navigate('/')} className="ml-2 text-xl font-bold italic tracking-tight">
          Instagram
        </button>
        <div className="ml-auto flex items-center gap-1">
          <button type="button" className="icon-btn" onClick={() => navigate('/search')} aria-label="Search">
            <Search size={22} />
          </button>
          <button type="button" className="icon-btn" onClick={() => create('post')} aria-label="Create">
            <Plus size={24} />
          </button>
        </div>
      </header>

      <aside className={cx(
        'fixed inset-y-0 left-0 z-40 flex w-[244px] flex-col border-r border-ig-border bg-ig-panel px-3 py-6 transition-transform dark:border-darkig-border dark:bg-darkig-panel md:sticky md:top-0 md:h-screen md:translate-x-0',
        sidebarOpen ? 'translate-x-0' : '-translate-x-full',
      )}>
        <div className="mb-8 flex items-center justify-between px-3">
          <button type="button" onClick={() => navigate('/')} className="text-2xl font-bold italic tracking-tight">
            Instagram
          </button>
          <button type="button" className="icon-btn md:hidden" onClick={() => setSidebarOpen(false)} aria-label="Close menu">
            <X size={20} />
          </button>
        </div>

        <nav className="flex flex-1 flex-col gap-1">
          {navItems.map(({ to, label, icon: Icon }) => (
            <NavLink key={to} to={to} end={to === '/'} className={navClass}>
              <Icon size={24} strokeWidth={location.pathname === to ? 2.5 : 1.8} />
              <span className="relative">
                {label}
                {label === 'Messages' && unreadMessages > 0 && (
                  <span className="absolute -right-5 -top-1 flex h-4 min-w-4 items-center justify-center rounded-full bg-ig-accent px-1 text-[10px] font-bold text-white">
                    {unreadMessages > 9 ? '9+' : unreadMessages}
                  </span>
                )}
                {label === 'Notifications' && unread > 0 && (
                  <span className="absolute -right-5 -top-1 flex h-4 min-w-4 items-center justify-center rounded-full bg-rose-500 px-1 text-[10px] font-bold text-white">
                    {unread > 9 ? '9+' : unread}
                  </span>
                )}
              </span>
            </NavLink>
          ))}

          <button type="button" onClick={() => create('post')} className={navClass({ isActive: false })}>
            <Plus size={24} strokeWidth={1.8} /> Create
          </button>

          <NavLink to={user ? `/profile/${user.id}` : '/profile'} className={navClass}>
            <Avatar src={user?.profilePicture} name={user?.username} size={24} /> Profile
          </NavLink>
          <NavLink to="/settings" className={navClass}>
            <Settings size={24} strokeWidth={1.8} /> Settings
          </NavLink>
        </nav>

        <button type="button" onClick={() => void logout()} className="mt-4 flex items-center gap-3 rounded-lg px-3 py-3 text-sm text-ig-muted hover:bg-ig-hover dark:hover:bg-darkig-hover">
          <X size={20} /> Log out
        </button>
      </aside>

      {sidebarOpen && <button type="button" aria-label="Close menu" className="fixed inset-0 z-30 bg-black/40 md:hidden" onClick={() => setSidebarOpen(false)} />}

      <div className="md:pl-[244px]">
        <header className="sticky top-0 z-20 hidden h-[60px] items-center gap-4 border-b border-ig-border bg-ig-panel/95 px-6 backdrop-blur dark:border-darkig-border dark:bg-darkig-panel/95 md:flex">
          <form onSubmit={submitSearch} className="relative w-full max-w-[268px]">
            <Search size={17} className="absolute left-3 top-1/2 -translate-y-1/2 text-ig-muted" />
            <input
              value={search}
              onChange={(event) => setSearch(event.target.value)}
              placeholder="Search"
              className="w-full rounded-lg bg-ig-hover py-2 pl-9 pr-3 text-sm outline-none placeholder:text-ig-muted dark:bg-darkig-hover"
            />
          </form>

          <div className="ml-auto flex items-center gap-2">
            <button type="button" onClick={toggleTheme} className="icon-btn" aria-label="Toggle theme">
              {theme === 'dark' ? <Sun size={22} /> : <Moon size={22} />}
            </button>
            <button type="button" onClick={() => navigate('/messages')} className="icon-btn relative" aria-label="Messages">
              <MessageCircle size={23} />
              {unreadMessages > 0 && <span className="absolute right-1 top-1 h-2.5 w-2.5 rounded-full bg-ig-accent ring-2 ring-ig-panel dark:ring-darkig-panel" />}
            </button>
            <button type="button" onClick={() => navigate('/notifications')} className="icon-btn relative" aria-label="Notifications">
              <Heart size={23} />
              {unread > 0 && <span className="absolute right-1 top-1 h-2.5 w-2.5 rounded-full bg-rose-500 ring-2 ring-ig-panel dark:ring-darkig-panel" />}
            </button>
            <div className="relative">
              <button type="button" onClick={() => setCreateOpen((v) => !v)} className="icon-btn" aria-label="Create">
                <Plus size={25} />
              </button>
              {createOpen && (
                <div className="absolute right-0 top-11 z-30 w-44 overflow-hidden rounded-xl border border-ig-border bg-ig-panel py-1 shadow-modal dark:border-darkig-border dark:bg-darkig-panel">
                  <button type="button" onClick={() => create('post')} className="flex w-full items-center gap-2 px-4 py-2.5 text-sm hover:bg-ig-hover dark:hover:bg-darkig-hover"><ImagePlus size={16} /> New post</button>
                  <button type="button" onClick={() => create('story')} className="flex w-full items-center gap-2 px-4 py-2.5 text-sm hover:bg-ig-hover dark:hover:bg-darkig-hover"><User size={16} /> New story</button>
                  <button type="button" onClick={() => create('reel')} className="flex w-full items-center gap-2 px-4 py-2.5 text-sm hover:bg-ig-hover dark:hover:bg-darkig-hover"><Film size={16} /> New reel</button>
                </div>
              )}
            </div>
            <button type="button" onClick={() => navigate(`/profile/${user?.id || ''}`)} className="icon-btn" aria-label="Profile">
              <Avatar src={user?.profilePicture} name={user?.username} size={28} />
            </button>
          </div>
        </header>

        <main className="mx-auto w-full max-w-[975px] px-0 pb-24 pt-0 md:px-6 md:pb-12 md:pt-6">
          <Outlet />
        </main>
      </div>

      <nav className="fixed inset-x-0 bottom-0 z-30 flex h-14 items-center justify-around border-t border-ig-border bg-ig-panel dark:border-darkig-border dark:bg-darkig-panel md:hidden">
        {navItems.slice(0, 5).map(({ to, label, icon: Icon }) => (
          <NavLink key={to} to={to} end={to === '/'} className={({ isActive }) => cx('icon-btn relative', isActive && 'text-ig-accent')} aria-label={label}>
            <Icon size={23} />
            {label === 'Messages' && unreadMessages > 0 && <span className="absolute right-1.5 top-1.5 h-2 w-2 rounded-full bg-ig-accent" />}
            {label === 'Notifications' && unread > 0 && <span className="absolute right-1.5 top-1.5 h-2 w-2 rounded-full bg-rose-500" />}
          </NavLink>
        ))}
        <button type="button" onClick={() => navigate(`/profile/${user?.id || ''}`)} className="icon-btn" aria-label="Profile">
          <Avatar src={user?.profilePicture} name={user?.username} size={24} />
        </button>
      </nav>
    </div>
  );
}
