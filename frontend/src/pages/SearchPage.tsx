import { useCallback, useEffect, useState, type FormEvent } from 'react';
import { Link, useNavigate, useSearchParams } from 'react-router-dom';
import { Clock, Hash, Search as SearchIcon, User } from 'lucide-react';
import Avatar from '../components/Avatar';
import Spinner from '../components/Spinner';
import { useAuth } from '../context/AuthContext';
import type { Hashtag, Post, SearchResults, User as IgUser } from '../lib/types';
import { formatCount } from '../lib/utils';

export default function SearchPage() {
  const { request } = useAuth();
  const navigate = useNavigate();
  const [searchParams, setSearchParams] = useSearchParams();
  const [query, setQuery] = useState(searchParams.get('q') || '');
  const [results, setResults] = useState<SearchResults>({ users: [], hashtags: [], posts: [] });
  const [recent, setRecent] = useState<string[]>([]);
  const [trending, setTrending] = useState<Hashtag[]>([]);
  const [error, setError] = useState('');
  const [loading, setLoading] = useState(false);

  const loadRecent = useCallback(() => {
    request<{ queries: string[] }>('/search/recent')
      .then((data) => setRecent(data.queries.filter((value, index, list) => list.indexOf(value) === index)))
      .catch(() => undefined);
  }, [request]);

  const loadTrending = useCallback(() => {
    request<{ hashtags: Hashtag[] }>('/hashtags/trending')
      .then((data) => setTrending(data.hashtags.slice(0, 8)))
      .catch(() => undefined);
  }, [request]);

  useEffect(() => { loadRecent(); loadTrending(); }, [loadRecent, loadTrending]);

  useEffect(() => {
    const q = searchParams.get('q') || '';
    setQuery(q);
    if (!q) {
      setResults({ users: [], hashtags: [], posts: [] });
      return;
    }
    let active = true;
    setLoading(true);
    setError('');
    const timer = window.setTimeout(() => {
      request<SearchResults>(`/search?q=${encodeURIComponent(q)}`)
        .then((data) => { if (active) setResults(data); })
        .catch(() => { if (active) setError('Search failed. Please try again.'); })
        .finally(() => { if (active) setLoading(false); loadRecent(); });
    }, 220);
    return () => { active = false; window.clearTimeout(timer); };
  }, [searchParams, request, loadRecent]);

  const submit = (event: FormEvent) => {
    event.preventDefault();
    const value = query.trim().replace(/^#/, '');
    setQuery(value);
    setSearchParams(value ? { q: value } : {});
  };

  return (
    <div className="mx-auto max-w-2xl space-y-5">
      <form onSubmit={submit}>
        <div className="relative">
          <SearchIcon size={18} className="absolute left-3 top-1/2 -translate-y-1/2 text-ig-muted" />
          <input autoFocus value={query} onChange={(event) => setQuery(event.target.value)} placeholder="Search people, hashtags and posts" className="input pl-10" />
        </div>
      </form>

      {error && (
        <div className="card flex items-center justify-between gap-3 p-4 text-sm">
          <span className="text-rose-500">{error}</span>
          <button type="button" onClick={() => setSearchParams({ q: query })} className="font-semibold underline">Try again</button>
        </div>
      )}

      {!query && (
        <div className="card divide-y divide-ig-border dark:divide-darkig-border">
          <div className="flex items-center gap-2 px-4 py-3 text-sm font-semibold"><Clock size={16} /> Recent</div>
          {recent.length === 0 ? <p className="p-8 text-center text-sm text-ig-muted">No recent searches.</p> : recent.map((item) => (
            <button key={item} type="button" onClick={() => setSearchParams({ q: item })} className="flex w-full items-center gap-3 px-4 py-3 text-left text-sm hover:bg-ig-hover dark:hover:bg-darkig-hover">
              <Clock size={15} className="text-ig-muted" /> {item}
            </button>
          ))}
        </div>
      )}

      {!query && trending.length > 0 && (
        <section className="card p-4">
          <h2 className="mb-3 flex items-center gap-2 text-sm font-semibold"><Hash size={16} /> Trending hashtags</h2>
          <div className="flex flex-wrap gap-2">
            {trending.map((item) => (
              <Link key={item.tag} to={`/explore?tag=${item.tag}`} className="rounded-full bg-ig-hover px-4 py-2 text-sm dark:bg-darkig-hover">
                <strong>#{item.tag}</strong> <span className="text-ig-muted">{formatCount(item.count)} posts</span>
              </Link>
            ))}
          </div>
        </section>
      )}

      {loading && <Spinner label="Searching…" />}

      {query && !loading && (
        <div className="space-y-6">
          <section className="card divide-y divide-ig-border dark:divide-darkig-border">
            <h2 className="flex items-center gap-2 px-4 py-3 text-sm font-semibold"><User size={16} /> People</h2>
            {results.users.length === 0 ? <p className="px-4 py-5 text-sm text-ig-muted">No people found.</p> : results.users.map((item: IgUser) => (
              <button key={item.id} type="button" onClick={() => navigate(`/profile/${item.id}`)} className="flex w-full items-center gap-3 px-4 py-3 text-left hover:bg-ig-hover dark:hover:bg-darkig-hover">
                <Avatar src={item.profilePicture} name={item.username} size={44} />
                <div className="min-w-0 flex-1">
                  <div className="truncate text-sm font-semibold">{item.username} {item.isVerified && <span className="text-ig-accent">✓</span>}</div>
                  <div className="truncate text-xs text-ig-muted">{item.fullName}</div>
                </div>
              </button>
            ))}
          </section>

          <section className="card p-4">
            <h2 className="mb-3 flex items-center gap-2 text-sm font-semibold"><Hash size={16} /> Hashtags</h2>
            <div className="flex flex-wrap gap-2">
              {results.hashtags.length === 0 && <p className="text-sm text-ig-muted">No hashtags found.</p>}
              {results.hashtags.map((tag) => (
                <Link key={tag.tag} to={`/explore?tag=${tag.tag}`} className="rounded-full bg-ig-hover px-4 py-2 text-sm dark:bg-darkig-hover">
                  <strong>#{tag.tag}</strong> <span className="text-ig-muted">{formatCount(tag.count)} posts</span>
                </Link>
              ))}
            </div>
          </section>

          <section>
            <h2 className="mb-3 text-sm font-semibold">Posts</h2>
            {results.posts.length === 0 ? (
              <div className="card p-8 text-center text-sm text-ig-muted">No posts matched your search.</div>
            ) : (
              <div className="grid grid-cols-3 gap-1">
                {results.posts.map((post: Post) => (
                  <Link key={post.id} to={`/post/${post.id}`} className="aspect-square overflow-hidden bg-neutral-200 dark:bg-neutral-800">
                    <img src={post.media[0]?.url} alt={post.caption} className="h-full w-full object-cover transition hover:scale-105" />
                  </Link>
                ))}
              </div>
            )}
          </section>
        </div>
      )}
    </div>
  );
}
