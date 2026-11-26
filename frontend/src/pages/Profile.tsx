import { useCallback, useEffect, useState, type FormEvent } from 'react';
import { Link, useNavigate, useParams } from 'react-router-dom';
import { Bookmark, Grid3X3, Lock, MoreHorizontal, Settings, UserCheck, UserPlus, UserX } from 'lucide-react';
import Avatar from '../components/Avatar';
import Modal from '../components/Modal';
import PostCard from '../components/PostCard';
import Spinner from '../components/Spinner';
import UserListModal from '../components/UserListModal';
import { useAuth } from '../context/AuthContext';
import { useToast } from '../context/ToastContext';
import type { FollowStatus, Post, User } from '../lib/types';
import { cx, formatCount } from '../lib/utils';

type Tab = 'posts' | 'saved';

export default function Profile() {
  const { id } = useParams();
  const { user: viewer, request, refreshUser } = useAuth();
  const { showToast } = useToast();
  const navigate = useNavigate();
  const profileId = Number(id || viewer?.id || 0);
  const own = profileId === viewer?.id;

  const [profile, setProfile] = useState<User | null>(null);
  const [posts, setPosts] = useState<Post[]>([]);
  const [tab, setTab] = useState<Tab>('posts');
  const [loading, setLoading] = useState(true);
  const [editOpen, setEditOpen] = useState(false);
  const [listModal, setListModal] = useState<null | { title: string; endpoint: string; key?: string }>(null);
  const [requests, setRequests] = useState<User[]>([]);
  const [requestOpen, setRequestOpen] = useState(false);
  const [reportOpen, setReportOpen] = useState(false);
  const [reason, setReason] = useState('Spam');
  const [menuOpen, setMenuOpen] = useState(false);
  const [followBusy, setFollowBusy] = useState(false);
  const [blockBusy, setBlockBusy] = useState(false);
  const [form, setForm] = useState({ fullName: '', bio: '', website: '', isPrivate: false, profilePicture: '' });

  const loadProfile = useCallback(async () => {
    if (!profileId) return;
    setLoading(true);
    try {
      const data = await request<User>(`/users/${profileId}`);
      setProfile(data);
      setForm({
        fullName: data.fullName,
        bio: data.bio,
        website: data.website,
        isPrivate: data.isPrivate,
        profilePicture: data.profilePicture,
      });
    } catch {
      setProfile(null);
    } finally {
      setLoading(false);
    }
  }, [profileId, request]);

  const loadPosts = useCallback(async () => {
    if (!profileId) return;
    try {
      const endpoint = tab === 'saved' && own ? '/posts/saved' : `/posts?userId=${profileId}&limit=30`;
      const data = await request<{ posts: Post[] }>(endpoint);
      setPosts(data.posts);
    } catch {
      setPosts([]);
    }
  }, [profileId, tab, own, request]);

  useEffect(() => { void loadProfile(); }, [loadProfile]);
  useEffect(() => { void loadPosts(); }, [loadPosts]);

  useEffect(() => {
    if (!own || !profile?.isPrivate) return;
    request<{ requests: User[] }>('/users/requests')
      .then((data) => setRequests(data.requests))
      .catch(() => undefined);
  }, [own, profile?.isPrivate, request]);

  const follow = async () => {
    if (!profile || own || followBusy) return;
    const wasFollowing = !!profile.isFollowing;
    const wasPending = profile.followStatus === 'pending';
    const method = wasFollowing || wasPending ? 'DELETE' : 'POST';
    setFollowBusy(true);
    try {
      const data = await request<{ status: string }>(`/users/${profile.id}/follow`, { method });

      // Update counts/state immediately from the successful response, then
      // reconcile with the authoritative follow-status endpoint.
      if (data.status === 'requested') {
        setProfile((current) => current ? { ...current, isFollowing: false, followStatus: 'pending' } : current);
        showToast('Follow request sent');
      } else if (data.status === 'following') {
        setProfile((current) => current ? {
          ...current,
          isFollowing: true,
          followStatus: 'accepted',
          followersCount: (current.followersCount || 0) + (wasFollowing || wasPending ? 0 : 1),
        } : current);
        showToast('Following');
      } else {
        setProfile((current) => current ? {
          ...current,
          isFollowing: false,
          followStatus: 'none',
          followersCount: Math.max(0, (current.followersCount || 0) - (wasFollowing ? 1 : 0)),
        } : current);
        showToast(wasPending ? 'Request cancelled' : 'Unfollowed');
      }

      try {
        const status = await request<FollowStatus>(`/users/${profile.id}/follow-status`);
        setProfile((current) => current ? {
          ...current,
          isFollowing: status.isFollowing,
          followStatus: status.followStatus,
          followersCount: status.followersCount,
          followingCount: status.followingCount,
          mutualCount: status.mutualCount,
        } : current);
      } catch {
        // Keep the optimistic state if the reconciliation call fails.
      }
    } catch (error) {
      showToast(error instanceof Error ? error.message : 'Could not update follow', 'error');
    } finally {
      setFollowBusy(false);
    }
  };

  const respond = async (requesterId: number, accept: boolean) => {
    try {
      await request(`/users/requests/${requesterId}`, { method: 'POST', body: { accept } });
      setRequests((current) => current.filter((item) => item.id !== requesterId));
      showToast(accept ? 'Request accepted' : 'Request rejected');
      await loadProfile();
    } catch {
      showToast('Could not update request', 'error');
    }
  };

  const saveProfile = async (event: FormEvent) => {
    event.preventDefault();
    try {
      await request(`/users/${profileId}`, { method: 'PUT', body: form });
      await Promise.all([loadProfile(), refreshUser()]);
      setEditOpen(false);
      showToast('Profile updated');
    } catch (error) {
      showToast(error instanceof Error ? error.message : 'Could not update profile', 'error');
    }
  };

  const submitReport = async () => {
    if (!profile) return;
    try {
      await request('/users/report', { method: 'POST', body: { userId: profile.id, reason, details: 'Reported from profile' } });
      setReportOpen(false);
      showToast('Report submitted');
    } catch {
      showToast('Could not submit report', 'error');
    }
  };

  const blockUser = async () => {
    if (!profile || blockBusy) return;
    setBlockBusy(true);
    try {
      await request(`/users/${profile.id}/block`, { method: 'POST' });
      setMenuOpen(false);
      showToast(`@${profile.username} blocked. You can unblock in Settings.`);
      navigate('/settings');
    } catch (error) {
      showToast(error instanceof Error ? error.message : 'Could not block user', 'error');
    } finally {
      setBlockBusy(false);
    }
  };

  if (loading) return <Spinner label="Loading profile…" />;
  if (!profile) {
    return (
      <div className="card p-12 text-center">
        <p className="text-lg font-semibold">User not found</p>
        <Link to="/" className="mt-3 inline-block text-sm font-semibold text-ig-accent">Back to feed</Link>
      </div>
    );
  }

  return (
    <div className="space-y-6">
      <header className="card flex flex-col gap-6 p-6 sm:flex-row sm:items-center">
        <Avatar src={profile.profilePicture} name={profile.username} size={110} ring={!profile.isPrivate} />
        <div className="min-w-0 flex-1">
          <div className="flex flex-wrap items-center gap-3">
            <h1 className="text-2xl font-normal">{profile.username}</h1>
            {profile.isVerified && <span className="text-ig-accent">✓</span>}
            {own ? (
              <>
                <button type="button" onClick={() => setEditOpen(true)} className="btn-secondary py-2">Edit profile</button>
                <button type="button" onClick={() => navigate('/settings')} className="icon-btn"><Settings size={19} /></button>
              </>
            ) : (
              <>
                <button
                  type="button"
                  disabled={followBusy}
                  onClick={() => void follow()}
                  aria-label={profile.isFollowing ? 'Unfollow this user' : profile.followStatus === 'pending' ? 'Cancel follow request' : 'Follow this user'}
                  className={cx('rounded-lg px-5 py-2 text-sm font-semibold disabled:opacity-60', profile.isFollowing || profile.followStatus === 'pending' ? 'btn-secondary' : 'btn-primary')}
                >
                  {followBusy ? 'Please wait…' : profile.isFollowing ? 'Unfollow' : profile.followStatus === 'pending' ? 'Cancel request' : 'Follow'}
                </button>
                <button type="button" onClick={() => navigate(`/messages?user=${profile.id}`)} className="btn-secondary py-2">Message</button>
                <div className="relative">
                  <button type="button" onClick={() => setMenuOpen((v) => !v)} className="icon-btn" aria-label="More options"><MoreHorizontal size={20} /></button>
                  {menuOpen && (
                    <div className="absolute right-0 top-10 z-20 w-40 rounded-xl border border-ig-border bg-ig-panel py-1 shadow-modal dark:border-darkig-border dark:bg-darkig-panel">
                      <button type="button" onClick={() => { setMenuOpen(false); setReportOpen(true); }} className="w-full px-4 py-2 text-left text-sm hover:bg-ig-hover dark:hover:bg-darkig-hover">Report user</button>
                      <button type="button" disabled={blockBusy} onClick={() => void blockUser()} className="flex w-full items-center gap-2 px-4 py-2 text-left text-sm text-rose-500 hover:bg-rose-50 disabled:opacity-60 dark:hover:bg-rose-950/20"><UserX size={14} /> {blockBusy ? 'Blocking…' : 'Block user'}</button>
                    </div>
                  )}
                </div>
              </>
            )}
          </div>

          <div className="mt-5 flex flex-wrap gap-6 text-sm">
            <span><strong>{formatCount(profile.postsCount || 0)}</strong> posts</span>
            <button type="button" onClick={() => setListModal({ title: 'Followers', endpoint: `/users/${profile.id}/followers` })}>
              <strong>{formatCount(profile.followersCount || 0)}</strong> followers
            </button>
            <button type="button" onClick={() => setListModal({ title: 'Following', endpoint: `/users/${profile.id}/following` })}>
              <strong>{formatCount(profile.followingCount || 0)}</strong> following
            </button>
            {!own && <button type="button" onClick={() => setListModal({ title: 'Mutual followers', endpoint: `/users/${profile.id}/mutual` })}>
              <strong>{profile.mutualCount || 0}</strong> mutual
            </button>}
          </div>

          <div className="mt-3 text-sm">
            <div className="font-semibold">{profile.fullName}</div>
            {profile.bio && <p className="mt-1 whitespace-pre-wrap">{profile.bio}</p>}
            {profile.website && <a href={profile.website} target="_blank" rel="noreferrer" className="mt-1 block text-sm text-ig-accent">{profile.website}</a>}
            {profile.isPrivate && <p className="mt-2 flex items-center gap-1 text-xs text-ig-muted"><Lock size={12} /> Private account</p>}
          </div>
        </div>
      </header>

      {own && requests.length > 0 && (
        <div className="card flex items-center justify-between gap-3 p-4">
          <div className="flex items-center gap-3">
            <UserCheck size={22} className="text-ig-accent" />
            <div>
              <p className="text-sm font-semibold">{requests.length} follow request{requests.length > 1 ? 's' : ''}</p>
              <p className="text-xs text-ig-muted">Review people who want to follow you</p>
            </div>
          </div>
          <button type="button" onClick={() => setRequestOpen(true)} className="btn-primary py-2">Review</button>
        </div>
      )}

      <div className="flex justify-center gap-10 border-t border-ig-border pt-1 dark:border-darkig-border">
        <button type="button" onClick={() => setTab('posts')} className={cx('flex items-center gap-2 border-t-2 px-3 py-3 text-xs font-semibold tracking-wide', tab === 'posts' ? 'border-ig-text dark:border-white' : 'border-transparent text-ig-muted')}>
          <Grid3X3 size={15} /> POSTS
        </button>
        {own && (
          <button type="button" onClick={() => setTab('saved')} className={cx('flex items-center gap-2 border-t-2 px-3 py-3 text-xs font-semibold tracking-wide', tab === 'saved' ? 'border-ig-text dark:border-white' : 'border-transparent text-ig-muted')}>
            <Bookmark size={15} /> SAVED
          </button>
        )}
      </div>

      {posts.length === 0 ? (
        <div className="card flex flex-col items-center gap-2 p-14 text-center">
          {tab === 'saved' ? <Bookmark size={38} strokeWidth={1.2} /> : <Grid3X3 size={38} strokeWidth={1.2} />}
          <p className="font-semibold">{tab === 'saved' ? 'Nothing saved yet' : 'No posts yet'}</p>
          <p className="text-sm text-ig-muted">{tab === 'saved' ? 'Save posts to see them here.' : 'When posts appear, they will show here.'}</p>
        </div>
      ) : (
        <div className="grid grid-cols-1 gap-4 sm:grid-cols-2 lg:grid-cols-3">
          {posts.map((post) => <PostCard key={post.id} post={post} onChanged={() => void loadPosts()} />)}
        </div>
      )}

      <Modal open={editOpen} onClose={() => setEditOpen(false)} title="Edit profile" width="max-w-md">
        <form onSubmit={(event) => void saveProfile(event)} className="space-y-4 p-5">
          <div className="flex justify-center"><Avatar src={form.profilePicture} name={profile.username} size={84} /></div>
          <label className="block text-sm">
            <span className="mb-1 block font-semibold">Profile picture URL</span>
            <input value={form.profilePicture} onChange={(event) => setForm({ ...form, profilePicture: event.target.value })} className="input" />
          </label>
          <label className="block text-sm">
            <span className="mb-1 block font-semibold">Full name</span>
            <input value={form.fullName} onChange={(event) => setForm({ ...form, fullName: event.target.value })} className="input" maxLength={100} />
          </label>
          <label className="block text-sm">
            <span className="mb-1 block font-semibold">Bio</span>
            <textarea value={form.bio} onChange={(event) => setForm({ ...form, bio: event.target.value })} className="input min-h-[90px]" maxLength={500} />
          </label>
          <label className="block text-sm">
            <span className="mb-1 block font-semibold">Website</span>
            <input value={form.website} onChange={(event) => setForm({ ...form, website: event.target.value })} className="input" />
          </label>
          <label className="flex items-center justify-between text-sm">
            <span className="font-semibold">Private account</span>
            <input type="checkbox" checked={form.isPrivate} onChange={(event) => setForm({ ...form, isPrivate: event.target.checked })} className="h-5 w-5 accent-sky-600" />
          </label>
          <button type="submit" className="btn-primary w-full">Save changes</button>
        </form>
      </Modal>

      <UserListModal open={!!listModal} onClose={() => setListModal(null)} title={listModal?.title || ''} endpoint={listModal?.endpoint || ''} keyName={listModal?.key || 'users'} />

      <Modal open={requestOpen} onClose={() => setRequestOpen(false)} title="Follow requests" width="max-w-md">
        <div className="divide-y divide-ig-border dark:divide-darkig-border">
          {requests.map((item) => (
            <div key={item.id} className="flex items-center gap-3 p-4">
              <Avatar src={item.profilePicture} name={item.username} size={40} />
              <div className="min-w-0 flex-1">
                <div className="truncate text-sm font-semibold">{item.username}</div>
                <div className="truncate text-xs text-ig-muted">{item.fullName}</div>
              </div>
              <button type="button" onClick={() => void respond(item.id, false)} className="btn-secondary py-1.5">Decline</button>
              <button type="button" onClick={() => void respond(item.id, true)} className="btn-primary py-1.5">Accept</button>
            </div>
          ))}
          {requests.length === 0 && <p className="p-8 text-center text-sm text-ig-muted">No pending requests.</p>}
        </div>
      </Modal>

      <Modal open={reportOpen} onClose={() => setReportOpen(false)} title="Report user" width="max-w-md">
        <div className="space-y-4 p-5">
          <p className="text-sm text-ig-muted">Report @{profile.username} to the demo moderators.</p>
          <select value={reason} onChange={(event) => setReason(event.target.value)} className="input">
            <option>Spam</option><option>Harassment</option><option>Impersonation</option><option>Inappropriate content</option><option>Other</option>
          </select>
          <button type="button" onClick={() => void submitReport()} className="btn-primary w-full">Submit report</button>
        </div>
      </Modal>

      {!own && <p className="text-center text-xs text-ig-muted"><UserPlus size={12} className="mr-1 inline" />Follow graph powered by the custom adjacency-list Graph</p>}
    </div>
  );
}
