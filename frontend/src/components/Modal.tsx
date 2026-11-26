import { useEffect, type ReactNode } from 'react';
import { X } from 'lucide-react';
import { cx } from '../lib/utils';

interface ModalProps {
  open: boolean;
  onClose: () => void;
  title?: string;
  children: ReactNode;
  width?: string;
  hideHeader?: boolean;
}

export default function Modal({ open, onClose, title, children, width = 'max-w-lg', hideHeader }: ModalProps) {
  useEffect(() => {
    if (!open) return;
    const onKey = (event: KeyboardEvent) => {
      if (event.key === 'Escape') onClose();
    };
    document.addEventListener('keydown', onKey);
    document.body.style.overflow = 'hidden';
    return () => {
      document.removeEventListener('keydown', onKey);
      document.body.style.overflow = '';
    };
  }, [open, onClose]);

  if (!open) return null;

  return (
    <div className="fixed inset-0 z-50 flex animate-fade-in items-center justify-center bg-black/60 p-3 sm:p-6" onMouseDown={onClose}>
      <div
        role="dialog"
        aria-modal="true"
        aria-label={title}
        className={cx('max-h-[92vh] w-full overflow-hidden rounded-2xl bg-ig-panel shadow-modal dark:bg-darkig-panel', width)}
        onMouseDown={(event) => event.stopPropagation()}
      >
        {!hideHeader && (
          <div className="flex items-center justify-between border-b border-ig-border px-5 py-3 dark:border-darkig-border">
            <h2 className="text-base font-semibold">{title || ''}</h2>
            <button type="button" onClick={onClose} className="icon-btn" aria-label="Close">
              <X size={20} />
            </button>
          </div>
        )}
        <div className="max-h-[calc(92vh-56px)] overflow-y-auto">{children}</div>
      </div>
    </div>
  );
}
