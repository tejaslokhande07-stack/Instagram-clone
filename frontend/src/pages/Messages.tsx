import { useCallback, useEffect, useRef, useState, type FormEvent } from 'react';
import { useSearchParams } from 'react-router-dom';
import { ImagePlus, Search, Send, Smile } from 'lucide-react';
import Avatar from '../components/Avatar';
import Spinner from '../components/Spinner';
import { useAuth } from '../context/AuthContext';
import { useToast } from '../context/ToastContext';
import { fileToBase64, mediaUrl } from '../lib/api';
import type { Conversation, Message } from '../lib/types';
import { cx, timeAgo } from '../lib/utils';

export default function Messages() {
  const { user, request } = useAuth();
  const { showToast } = useToast();
  const [searchParams] = useSearchParams();
  const [conversations, setConversations] = useState<Conversation[]>([]);
  const [query, setQuery] = useState('');
  const [active, setActive] = useState<Conversation | null>(null);
  const [messages, setMessages] = useState<Message[]>([]);
  const [typing, setTyping] = useState(false);
  const [text, setText] = useState('');
  const [loading, setLoading] = useState(true);
  const [loadingMessages, setLoadingMessages] = useState(false);
  const [mobileChat, setMobileChat] = useState(false);
  const bottomRef = useRef<HTMLDivElement>(null);
  const typingTimer = useRef<number | null>(null);

  const loadConversations = useCallback(async () => {
    try {
      const suffix = query ? `?q=${encodeURIComponent(query)}` : '';
      const data = await request<{ conversations: Conversation[] }>(`/messages/conversations${suffix}`);
      setConversations(data.conversations);
      setActive((current) => current
        ? data.conversations.find((item) => item.id === current.id) || current
        : null);
    } catch {
      showToast('Could not load conversations', 'error');
    } finally {
      setLoading(false);
    }
  }, [request, query, showToast]);

  useEffect(() => { void loadConversations(); }, [loadConversations]);

  useEffect(() => {
    const timer = window.setInterval(() => void loadConversations(), 8000);
    return () => window.clearInterval(timer);
  }, [loadConversations]);

  useEffect(() => {
    const target = Number(searchParams.get('user') || 0);
    if (!target || !user) return;
    request<{ conversationId: number }>(`/messages/conversation/${target}`, {
      method: 'POST',
      body: { recipientId: target },
    }).then((data) => {
      const conversation = conversations.find((item) => item.id === data.conversationId);
      const fallback: Conversation = {
        id: data.conversationId,
        otherId: target,
        otherUsername: 'New conversation',
        otherAvatar: '',
        online: false,
        lastMessage: '',
        lastMessageAt: '',
        unreadCount: 0,
      };
      setActive(conversation || fallback);
      setMobileChat(true);
      void loadConversations();
    }).catch(() => showToast('Could not start conversation', 'error'));
    // Run once for the deep-link user; conversation changes are handled by selection.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [searchParams]);

  const loadMessages = useCallback(async (silent = false) => {
    if (!active) return;
    if (!silent) setLoadingMessages(true);
    try {
      const data = await request<{ messages: Message[]; typing: boolean }>(`/messages/${active.id}?limit=80`);
      setMessages(data.messages);
      setTyping(data.typing);
    } catch {
      if (!silent) showToast('Could not load messages', 'error');
    } finally {
      setLoadingMessages(false);
    }
  }, [active, request, showToast]);

  useEffect(() => {
    if (!active) { setMessages([]); return; }
    void loadMessages();
    void request(`/messages/${active.id}/read`, { method: 'POST' })
      .then(() => {
        window.dispatchEvent(new Event('ig:messages-changed'));
        void loadConversations();
      })
      .catch(() => undefined);
    const timer = window.setInterval(() => void loadMessages(true), 4000);
    return () => window.clearInterval(timer);
  }, [active, loadMessages, loadConversations, request]);

  useEffect(() => {
    bottomRef.current?.scrollIntoView({ behavior: 'smooth' });
  }, [messages, typing]);

  const notifyTyping = () => {
    if (!active) return;
    void request(`/messages/${active.id}/typing`, { method: 'POST', body: { typing: true } }).catch(() => undefined);
    if (typingTimer.current) window.clearTimeout(typingTimer.current);
    typingTimer.current = window.setTimeout(() => {
      if (active) void request(`/messages/${active.id}/typing`, { method: 'POST', body: { typing: false } }).catch(() => undefined);
    }, 2500);
  };

  const send = async (event: FormEvent, mediaData?: string, filename?: string) => {
    event.preventDefault();
    if (!active || (!text.trim() && !mediaData)) return;
    const body = text.trim();
    setText('');
    try {
      const data = await request<{ message: Message }>('/messages', {
        method: 'POST',
        body: { conversationId: active.id, body, mediaData, filename },
      });
      setMessages((current) => [...current, data.message]);
      void loadConversations();
      window.dispatchEvent(new Event('ig:messages-changed'));
    } catch (error) {
      showToast(error instanceof Error ? error.message : 'Could not send message', 'error');
    }
  };

  const attach = async (file?: File) => {
    if (!file || !active) return;
    const data = await fileToBase64(file);
    await send(new Event('submit') as unknown as FormEvent, data, file.name);
  };

  return (
    <div className="card h-[calc(100vh-150px)] min-h-[520px] overflow-hidden md:h-[calc(100vh-140px)]">
      <div className="grid h-full md:grid-cols-[320px_1fr]">
        <aside className={cx('flex h-full flex-col border-r border-ig-border dark:border-darkig-border', mobileChat && 'hidden md:flex')}>
          <div className="border-b border-ig-border p-4 dark:border-darkig-border">
            <h1 className="mb-3 text-xl font-bold">Messages</h1>
            <div className="relative">
              <Search size={16} className="absolute left-3 top-1/2 -translate-y-1/2 text-ig-muted" />
              <input value={query} onChange={(event) => setQuery(event.target.value)} placeholder="Search messages" className="input py-2 pl-9" />
            </div>
          </div>
          <div className="flex-1 overflow-y-auto">
            {loading ? <Spinner /> : conversations.length === 0 ? (
              <p className="p-8 text-center text-sm text-ig-muted">No conversations yet. Message someone from their profile.</p>
            ) : conversations.map((conversation) => (
              <button
                key={conversation.id}
                type="button"
                onClick={() => { setActive(conversation); setMobileChat(true); }}
                className={cx('flex w-full items-center gap-3 px-4 py-3 text-left transition hover:bg-ig-hover dark:hover:bg-darkig-hover', active?.id === conversation.id && 'bg-ig-hover dark:bg-darkig-hover')}
              >
                <div className="relative">
                  <Avatar src={conversation.otherAvatar} name={conversation.otherUsername} size={48} />
                  {conversation.online && <span className="absolute bottom-0 right-0 h-3 w-3 rounded-full border-2 border-ig-panel bg-emerald-500 dark:border-darkig-panel" />}
                </div>
                <div className="min-w-0 flex-1">
                  <div className="flex items-center justify-between gap-2">
                    <span className="truncate text-sm font-semibold">{conversation.otherUsername}</span>
                    <span className="shrink-0 text-[10px] text-ig-muted">{timeAgo(conversation.lastMessageAt)}</span>
                  </div>
                  <div className="flex items-center justify-between gap-2">
                    <span className="truncate text-xs text-ig-muted dark:text-darkig-muted">{conversation.lastMessage || 'Start a conversation'}</span>
                    {conversation.unreadCount > 0 && <span className="flex h-5 min-w-5 items-center justify-center rounded-full bg-ig-accent px-1 text-[10px] font-bold text-white">{conversation.unreadCount}</span>}
                  </div>
                </div>
              </button>
            ))}
          </div>
        </aside>

        <section className={cx('h-full flex-col', mobileChat ? 'flex' : 'hidden md:flex')}>
          {!active ? (
            <div className="flex h-full flex-col items-center justify-center gap-3 text-center">
              <Send size={44} strokeWidth={1} className="text-ig-muted" />
              <p className="font-semibold">Select a conversation</p>
              <p className="max-w-xs text-sm text-ig-muted">Your direct messages appear here. Polling keeps them fresh without a WebSocket server.</p>
            </div>
          ) : (
            <>
              <header className="flex items-center gap-3 border-b border-ig-border px-4 py-3 dark:border-darkig-border">
                <button type="button" onClick={() => setMobileChat(false)} className="icon-btn md:hidden" aria-label="Back">←</button>
                <Avatar src={active.otherAvatar} name={active.otherUsername} size={36} />
                <div>
                  <div className="text-sm font-semibold">{active.otherUsername}</div>
                  <div className="text-xs text-ig-muted">{active.online ? 'Active now' : 'Offline'}</div>
                </div>
              </header>

              <div className="flex-1 space-y-3 overflow-y-auto bg-ig-bg px-4 py-5 dark:bg-darkig-bg">
                {loadingMessages ? <Spinner /> : messages.length === 0 && (
                  <p className="mt-10 text-center text-sm text-ig-muted">Say hello 👋</p>
                )}
                {messages.map((message) => {
                  const mine = message.senderId === user?.id;
                  return (
                    <div key={message.id} className={cx('flex gap-2', mine ? 'justify-end' : 'justify-start')}>
                      {!mine && <Avatar src={active.otherAvatar} name={active.otherUsername} size={28} />}
                      <div className={cx('max-w-[75%] rounded-2xl px-4 py-2 text-sm', mine ? 'rounded-br-sm bg-ig-accent text-white' : 'rounded-bl-sm bg-ig-panel dark:bg-darkig-panel')}>
                        {message.mediaUrl && <img src={mediaUrl(message.mediaUrl)} alt="Attachment" className="mb-2 max-h-56 rounded-lg object-cover" />}
                        {message.body && <p className="whitespace-pre-wrap break-words">{message.body}</p>}
                        <p className={cx('mt-1 text-[10px]', mine ? 'text-white/70' : 'text-ig-muted')}>{timeAgo(message.createdAt)}</p>
                      </div>
                    </div>
                  );
                })}
                {typing && (
                  <div className="flex items-center gap-2 text-xs text-ig-muted">
                    <Avatar src={active.otherAvatar} name={active.otherUsername} size={24} />
                    <span className="rounded-full bg-ig-panel px-3 py-2 dark:bg-darkig-panel">typing…</span>
                  </div>
                )}
                <div ref={bottomRef} />
              </div>

              <form onSubmit={(event) => void send(event)} className="flex items-center gap-2 border-t border-ig-border p-3 dark:border-darkig-border">
                <label className="icon-btn cursor-pointer" aria-label="Attach image">
                  <ImagePlus size={21} />
                  <input type="file" accept="image/*" hidden onChange={(event) => void attach(event.target.files?.[0])} />
                </label>
                <button type="button" className="icon-btn" aria-label="Emoji" onClick={() => setText((value) => value + ' 😊')}>
                  <Smile size={21} />
                </button>
                <input value={text} onChange={(event) => { setText(event.target.value); notifyTyping(); }} placeholder="Message…" className="input flex-1 py-2" />
                <button type="submit" disabled={!text.trim()} className="text-sm font-semibold text-ig-accent disabled:opacity-40">Send</button>
              </form>
            </>
          )}
        </section>
      </div>
    </div>
  );
}
