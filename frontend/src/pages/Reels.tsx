import { useCallback, useEffect, useRef, useState, type FormEvent } from 'react';
import { useSearchParams } from 'react-router-dom';
import { Bookmark, Film, Heart, MessageCircle, Play, Send, Volume2, VolumeX, X } from 'lucide-react';
import Avatar from '../components/Avatar';
import Modal from '../components/Modal';
import Spinner from '../components/Spinner';
import { useAuth } from '../context/AuthContext';
import { useToast } from '../context/ToastContext';
import { fileToBase64, mediaUrl } from '../lib/api';
import type { Comment, Reel } from '../lib/types';
import { cx, formatCount, timeAgo } from '../lib/utils';

export default function Reels() {
  const { request } = useAuth();
  const { showToast } = useToast();
  const [searchParams, setSearchParams] = useSearchParams();
  const [mode, setMode] = useState<'latest' | 'trending' | 'saved'>('latest');
  const [reels, setReels] = useState<Reel[]>([]);
  const [loading, setLoading] = useState(true);
  const [muted, setMuted] = useState(true);
  const [viewed, setViewed] = useState<Set<number>>(new Set());
  const [commentsFor, setCommentsFor] = useState<Reel | null>(null);
  const [comments, setComments] = useState<Comment[]>([]);
  const [commentText, setCommentText] = useState('');
  const [createOpen, setCreateOpen] = useState(false);
  const [videoFile, setVideoFile] = useState<File | null>(null);
  const [thumbFile, setThumbFile] = useState<File | null>(null);
  const [caption, setCaption] = useState('');
  const [busy, setBusy] = useState(false);
  const fileRef = useRef<HTMLInputElement>(null);

  const load = useCallback(() => {
    setLoading(true);
    const path = mode === 'saved' ? '/reels/saved' : `/reels?mode=${mode}&limit=12`;
    request<{ reels: Reel[] }>(path)
      .then((data) => setReels(data.reels))
      .catch(() => showToast('Could not load reels', 'error'))
      .finally(() => setLoading(false));
  }, [request, mode, showToast]);

  useEffect(() => { load(); }, [load]);

  useEffect(() => {
    if (searchParams.get('create') === 'reel') setCreateOpen(true);
  }, [searchParams]);

  const closeCreate = () => {
    setCreateOpen(false);
    setVideoFile(null);
    setThumbFile(null);
    setCaption('');
    if (searchParams.get('create') === 'reel') {
      const next = new URLSearchParams(searchParams);
      next.delete('create');
      setSearchParams(next, { replace: true });
    }
  };

  const registerView = async (id: number) => {
    if (viewed.has(id)) return;
    setViewed((current) => new Set(current).add(id));
    try {
      await request(`/reels/${id}/view`, { method: 'POST' });
      setReels((current) => current.map((reel) => reel.id === id ? { ...reel, views: reel.views + 1 } : reel));
    } catch { /* non-critical */ }
  };

  const toggleLike = async (reel: Reel) => {
    const next = !reel.likedByViewer;
    setReels((current) => current.map((item) => item.id === reel.id
      ? { ...item, likedByViewer: next, likes: Math.max(0, item.likes + (next ? 1 : -1)) }
      : item));
    try {
      await request(`/reels/${reel.id}/like`, { method: next ? 'POST' : 'DELETE' });
    } catch {
      showToast('Could not update like', 'error');
    }
  };

  const toggleSave = async (reel: Reel) => {
    const next = !reel.savedByViewer;
    setReels((current) => mode === 'saved' && !next
      ? current.filter((item) => item.id !== reel.id)
      : current.map((item) => item.id === reel.id ? { ...item, savedByViewer: next } : item));
    try {
      await request(`/reels/${reel.id}/save`, { method: next ? 'POST' : 'DELETE' });
      showToast(next ? 'Reel saved' : 'Reel removed from saved');
    } catch {
      showToast('Could not update saved reels', 'error');
      void load();
    }
  };

  const openComments = async (reel: Reel) => {
    setCommentsFor(reel);
    try {
      const data = await request<{ comments: Comment[] }>(`/reels/${reel.id}/comments`);
      setComments(data.comments);
    } catch {
      setComments([]);
    }
  };

  const addComment = async (event: FormEvent) => {
    event.preventDefault();
    if (!commentsFor || !commentText.trim()) return;
    try {
      await request(`/reels/${commentsFor.id}/comments`, { method: 'POST', body: { body: commentText.trim() } });
      setComments((current) => [...current, {
        id: Date.now(), userId: 0, username: 'you', avatar: '', body: commentText.trim(), createdAt: new Date().toISOString(),
      }]);
      setReels((current) => current.map((reel) => reel.id === commentsFor.id ? { ...reel, comments: reel.comments + 1 } : reel));
      setCommentText('');
    } catch {
      showToast('Could not add comment', 'error');
    }
  };

  const submitReel = async () => {
    if (!videoFile || busy) return;
    setBusy(true);
    try {
      const body: Record<string, unknown> = {
        data: await fileToBase64(videoFile),
        filename: videoFile.name,
        caption,
      };
      if (thumbFile) {
        body.thumbnailData = await fileToBase64(thumbFile);
        body.thumbnailFilename = thumbFile.name;
      }
      await request('/reels', { method: 'POST', body });
      showToast('Reel published');
      closeCreate();
      load();
    } catch (error) {
      showToast(error instanceof Error ? error.message : 'Could not publish reel', 'error');
    } finally {
      setBusy(false);
    }
  };

  return (
    <div className="space-y-5">
      <div className="flex items-center justify-between">
        <div>
          <h1 className="text-xl font-bold">Reels</h1>
          <p className="text-xs text-ig-muted dark:text-darkig-muted">Short videos, ranked with a custom max-heap</p>
        </div>
        <div className="flex items-center gap-2">
          <div className="flex rounded-full border border-ig-border p-1 dark:border-darkig-border">
            <button type="button" onClick={() => setMode('latest')} className={cx('rounded-full px-4 py-1.5 text-xs font-semibold', mode === 'latest' ? 'bg-ig-text text-white dark:bg-white dark:text-black' : '')}>Latest</button>
            <button type="button" onClick={() => setMode('trending')} className={cx('rounded-full px-4 py-1.5 text-xs font-semibold', mode === 'trending' ? 'bg-ig-text text-white dark:bg-white dark:text-black' : '')}>Trending</button>
            <button type="button" onClick={() => setMode('saved')} className={cx('rounded-full px-4 py-1.5 text-xs font-semibold', mode === 'saved' ? 'bg-ig-text text-white dark:bg-white dark:text-black' : '')}>Saved</button>
          </div>
          <button type="button" onClick={() => setCreateOpen(true)} className="btn-primary py-2">New reel</button>
        </div>
      </div>

      {loading ? <Spinner label="Loading reels…" /> : reels.length === 0 ? (
        <div className="card flex flex-col items-center gap-3 p-16 text-center">
          <Film size={44} strokeWidth={1.2} className="text-ig-muted" />
          <p className="font-semibold">{mode === 'saved' ? 'No saved reels yet' : 'No reels yet'}</p>
          {mode === 'saved' ? (
            <button type="button" onClick={() => setMode('latest')} className="btn-primary">Browse reels</button>
          ) : (
            <button type="button" onClick={() => setCreateOpen(true)} className="btn-primary">Upload the first one</button>
          )}
        </div>
      ) : (
        <div className="no-scrollbar flex snap-y snap-mandatory flex-col items-center gap-6 overflow-y-auto" style={{ maxHeight: 'calc(100vh - 150px)' }}>
          {reels.map((reel) => (
            <article key={reel.id} className="relative aspect-[9/14] w-full max-w-[420px] snap-center overflow-hidden rounded-2xl bg-black shadow-card">
              <video
                src={mediaUrl(reel.videoUrl)}
                poster={mediaUrl(reel.thumbnailUrl)}
                autoPlay
                loop
                muted={muted}
                playsInline
                onPlay={() => void registerView(reel.id)}
                className="h-full w-full object-cover"
              />
              <div className="pointer-events-none absolute inset-x-0 bottom-0 h-1/2 bg-gradient-to-t from-black/85 to-transparent" />

              <button type="button" onClick={() => setMuted((value) => !value)} className="absolute right-3 top-3 rounded-full bg-black/40 p-2 text-white" aria-label="Toggle sound">
                {muted ? <VolumeX size={18} /> : <Volume2 size={18} />}
              </button>

              <div className="absolute bottom-5 right-3 flex flex-col items-center gap-5 text-white">
                <button type="button" onClick={() => void toggleLike(reel)} className="flex flex-col items-center gap-1" aria-label="Like reel">
                  <Heart size={30} fill={reel.likedByViewer ? '#ed4956' : 'none'} className={reel.likedByViewer ? 'text-[#ed4956]' : ''} />
                  <span className="text-xs">{formatCount(reel.likes)}</span>
                </button>
                <button type="button" onClick={() => void openComments(reel)} className="flex flex-col items-center gap-1" aria-label="Comments">
                  <MessageCircle size={30} />
                  <span className="text-xs">{formatCount(reel.comments)}</span>
                </button>
                <button type="button" onClick={() => void toggleSave(reel)} className="flex flex-col items-center gap-1" aria-label="Save reel">
                  <Bookmark size={28} fill={reel.savedByViewer ? 'white' : 'none'} />
                  <span className="text-xs">Save</span>
                </button>
                <button type="button" onClick={() => void navigator.clipboard?.writeText(`${window.location.origin}/reels?reel=${reel.id}`)} className="flex flex-col items-center gap-1" aria-label="Share reel">
                  <Send size={27} />
                  <span className="text-xs">Share</span>
                </button>
              </div>

              <div className="absolute bottom-6 left-4 max-w-[70%] text-white">
                <div className="flex items-center gap-2">
                  <Avatar src={reel.avatar} name={reel.username} size={32} />
                  <span className="text-sm font-semibold">@{reel.username}</span>
                </div>
                <p className="mt-2 line-clamp-3 text-sm">{reel.caption}</p>
                <p className="mt-1 flex items-center gap-1 text-xs text-white/70"><Play size={11} fill="white" /> {formatCount(reel.views)} views · {timeAgo(reel.createdAt)}</p>
              </div>
            </article>
          ))}
        </div>
      )}

      <Modal open={!!commentsFor} onClose={() => setCommentsFor(null)} title="Comments" width="max-w-lg">
        <div className="flex h-[60vh] flex-col">
          <div className="flex-1 space-y-4 overflow-y-auto p-5">
            {comments.length === 0 && <p className="pt-10 text-center text-sm text-ig-muted">No comments yet.</p>}
            {comments.map((comment) => (
              <div key={comment.id} className="flex gap-3">
                <Avatar src={comment.avatar} name={comment.username} size={32} />
                <p className="text-sm"><span className="font-semibold">{comment.username}</span> {comment.body}</p>
              </div>
            ))}
          </div>
          <form onSubmit={(event) => void addComment(event)} className="flex gap-2 border-t border-ig-border p-3 dark:border-darkig-border">
            <input value={commentText} onChange={(event) => setCommentText(event.target.value)} placeholder="Add a comment…" className="input flex-1 py-2" />
            <button type="submit" disabled={!commentText.trim()} className="text-sm font-semibold text-ig-accent disabled:opacity-40">Post</button>
          </form>
        </div>
      </Modal>

      <Modal open={createOpen} onClose={closeCreate} title="Create reel" width="max-w-lg">
        <div className="space-y-4 p-5">
          <input ref={fileRef} type="file" accept="video/*" hidden onChange={(event) => setVideoFile(event.target.files?.[0] || null)} />
          <input type="file" accept="image/*" hidden id="reel-thumb" onChange={(event) => setThumbFile(event.target.files?.[0] || null)} />
          <button type="button" onClick={() => fileRef.current?.click()} className="flex h-44 w-full flex-col items-center justify-center gap-2 rounded-xl border-2 border-dashed border-ig-border text-ig-muted dark:border-darkig-border">
            <Film size={36} strokeWidth={1.2} />
            <span>{videoFile ? videoFile.name : 'Choose a video'}</span>
          </button>
          <label className="block text-sm">
            <span className="mb-1 block font-semibold">Thumbnail (optional)</span>
            <label htmlFor="reel-thumb" className="btn-secondary inline-block cursor-pointer">
              {thumbFile ? thumbFile.name : 'Choose image'}
            </label>
          </label>
          <textarea value={caption} onChange={(event) => setCaption(event.target.value)} placeholder="Caption… #reels" className="input min-h-[90px]" />
          <button type="button" disabled={!videoFile || busy} onClick={() => void submitReel()} className="btn-primary w-full">
            {busy ? 'Publishing…' : 'Publish reel'}
          </button>
          <button type="button" onClick={closeCreate} className="mx-auto flex items-center gap-1 text-xs text-ig-muted"><X size={13} /> Cancel</button>
        </div>
      </Modal>
    </div>
  );
}
