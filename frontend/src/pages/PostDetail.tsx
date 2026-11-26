import { useEffect, useState } from 'react';
import { Link, useNavigate, useParams } from 'react-router-dom';
import { ArrowLeft } from 'lucide-react';
import PostCard from '../components/PostCard';
import Spinner from '../components/Spinner';
import { useAuth } from '../context/AuthContext';
import type { Post } from '../lib/types';

export default function PostDetail() {
  const { id } = useParams();
  const navigate = useNavigate();
  const { request } = useAuth();
  const [post, setPost] = useState<Post | null>(null);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    let active = true;
    request<{ post: Post }>(`/posts/${id}`)
      .then((data) => { if (active) setPost(data.post); })
      .catch(() => { if (active) setPost(null); })
      .finally(() => { if (active) setLoading(false); });
    return () => { active = false; };
  }, [id, request]);

  if (loading) return <Spinner label="Loading post…" />;

  return (
    <div className="mx-auto max-w-xl space-y-4">
      <Link to="/" className="inline-flex items-center gap-2 text-sm text-ig-muted hover:opacity-70"><ArrowLeft size={16} /> Back to feed</Link>
      {post ? <PostCard post={post} onDeleted={() => navigate('/')} /> : (
        <div className="card p-12 text-center">
          <p className="font-semibold">Post unavailable</p>
          <p className="mt-2 text-sm text-ig-muted">It may have been deleted or the account is private.</p>
        </div>
      )}
    </div>
  );
}
