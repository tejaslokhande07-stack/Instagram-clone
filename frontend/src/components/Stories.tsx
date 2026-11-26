import { useEffect, useMemo, useState } from 'react';
import { ChevronLeft, ChevronRight, Plus, X } from 'lucide-react';
import Avatar from './Avatar';
import Modal from './Modal';
import { useAuth } from '../context/AuthContext';
import { mediaUrl } from '../lib/api';
import type { Story } from '../lib/types';
import { cx, timeAgo } from '../lib/utils';

interface StoriesProps {
  stories: Story[];
  mine: Story[];
  onViewed: (storyId: number) => void;
  onCreate: () => void;
}

export default function Stories({ stories, mine, onViewed, onCreate }: StoriesProps) {
  const { request, user } = useAuth();
  const [activeIndex, setActiveIndex] = useState<number | null>(null);
  const [open, setOpen] = useState(false);

  const groups = useMemo(() => {
    const map = new Map<number, Story[]>();
    [...mine, ...stories].forEach((story) => {
      const list = map.get(story.userId) || [];
      list.push(story);
      map.set(story.userId, list);
    });
    return Array.from(map.values()).map((list) => list.sort((a, b) => a.createdAt.localeCompare(b.createdAt)));
  }, [mine, stories]);

  const flat = useMemo(() => groups.flat(), [groups]);
  const active = activeIndex === null ? null : flat[activeIndex] || null;

  useEffect(() => {
    if (!open || !active || active.mediaType === 'video') return;
    const timer = window.setTimeout(() => {
      setActiveIndex((index) => (index === null ? null : index + 1 >= flat.length ? (setOpen(false), null) : index + 1));
    }, 5000);
    return () => window.clearTimeout(timer);
  }, [open, active, flat.length]);

  useEffect(() => {
    if (!active) return;
    onViewed(active.id);
    if (active.userId !== user?.id) {
      void request(`/stories/${active.id}/view`, { method: 'POST' }).catch(() => undefined);
    }
    // onViewed is intentionally excluded to avoid looping when the parent refreshes.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [active?.id]);

  const openAt = (story: Story) => {
    const index = flat.findIndex((item) => item.id === story.id);
    setActiveIndex(index >= 0 ? index : null);
    setOpen(true);
  };

  const move = (direction: number) => {
    setActiveIndex((index) => {
      if (index === null) return null;
      const next = index + direction;
      if (next < 0) return index;
      if (next >= flat.length) {
        setOpen(false);
        return null;
      }
      return next;
    });
  };

  return (
    <>
      <div className="no-scrollbar flex gap-4 overflow-x-auto border-b border-ig-border bg-ig-panel px-4 py-4 dark:border-darkig-border dark:bg-darkig-panel">
        <button type="button" onClick={onCreate} className="flex w-[68px] shrink-0 flex-col items-center gap-1.5">
          <span className="relative flex h-[62px] w-[62px] items-center justify-center rounded-full border-2 border-dashed border-ig-border dark:border-darkig-border">
            <Avatar src={user?.profilePicture} name={user?.username} size={50} />
            <span className="absolute bottom-0 right-0 flex h-5 w-5 items-center justify-center rounded-full border-2 border-ig-panel bg-ig-accent text-white dark:border-darkig-panel">
              <Plus size={13} strokeWidth={3} />
            </span>
          </span>
          <span className="w-full truncate text-center text-[11px] text-ig-muted">Your story</span>
        </button>

        {groups.map((group) => {
          const first = group[0];
          const seen = group.every((story) => story.viewedByViewer);
          return (
            <button key={first.userId} type="button" onClick={() => openAt(group.find((story) => !story.viewedByViewer) || first)} className="flex w-[68px] shrink-0 flex-col items-center gap-1.5">
              <span className={cx('rounded-full p-[2px]', seen ? 'bg-neutral-300 dark:bg-neutral-600' : 'ring-gradient')}>
                <span className="block rounded-full bg-ig-panel p-[2px] dark:bg-darkig-panel">
                  <Avatar src={first.avatar} name={first.username} size={52} />
                </span>
              </span>
              <span className="w-full truncate text-center text-[11px]">{first.username}</span>
            </button>
          );
        })}
      </div>

      <Modal open={open && !!active} onClose={() => setOpen(false)} hideHeader width="max-w-md">
        {active && (
          <div className="relative h-[min(86vh,820px)] w-full overflow-hidden bg-black text-white">
            <div className="absolute inset-x-0 top-0 z-20 flex gap-1 p-2">
              {flat.map((story) => (
                <div key={story.id} className="story-progress h-[3px] flex-1 overflow-hidden rounded-full bg-white/30">
                  {story.id === active.id ? <div className="h-full bg-white" /> : (
                    <div className="h-full bg-white/70" style={{ width: story.viewedByViewer ? '100%' : '0%' }} />
                  )}
                </div>
              ))}
            </div>

            <div className="absolute inset-x-0 top-6 z-20 flex items-center gap-3 px-4 py-2">
              <Avatar src={active.avatar} name={active.username} size={36} />
              <div className="min-w-0 flex-1">
                <div className="text-sm font-semibold">{active.username}</div>
                <div className="text-xs text-white/70">{timeAgo(active.createdAt)}</div>
              </div>
              <button type="button" onClick={() => setOpen(false)} className="icon-btn text-white hover:bg-white/10" aria-label="Close story">
                <X size={22} />
              </button>
            </div>

            {active.mediaType === 'video' ? (
              <video src={mediaUrl(active.mediaUrl)} autoPlay playsInline className="h-full w-full object-cover" onEnded={() => move(1)} />
            ) : (
              <img src={mediaUrl(active.mediaUrl)} alt="Story" className="h-full w-full object-cover" />
            )}

            <button type="button" onClick={() => move(-1)} className="absolute left-2 top-1/2 z-20 -translate-y-1/2 rounded-full bg-black/30 p-2" aria-label="Previous story">
              <ChevronLeft size={22} />
            </button>
            <button type="button" onClick={() => move(1)} className="absolute right-2 top-1/2 z-20 -translate-y-1/2 rounded-full bg-black/30 p-2" aria-label="Next story">
              <ChevronRight size={22} />
            </button>
          </div>
        )}
      </Modal>
    </>
  );
}
