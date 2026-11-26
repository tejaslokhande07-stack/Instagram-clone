import { useEffect, useMemo, useState } from 'react';
import { Link, useNavigate } from 'react-router-dom';
import {
  Bookmark, ChevronLeft, ChevronRight, Heart, MapPin, MessageCircle,
  MoreHorizontal, Send, Trash2, Flag, X,
} from 'lucide-react';
import Avatar from './Avatar';
import Modal from './Modal';
import Spinner from './Spinner';
import { useAuth } from '../context/AuthContext';
import { useToast } from '../context/ToastContext';
import { mediaUrl } from '../lib/api';
import type { Comment, Post } from '../lib/types';
import { cx, formatCount, timeAgo } from '../lib/utils';

interface PostCardProps {
  post: Post;
  onChanged?: () => void;
  onDeleted?: () => void;
  compact?: boolean;
}

interface LikeResponse {
  postId: number;
  liked: boolean;
  likeCount: number;
}

interface SaveResponse {
  postId: number;
  saved: boolean;
}

interface CommentListResponse {
  comments: Comment[];
  postId: number;
  commentCount: number;
}

interface CommentMutationResponse {
  comment: Comment;
  postId: number;
  commentCount: number;
}

interface CommentDeleteResponse {
  postId: number;
  commentCount: number;
}

