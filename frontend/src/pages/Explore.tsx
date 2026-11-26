import { useCallback, useEffect, useState, type FormEvent } from 'react';
import { useSearchParams } from 'react-router-dom';
import { Heart, MessageCircle, Search, X } from 'lucide-react';
import Modal from '../components/Modal';
import PostCard from '../components/PostCard';
import Spinner from '../components/Spinner';
import { useAuth } from '../context/AuthContext';
import type { Hashtag, Post } from '../lib/types';
import { formatCount } from '../lib/utils';

export default function Explore() {
  const { request } = useAuth();
  const [searchParams, setSearchParams] = useSearchParams();
  const [tag, setTag] = useState(searchParams.get('tag') || '');
  const [input, setInput] = useState(tag);
  const [posts, setPosts] = useState<Post[]>([]);
  const [tags, setTags] = useState<Hashtag[]>([]);
  const [loading, setLoading] = useState(true);
  const [selected, setSelected] = useState<Post | null>(null);

  const load = useCallback(() => {
    setLoading(true);
    const query = tag ? `?tag=${encodeURIComponent(tag)}` : '';
    request<{ posts: Post[] }>(`/explore${query}`)
      .then((data) => setPosts(data.posts))
      .catch(() => setPosts([]))
      .finally(() => setLoading(false));
  }, [request, tag]);

  useEffect(() => { load(); }, [load]);

  useEffect(() => {
    request<{ hashtags: Hashtag[] }>('/hashtags/trending')
      .then((data) => setTags(data.hashtags))
      .catch(() => undefined);
  }, [request]);

  useEffect(() => {
    const urlTag = searchParams.get('tag') || '';
    setTag(urlTag);
    setInput(urlTag);
  }, [searchParams]);

  const search = (event: FormEvent) => {
    event.preventDefault();
    const next = input.trim().replace(/^#/, '');
    setTag(next);
    setSelected(null);
    const params = new URLSearchParams(searchParams);
    if (next) params.set('tag', next);
    else params.delete('tag');
    setSearchParams(params, { replace: true });
  };

  return (
    <div className="space-y-6">
      <form onSubmit={search} className="mx-auto flex max-w-xl gap-2">
        <div className="relative flex-1">
          <Search size={18} className="absolute left-3 top-1/2 -translate-y-1/2 text-ig-muted" />
          <input value={input} onChange={(event) => setInput(event.target.value)} placeholder="Search hashtags" className="input pl-10" />
        </div>
        <button type="submit" className="btn-primary">Search</button>
      </form>

      <div className="flex flex-wrap justify-center gap-2">
        {tags.slice(0, 10).map((item) => (
          <button
            key={item.tag}
            type="button"
            onClick={() => { setTag(item.tag); setInput(item.tag); setSearchParams({ tag: item.tag }); }}
            className={`rounded-full border px-4 py-2 text-sm transition ${tag === item.tag ? 'border-ig-accent bg-ig-accent text-white' : 'border-ig-border hover:bg-ig-hover dark:border-darkig-border dark:hover:bg-darkig-hover'}`}
          >
            #{item.tag} <span className="opacity-70">{formatCount(item.count)}</span>
          </button>
        ))}
      </div>

      {tag && (
        <div className="flex items-center justify-center gap-2 text-sm text-ig-muted">
          Showing <span className="font-semibold text-ig-text dark:text-darkig-text">#{tag}</span>
          <button type="button" onClick={() => { setTag(''); setInput(''); setSearchParams({}); }} className="icon-btn h-6 w-6"><X size={14} /></button>
        </div>
      )}

      {loading ? <Spinner label="Exploring…" /> : posts.length === 0 ? (
        <div className="card p-12 text-center text-sm text-ig-muted">No posts found. Try another hashtag.</div>
      ) : (
        <div className="grid grid-cols-1 gap-1 sm:grid-cols-2 md:grid-cols-3">
          {posts.map((post) => {
            const media = post.media[0];
            return (
              <button key={post.id} type="button" onClick={() => setSelected(post)} className="group relative aspect-square overflow-hidden bg-neutral-200 dark:bg-neutral-800">
                {media?.type === 'video' ? (
                  <video src={media.url} muted className="h-full w-full object-cover" />
                ) : (
                  <img src={media?.url} alt={post.caption} className="h-full w-full object-cover transition group-hover:scale-105" />
                )}
                <span className="absolute inset-0 flex items-center justify-center gap-6 bg-black/40 text-sm font-semibold text-white opacity-0 transition group-hover:opacity-100">
                  <span className="flex items-center gap-1"><Heart size={18} fill="white" /> {formatCount(post.likeCount)}</span>
                  <span className="flex items-center gap-1"><MessageCircle size={18} /> {formatCount(post.commentCount)}</span>
                </span>
              </button>
            );
          })}
        </div>
      )}

      <Modal open={!!selected} onClose={() => setSelected(null)} width="max-w-xl">
        {selected && <PostCard post={selected} compact onChanged={load} />}
      </Modal>

      <p className="text-center text-xs text-ig-muted">
        Discover posts through hashtags indexed in MySQL and ranked in C++.
      </p>
    </div>
  );
}
