import { useCallback, useEffect, useState } from 'react';
import { Link, useSearchParams } from 'react-router-dom';
import { RefreshCw, UserPlus } from 'lucide-react';
import Avatar from '../components/Avatar';
import CreatePostModal from '../components/CreatePostModal';
import CreateStoryModal from '../components/CreateStoryModal';
import PostCard from '../components/PostCard';
import Stories from '../components/Stories';
import Spinner from '../components/Spinner';
import { useAuth } from '../context/AuthContext';
import { useToast } from '../context/ToastContext';
import type { Hashtag, Post, Story, User } from '../lib/types';
import { cx } from '../lib/utils';

interface FeedResponse {
  posts: Post[];
  hasMore: boolean;
  order?: 'ranked' | 'chronological';
}

type FeedMode = 'popular' | 'following' | 'recent';

export default function Home() {
  const { request, user: viewer } = useAuth();
  const { showToast } = useToast();
  const [searchParams, setSearchParams] = useSearchParams();
  const [mode, setMode] = useState<FeedMode>('following');
  const [posts, setPosts] = useState<Post[]>([]);
  const [stories, setStories] = useState<Story[]>([]);
  const [mine, setMine] = useState<Story[]>([]);
  const [suggestions, setSuggestions] = useState<User[]>([]);
  const [suggestionsLoading, setSuggestionsLoading] = useState(false);
  const [suggestionsError, setSuggestionsError] = useState(false);
  const [tags, setTags] = useState<Hashtag[]>([]);
  const [page, setPage] = useState(1);
  const [hasMore, setHasMore] = useState(false);
  const [loading, setLoading] = useState(true);
  const [loadingMore, setLoadingMore] = useState(false);
  const [feedError, setFeedError] = useState(false);
  const [createPost, setCreatePost] = useState(false);
  const [createStory, setCreateStory] = useState(false);

  useEffect(() => {
    const kind = searchParams.get('create');
    if (kind === 'post') setCreatePost(true);
    if (kind === 'story') setCreateStory(true);
  }, [searchParams]);

  const closeCreate = (key: string, close: () => void) => {
    close();
    if (searchParams.get('create') === key) {
      const next = new URLSearchParams(searchParams);
      next.delete('create');
      setSearchParams(next, { replace: true });
    }
  };

  const loadStories = useCallback(() => {
    request<{ stories: Story[]; mine: Story[] }>('/stories')
      .then((data) => { setStories(data.stories); setMine(data.mine); })
      .catch(() => undefined);
  }, [request]);

  const loadSidebar = useCallback(() => {
    if (!viewer) return;
    setSuggestionsLoading(true);
    setSuggestionsError(false);
    request<{ users: User[] }>(`/users/${viewer.id}/suggestions`)
      .then((data) => setSuggestions(data.users.slice(0, 5)))
      .catch(() => { setSuggestions([]); setSuggestionsError(true); })
      .finally(() => setSuggestionsLoading(false));
    request<{ hashtags: Hashtag[] }>('/hashtags/trending')
      .then((data) => setTags(data.hashtags.slice(0, 6)))
      .catch(() => undefined);
  }, [request, viewer]);

  const loadFeed = useCallback(async (targetPage: number, append: boolean) => {
    if (append) setLoadingMore(true);
    else setLoading(true);
    setFeedError(false);
    try {
      const data = await request<FeedResponse>(`/feed?mode=${mode}&page=${targetPage}&limit=8`);
      setPosts((current) => (append ? [...current, ...data.posts] : data.posts));
      setHasMore(data.hasMore);
    } catch {
      setFeedError(true);
      showToast('Could not load feed', 'error');
    } finally {
      setLoading(false);
      setLoadingMore(false);
    }
  }, [request, mode, showToast]);

  useEffect(() => {
    setPage(1);
    void loadFeed(1, false);
  }, [loadFeed]);

  useEffect(() => {
    loadStories();
    loadSidebar();
  }, [loadStories, loadSidebar]);

  const follow = async (userId: number) => {
    try {
      const data = await request<{ status: string }>(`/users/${userId}/follow`, { method: 'POST' });
      setSuggestions((current) => current.filter((user) => user.id !== userId));
      showToast(data.status === 'requested' ? 'Follow request sent' : 'Following');
    } catch (error) {
      showToast(error instanceof Error ? error.message : 'Could not follow', 'error');
    }
  };

  return (
    <div className="grid gap-6 lg:grid-cols-[minmax(0,1fr)_300px]">
      <div className="min-w-0 space-y-4">
        <div className="card overflow-hidden">
          <Stories stories={stories} mine={mine} onViewed={(id) => {
            setStories((current) => current.map((story) => story.id === id ? { ...story, viewedByViewer: true } : story));
            setMine((current) => current.map((story) => story.id === id ? { ...story, viewedByViewer: true } : story));
          }} onCreate={() => setCreateStory(true)} />
        </div>

        <div className="card flex items-center justify-between gap-2 px-4 py-3 text-sm">
          <div className="flex items-center gap-3 sm:gap-5">
            <button type="button" onClick={() => setMode('popular')} className={cx('font-semibold', mode === 'popular' ? 'text-ig-accent' : 'text-ig-muted')}>
              For you
            </button>
            <span className="h-1 w-1 rounded-full bg-ig-border dark:bg-darkig-border" />
            <button type="button" onClick={() => setMode('following')} className={cx('font-semibold', mode === 'following' ? 'text-ig-accent' : 'text-ig-muted')}>
              Following
            </button>
            <span className="h-1 w-1 rounded-full bg-ig-border dark:bg-darkig-border" />
            <button type="button" onClick={() => setMode('recent')} className={cx('font-semibold', mode === 'recent' ? 'text-ig-accent' : 'text-ig-muted')}>
              Recent
            </button>
          </div>
          <button
            type="button"
            aria-label="Refresh feed"
            title="Refresh feed"
            disabled={loading}
            onClick={() => void loadFeed(1, false)}
            className="rounded-full p-1.5 text-ig-muted transition hover:bg-ig-border/50 hover:text-ig-accent disabled:opacity-50 dark:hover:bg-darkig-border/50"
          >
            <RefreshCw size={16} className={cx(loading && 'animate-spin')} />
          </button>
        </div>

        {feedError && !loading && posts.length > 0 && (
          <div className="card flex flex-wrap items-center justify-between gap-3 p-4 text-sm">
            <span className="text-ig-muted dark:text-darkig-muted">Couldn’t refresh your feed. Showing the last loaded posts.</span>
            <button type="button" onClick={() => void loadFeed(1, false)} className="font-semibold text-ig-accent">Try again</button>
          </div>
        )}

        {loading ? <Spinner label="Loading your feed…" /> : feedError && posts.length === 0 ? (
          <div className="card p-12 text-center">
            <p className="text-lg font-semibold">Couldn’t load your feed</p>
            <p className="mt-2 text-sm text-ig-muted dark:text-darkig-muted">
              Check your connection and try again.
            </p>
            <button type="button" onClick={() => void loadFeed(1, false)} className="btn-primary mt-5">Try again</button>
          </div>
        ) : posts.length === 0 ? (
          <div className="card p-12 text-center">
            <p className="text-lg font-semibold">Nothing here yet</p>
            <p className="mt-2 text-sm text-ig-muted dark:text-darkig-muted">
              Follow a few people or share your first post to get started.
            </p>
            <button type="button" onClick={() => setCreatePost(true)} className="btn-primary mt-5">Create a post</button>
          </div>
        ) : posts.map((post) => (
          <PostCard key={post.id} post={post} onChanged={() => void loadFeed(page, false)} />
        ))}

        {hasMore && !loading && (
          <div className="flex justify-center py-2">
            <button type="button" disabled={loadingMore} onClick={() => {
              const next = page + 1;
              setPage(next);
              void loadFeed(next, true);
            }} className="btn-secondary">
              {loadingMore ? 'Loading…' : 'Load more'}
            </button>
          </div>
        )}
      </div>

      <aside className="hidden space-y-4 lg:block">
        <div className="card p-4">
          <div className="mb-3 flex items-center justify-between">
            <h2 className="text-sm font-semibold text-ig-muted dark:text-darkig-muted">Suggested for you</h2>
            <UserPlus size={16} className="text-ig-muted" />
          </div>
          <div className="space-y-3">
            {suggestionsLoading ? (
              <Spinner label="Finding accounts…" />
            ) : suggestionsError ? (
              <div className="text-sm">
                <p className="text-ig-muted">Could not load suggestions.</p>
                <button type="button" onClick={loadSidebar} className="mt-2 font-semibold text-ig-accent">Try again</button>
              </div>
            ) : suggestions.length === 0 ? (
              <p className="text-sm text-ig-muted">You’re all caught up.</p>
            ) : suggestions.map((user) => (
              <div key={user.id} className="flex items-center gap-3">
                <Link to={`/profile/${user.id}`}>
                  <Avatar src={user.profilePicture} name={user.username} size={36} />
                </Link>
                <div className="min-w-0 flex-1">
                  <Link to={`/profile/${user.id}`} className="block truncate text-sm font-semibold hover:opacity-70">{user.username}</Link>
                  <span className="block truncate text-xs text-ig-muted dark:text-darkig-muted">{user.fullName}</span>
                </div>
                <button type="button" onClick={() => void follow(user.id)} className="text-xs font-semibold text-ig-accent">Follow</button>
              </div>
            ))}
          </div>
        </div>

        <div className="card p-4">
          <h2 className="mb-3 text-sm font-semibold text-ig-muted dark:text-darkig-muted">Trending hashtags</h2>
          <div className="flex flex-wrap gap-2">
            {tags.map((tag) => (
              <Link key={tag.tag} to={`/explore?tag=${tag.tag}`} className="rounded-full bg-ig-hover px-3 py-1.5 text-xs dark:bg-darkig-hover">
                <span className="font-semibold">#{tag.tag}</span>
                <span className="ml-1 text-ig-muted">{tag.count}</span>
              </Link>
            ))}
          </div>
        </div>

        <p className="px-2 text-[11px] leading-5 text-ig-muted dark:text-darkig-muted">
          About · Help · Privacy · Terms · DSA demo project · C++20? No — C++17 · MySQL 8
        </p>
      </aside>

      <CreatePostModal
        open={createPost}
        onClose={() => closeCreate('post', () => setCreatePost(false))}
        onCreated={() => void loadFeed(1, false)}
      />
      <CreateStoryModal open={createStory} onClose={() => closeCreate('story', () => setCreateStory(false))} onCreated={loadStories} />
    </div>
  );
}