export default function PostCard({ post, onChanged, onDeleted, compact = false }: PostCardProps) {
  const { user, request } = useAuth();
  const { showToast } = useToast();
  const navigate = useNavigate();
  const [liked, setLiked] = useState(post.likedByViewer);
  const [saved, setSaved] = useState(post.savedByViewer);
  const [likeCount, setLikeCount] = useState(post.likeCount);
  const [commentCount, setCommentCount] = useState(post.commentCount);
  const [burst, setBurst] = useState(false);
  const [slide, setSlide] = useState(0);
  const [commentsOpen, setCommentsOpen] = useState(false);
  const [comments, setComments] = useState<Comment[]>([]);
  const [commentsLoading, setCommentsLoading] = useState(false);
  const [commentsError, setCommentsError] = useState(false);
  const [commentText, setCommentText] = useState('');
  const [menuOpen, setMenuOpen] = useState(false);
  const [reportOpen, setReportOpen] = useState(false);
  const [reason, setReason] = useState('Spam');
  const [busy, setBusy] = useState(false);
  const [likeBusy, setLikeBusy] = useState(false);
  const [saveBusy, setSaveBusy] = useState(false);

  useEffect(() => {
    setLiked(post.likedByViewer);
    setSaved(post.savedByViewer);
    setLikeCount(post.likeCount);
    setCommentCount(post.commentCount);
  }, [post]);

  const captionParts = useMemo(() => {
    const pieces = post.caption.split(/(#[\w]+)/g).filter(Boolean);
    return pieces;
  }, [post.caption]);

  const toggleLike = async (force?: boolean) => {
    if (likeBusy) return;
    const next = force ?? !liked;
    setLikeBusy(true);
    setLiked(next);
    setLikeCount((value) => Math.max(0, value + (next ? 1 : -1)));
    if (next) {
      setBurst(true);
      window.setTimeout(() => setBurst(false), 700);
    }
    try {
      const data = await request<LikeResponse>(`/posts/${post.id}/like`, {
        method: next ? 'POST' : 'DELETE',
      });
      setLiked(data.liked);
      setLikeCount(data.likeCount);
      onChanged?.();
    } catch {
      setLiked(!next);
      setLikeCount((value) => Math.max(0, value + (next ? -1 : 1)));
      showToast('Could not update like', 'error');
    } finally {
      setLikeBusy(false);
    }
  };

  const toggleSave = async () => {
    if (saveBusy) return;
    const next = !saved;
    setSaveBusy(true);
    setSaved(next);
    try {
      const data = await request<SaveResponse>(`/posts/${post.id}/save`, {
        method: next ? 'POST' : 'DELETE',
      });
      setSaved(data.saved);
      showToast(next ? 'Saved to your collection' : 'Removed from saved');
    } catch {
      setSaved(!saved);
      showToast('Could not update saved posts', 'error');
    } finally {
      setSaveBusy(false);
    }
  };

  const loadComments = async () => {
    setCommentsLoading(true);
    setCommentsError(false);
    try {
      const data = await request<CommentListResponse>(`/posts/${post.id}/comments`);
      setComments(data.comments);
      setCommentCount(data.commentCount);
    } catch {
      setComments([]);
      setCommentsError(true);
    } finally {
      setCommentsLoading(false);
    }
  };

  const openComments = () => {
    setCommentsOpen(true);
    void loadComments();
  };

  const addComment = async () => {
    if (!commentText.trim() || busy || commentsLoading || commentsError) return;
    setBusy(true);
    try {
      const data = await request<CommentMutationResponse>(`/posts/${post.id}/comments`, {
        method: 'POST',
        body: { body: commentText.trim() },
      });
      setComments((current) => [...current, data.comment]);
      setCommentCount(data.commentCount);
      setCommentText('');
      onChanged?.();
    } catch {
      showToast('Could not post comment', 'error');
    } finally {
      setBusy(false);
    }
  };

  const deleteComment = async (commentId: number) => {
    try {
      const data = await request<CommentDeleteResponse>(`/comments/${commentId}`, {
        method: 'DELETE',
      });
      setComments((current) => current.filter((item) => item.id !== commentId));
      setCommentCount(data.commentCount);
      onChanged?.();
    } catch {
      showToast('Could not delete comment', 'error');
    }
  };

  const share = async () => {
    const url = `${window.location.origin}/post/${post.id}`;
    try {
      if (navigator.share) await navigator.share({ title: `@${post.username} on Instagram`, url });
      else {
        await navigator.clipboard.writeText(url);
        showToast('Link copied to clipboard');
      }
    } catch {
      // User cancelled the native share sheet.
    }
  };

  const deletePost = async () => {
    setMenuOpen(false);
    try {
      await request(`/posts/${post.id}`, { method: 'DELETE' });
      showToast('Post deleted');
      onDeleted?.();
      onChanged?.();
    } catch {
      showToast('Could not delete post', 'error');
    }
  };

  const submitReport = async () => {
    try {
      await request('/users/report', {
        method: 'POST',
        body: { userId: post.userId, postId: post.id, reason, details: 'Reported from post menu' },
      });
      showToast('Report submitted');
      setReportOpen(false);
    } catch {
      showToast('Could not submit report', 'error');
    }
  };

  const isOwn = user?.id === post.userId;
  const media = post.media[slide];

  return (
    <article className={cx('card overflow-hidden', compact && 'rounded-none border-x-0 shadow-none')}>
      <header className="flex items-center gap-3 px-4 py-3">
        <button type="button" onClick={() => navigate(`/profile/${post.userId}`)}>
          <Avatar src={post.avatar} name={post.username} size={40} ring />
        </button>
        <div className="min-w-0 flex-1">
          <div className="flex items-center gap-1">
            <Link to={`/profile/${post.userId}`} className="truncate text-sm font-semibold hover:opacity-70">
              {post.username}
            </Link>
            {post.userId === 1 && <span className="text-xs text-ig-accent">✓</span>}
            <span className="text-ig-muted">·</span>
            <span className="text-xs text-ig-muted dark:text-darkig-muted">{timeAgo(post.createdAt)}</span>
          </div>
          {post.location && (
            <div className="flex items-center gap-1 text-[11px] text-ig-muted dark:text-darkig-muted">
              <MapPin size={11} /> {post.location}
            </div>
          )}
        </div>
        <div className="relative">
          <button type="button" className="icon-btn" onClick={() => setMenuOpen((v) => !v)} aria-label="More options">
            <MoreHorizontal size={21} />
          </button>
          {menuOpen && (
            <div className="absolute right-0 top-10 z-20 w-44 overflow-hidden rounded-xl border border-ig-border bg-ig-panel py-1 shadow-modal dark:border-darkig-border dark:bg-darkig-panel">
              {isOwn ? (
                <button type="button" onClick={deletePost} className="flex w-full items-center gap-2 px-4 py-2.5 text-sm text-rose-500 hover:bg-ig-hover dark:hover:bg-darkig-hover">
                  <Trash2 size={16} /> Delete post
                </button>
              ) : (
                <button type="button" onClick={() => { setMenuOpen(false); setReportOpen(true); }} className="flex w-full items-center gap-2 px-4 py-2.5 text-sm hover:bg-ig-hover dark:hover:bg-darkig-hover">
                  <Flag size={16} /> Report post
                </button>
              )}
              <button type="button" onClick={() => { setMenuOpen(false); void share(); }} className="flex w-full items-center gap-2 px-4 py-2.5 text-sm hover:bg-ig-hover dark:hover:bg-darkig-hover">
                <Send size={16} /> Copy link
              </button>
            </div>
          )}
        </div>
      </header>

      <div className="group relative aspect-[4/5] w-full overflow-hidden bg-black sm:aspect-[4/4.2]">
        {media?.type === 'video' ? (
          <video src={mediaUrl(media.url)} controls loop playsInline className="h-full w-full object-cover" />
        ) : (
          <img
            src={mediaUrl(media?.url || '')}
            alt={post.caption || 'Post'}
            className="h-full w-full object-cover"
            onDoubleClick={() => void toggleLike(true)}
          />
        )}
        {burst && <Heart size={90} fill="white" className="heart-pop pointer-events-none absolute inset-0 m-auto text-white drop-shadow-lg" />}
        {post.media.length > 1 && (
          <>
            <button type="button" onClick={() => setSlide((s) => (s - 1 + post.media.length) % post.media.length)} className="absolute left-2 top-1/2 hidden -translate-y-1/2 rounded-full bg-black/40 p-1.5 text-white group-hover:block" aria-label="Previous media">
              <ChevronLeft size={20} />
            </button>
            <button type="button" onClick={() => setSlide((s) => (s + 1) % post.media.length)} className="absolute right-2 top-1/2 hidden -translate-y-1/2 rounded-full bg-black/40 p-1.5 text-white group-hover:block" aria-label="Next media">
              <ChevronRight size={20} />
            </button>
            <div className="absolute bottom-3 left-1/2 flex -translate-x-1/2 gap-1.5">
              {post.media.map((item) => (
                <span key={item.id} className={cx('h-1.5 w-1.5 rounded-full', item.id === media?.id ? 'bg-white' : 'bg-white/50')} />
              ))}
            </div>
          </>
        )}
      </div>

      <div className="px-4 pb-4 pt-3">
        <div className="flex items-center gap-1">
          <button type="button" onClick={() => void toggleLike()} className="icon-btn -ml-2" aria-label={liked ? 'Unlike' : 'Like'}>
            <Heart size={24} fill={liked ? '#ed4956' : 'none'} className={liked ? 'text-[#ed4956]' : ''} />
          </button>
          <button type="button" onClick={openComments} className="icon-btn -ml-1" aria-label="Comments">
            <MessageCircle size={24} />
          </button>
          <button type="button" onClick={() => void share()} className="icon-btn" aria-label="Share">
            <Send size={22} />
          </button>
          <button type="button" onClick={() => void toggleSave()} className="icon-btn ml-auto" aria-label={saved ? 'Unsave' : 'Save'}>
            <Bookmark size={23} fill={saved ? 'currentColor' : 'none'} />
          </button>
        </div>

        <div className="mt-1 text-sm font-semibold">{formatCount(likeCount)} likes</div>
        {post.caption && (
          <p className="mt-1.5 line-clamp-3 text-sm leading-5">
            <Link to={`/profile/${post.userId}`} className="mr-1 font-semibold hover:opacity-70">{post.username}</Link>
            {captionParts.map((part, index) => (
              <span key={`${part}-${index}`} className={part.startsWith('#') ? 'text-ig-accent' : undefined}>
                {part}{' '}
              </span>
            ))}
          </p>
        )}
        {post.hashtags.length > 0 && (
          <p className="mt-1 text-xs text-ig-muted dark:text-darkig-muted">
            {post.hashtags.map((tag) => `#${tag}`).join(' ')}
          </p>
        )}
        {commentCount > 0 && (
          <button type="button" onClick={openComments} className="mt-2 block text-sm text-ig-muted hover:opacity-70 dark:text-darkig-muted">
            View all {commentCount} comments
          </button>
        )}
      </div>

      <Modal open={commentsOpen} onClose={() => setCommentsOpen(false)} title="Comments" width="max-w-xl">
        <div className="flex h-[min(65vh,520px)] flex-col">
          <div className="flex-1 space-y-4 overflow-y-auto px-5 py-4">
            {commentsLoading ? (
              <Spinner label="Loading comments…" />
            ) : commentsError ? (
              <div className="py-10 text-center text-sm">
                <p className="text-ig-muted">Could not load comments.</p>
                <button type="button" onClick={() => void loadComments()} className="mt-2 font-semibold text-ig-accent">Try again</button>
              </div>
            ) : comments.length === 0 ? (
              <p className="py-10 text-center text-sm text-ig-muted">No comments yet. Start the conversation.</p>
            ) : comments.map((comment) => (
              <div key={comment.id} className="flex gap-3">
                <Avatar src={comment.avatar} name={comment.username} size={34} />
                <div className="min-w-0 flex-1 text-sm">
                  <p><span className="font-semibold">{comment.username}</span> {comment.body}</p>
                  <p className="mt-1 text-xs text-ig-muted">{timeAgo(comment.createdAt)}</p>
                </div>
                {comment.userId === user?.id && (
                  <button type="button" onClick={() => void deleteComment(comment.id)} className="text-ig-muted hover:text-rose-500" aria-label="Delete comment">
                    <X size={16} />
                  </button>
                )}
              </div>
            ))}
          </div>
          <div className="flex items-center gap-2 border-t border-ig-border p-3 dark:border-darkig-border">
            <Avatar src={user?.profilePicture} name={user?.username} size={34} />
            <input
              value={commentText}
              onChange={(event) => setCommentText(event.target.value)}
              onKeyDown={(event) => { if (event.key === 'Enter') void addComment(); }}
              placeholder="Add a comment…"
              maxLength={1000}
              disabled={commentsLoading || commentsError}
              className="input flex-1 py-2"
            />
            <button type="button" onClick={() => void addComment()} disabled={!commentText.trim() || busy || commentsLoading || commentsError} className="text-sm font-semibold text-ig-accent disabled:opacity-40">
              Post
            </button>
          </div>
        </div>
      </Modal>

      <Modal open={reportOpen} onClose={() => setReportOpen(false)} title="Report post" width="max-w-md">
        <div className="space-y-4 p-5">
          <p className="text-sm text-ig-muted dark:text-darkig-muted">Tell us what is wrong with this post.</p>
          <select value={reason} onChange={(event) => setReason(event.target.value)} className="input">
            <option>Spam</option>
            <option>Harassment</option>
            <option>Inappropriate content</option>
            <option>Misinformation</option>
            <option>Other</option>
          </select>
          <button type="button" onClick={() => void submitReport()} className="btn-primary w-full">Submit report</button>
        </div>
      </Modal>
    </article>
  );
}
