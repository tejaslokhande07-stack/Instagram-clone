import { useEffect, useRef, useState } from 'react';
import { ImagePlus, X } from 'lucide-react';
import Modal from './Modal';
import Avatar from './Avatar';
import { useAuth } from '../context/AuthContext';
import { useToast } from '../context/ToastContext';
import { fileToBase64 } from '../lib/api';
import type { Post } from '../lib/types';

interface CreatePostModalProps {
  open: boolean;
  onClose: () => void;
  onCreated: (post: Post) => void;
}

interface Draft {
  file: File;
  url: string;
  data: string;
}

const MAX_MEDIA = 10;
const MAX_CAPTION = 5000;
const MAX_LOCATION = 150;

export default function CreatePostModal({ open, onClose, onCreated }: CreatePostModalProps) {
  const { user, request } = useAuth();
  const { showToast } = useToast();
  const [drafts, setDrafts] = useState<Draft[]>([]);
  const [caption, setCaption] = useState('');
  const [location, setLocation] = useState('');
  const [busy, setBusy] = useState(false);
  const objectUrls = useRef<Set<string>>(new Set<string>());
  const wasOpen = useRef(false);

  useEffect(() => {
    if (wasOpen.current && !open) {
      objectUrls.current.forEach((url) => URL.revokeObjectURL(url));
      objectUrls.current.clear();
    }
    wasOpen.current = open;
  }, [open]);

  useEffect(() => {
    if (!open) {
      setDrafts([]);
      setCaption('');
      setLocation('');
    }
  }, [open]);

  useEffect(() => () => {
    objectUrls.current.forEach((url) => URL.revokeObjectURL(url));
    objectUrls.current.clear();
  }, []);

  const removeDraft = (url: string) => {
    URL.revokeObjectURL(url);
    objectUrls.current.delete(url);
    setDrafts((current) => current.filter((draft) => draft.url !== url));
  };

  const pickFiles = async (files: FileList | null) => {
    if (!files || files.length === 0) return;
    const available = MAX_MEDIA - drafts.length;
    if (available <= 0) {
      showToast(`A post can contain at most ${MAX_MEDIA} images`, 'error');
      return;
    }
    const all = Array.from(files);
    const selected = all.filter((file) => file.type.startsWith('image/')).slice(0, available);
    if (selected.length === 0) {
      showToast('Only image files are supported', 'error');
      return;
    }
    if (selected.length < all.length) {
      showToast(all.some((file) => !file.type.startsWith('image/'))
        ? 'Only image files are supported'
        : `A post can contain at most ${MAX_MEDIA} images`, 'error');
    }
    try {
      const prepared = await Promise.all(selected.map(async (file) => {
        const url = URL.createObjectURL(file);
        objectUrls.current.add(url);
        try {
          return { file, url, data: await fileToBase64(file) };
        } catch (error) {
          URL.revokeObjectURL(url);
          objectUrls.current.delete(url);
          throw error;
        }
      }));
      setDrafts((current) => [...current, ...prepared].slice(0, MAX_MEDIA));
    } catch {
      showToast('Could not read the selected images', 'error');
    }
  };

  const submit = async () => {
    if (drafts.length === 0 || busy || caption.length > MAX_CAPTION || location.length > MAX_LOCATION) return;
    setBusy(true);
    try {
      const result = await request<{ post: Post }>('/posts', {
        method: 'POST',
        body: {
          caption,
          location,
          media: drafts.map((draft) => ({
            data: draft.data,
            filename: draft.file.name,
            type: 'image',
          })),
        },
      });
      showToast('Post shared');
      onCreated(result.post);
      onClose();
    } catch (error) {
      showToast(error instanceof Error ? error.message : 'Could not share post', 'error');
    } finally {
      setBusy(false);
    }
  };

  return (
    <Modal open={open} onClose={onClose} title="Create new post" width="max-w-3xl">
      {drafts.length === 0 ? (
        <label className="flex h-[420px] cursor-pointer flex-col items-center justify-center gap-3 text-center">
          <ImagePlus size={56} strokeWidth={1.2} className="text-ig-muted" />
          <p className="text-xl font-light">Drag photos here</p>
          <p className="text-sm text-ig-muted">Select up to {MAX_MEDIA} images from your computer</p>
          <span className="btn-primary mt-2">Select from computer</span>
          <input type="file" accept="image/*" multiple hidden onChange={(event) => void pickFiles(event.target.files)} />
        </label>
      ) : (
        <div className="grid min-h-[440px] md:grid-cols-[1fr_320px]">
          <div className="relative flex min-h-[300px] items-center justify-center bg-black">
            <img src={drafts[0].url} alt="Preview" className="max-h-[520px] max-w-full object-contain" />
            {drafts.length > 1 && (
              <span className="absolute bottom-3 rounded-full bg-black/60 px-3 py-1 text-xs text-white">
                1 of {drafts.length}
              </span>
            )}
          </div>
          <div className="flex flex-col border-l border-ig-border dark:border-darkig-border">
            <div className="flex items-center gap-3 border-b border-ig-border px-4 py-3 dark:border-darkig-border">
              <Avatar src={user?.profilePicture} name={user?.username} size={34} />
              <span className="text-sm font-semibold">{user?.username}</span>
            </div>
            <textarea
              value={caption}
              onChange={(event) => setCaption(event.target.value)}
              placeholder="Write a caption… #travel #sunset"
              maxLength={MAX_CAPTION}
              className="min-h-[150px] flex-1 resize-none bg-transparent p-4 text-sm outline-none placeholder:text-ig-muted"
            />
            <p className="px-4 text-right text-[11px] text-ig-muted">{caption.length}/{MAX_CAPTION}</p>
            <input
              value={location}
              onChange={(event) => setLocation(event.target.value)}
              placeholder="Add location"
              maxLength={MAX_LOCATION}
              className="border-t border-ig-border bg-transparent px-4 py-3 text-sm outline-none placeholder:text-ig-muted dark:border-darkig-border"
            />
            <div className="flex gap-2 overflow-x-auto p-3">
              {drafts.map((draft, index) => (
                <div key={draft.url} className="relative h-14 w-14 shrink-0">
                  <img src={draft.url} alt={`Draft ${index + 1}`} className="h-full w-full rounded object-cover" />
                  <button type="button" onClick={() => removeDraft(draft.url)} className="absolute -right-1 -top-1 rounded-full bg-black/70 p-0.5 text-white" aria-label="Remove image">
                    <X size={12} />
                  </button>
                </div>
              ))}
            </div>
            <div className="flex items-center justify-between border-t border-ig-border p-3 dark:border-darkig-border">
              <label className="cursor-pointer text-sm font-semibold text-ig-accent">
                Add more
                <input type="file" accept="image/*" multiple hidden onChange={(event) => void pickFiles(event.target.files)} />
              </label>
              <button
                type="button"
                onClick={() => void submit()}
                disabled={busy || drafts.length === 0 || caption.length > MAX_CAPTION || location.length > MAX_LOCATION}
                className="btn-primary"
              >
                {busy ? 'Sharing…' : 'Share'}
              </button>
            </div>
          </div>
        </div>
      )}
    </Modal>
  );
}
