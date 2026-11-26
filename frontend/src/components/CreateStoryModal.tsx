import { useEffect, useRef, useState } from 'react';
import { ImagePlus } from 'lucide-react';
import Modal from './Modal';
import { useAuth } from '../context/AuthContext';
import { useToast } from '../context/ToastContext';
import { fileToBase64 } from '../lib/api';

interface CreateStoryModalProps {
  open: boolean;
  onClose: () => void;
  onCreated: () => void;
}

export default function CreateStoryModal({ open, onClose, onCreated }: CreateStoryModalProps) {
  const { request } = useAuth();
  const { showToast } = useToast();
  const inputRef = useRef<HTMLInputElement>(null);
  const [file, setFile] = useState<File | null>(null);
  const [preview, setPreview] = useState('');
  const [busy, setBusy] = useState(false);

  useEffect(() => {
    if (!open) {
      setFile(null);
      setPreview('');
    }
  }, [open]);

  useEffect(() => () => { if (preview) URL.revokeObjectURL(preview); }, [preview]);

  const choose = (selected?: File) => {
    if (!selected) return;
    setFile(selected);
    setPreview(URL.createObjectURL(selected));
  };

  const submit = async () => {
    if (!file || busy) return;
    setBusy(true);
    try {
      await request('/stories', {
        method: 'POST',
        body: {
          data: await fileToBase64(file),
          filename: file.name,
          type: file.type.startsWith('video/') ? 'video' : 'image',
        },
      });
      showToast('Story added');
      onCreated();
      onClose();
    } catch (error) {
      showToast(error instanceof Error ? error.message : 'Could not add story', 'error');
    } finally {
      setBusy(false);
    }
  };

  return (
    <Modal open={open} onClose={onClose} title="Add to your story" width="max-w-md">
      <div className="space-y-4 p-5">
        <input
          ref={inputRef}
          type="file"
          accept="image/*,video/*"
          hidden
          onChange={(event) => choose(event.target.files?.[0])}
        />
        {preview ? (
          <div className="relative aspect-[9/14] max-h-[440px] overflow-hidden rounded-xl bg-black">
            {file?.type.startsWith('video/') ? (
              <video src={preview} className="h-full w-full object-contain" controls />
            ) : (
              <img src={preview} alt="Story preview" className="h-full w-full object-contain" />
            )}
            <button type="button" onClick={() => inputRef.current?.click()} className="absolute bottom-3 left-1/2 -translate-x-1/2 rounded-full bg-black/60 px-4 py-2 text-sm text-white">
              Change
            </button>
          </div>
        ) : (
          <button type="button" onClick={() => inputRef.current?.click()} className="flex h-56 w-full flex-col items-center justify-center gap-2 rounded-xl border-2 border-dashed border-ig-border text-ig-muted dark:border-darkig-border">
            <ImagePlus size={40} strokeWidth={1.2} />
            <span>Choose a photo or video</span>
          </button>
        )}
        <button type="button" onClick={() => void submit()} disabled={!file || busy} className="btn-primary w-full">
          {busy ? 'Sharing…' : 'Share to story'}
        </button>
      </div>
    </Modal>
  );
}
