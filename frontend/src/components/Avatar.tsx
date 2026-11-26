import { mediaUrl } from '../lib/api';
import { cx, initials } from '../lib/utils';

interface AvatarProps {
  src?: string;
  name?: string;
  size?: number;
  ring?: boolean;
  className?: string;
}

export default function Avatar({ src, name = '', size = 40, ring = false, className }: AvatarProps) {
  return (
    <span
      className={cx(
        'inline-flex shrink-0 items-center justify-center overflow-hidden rounded-full bg-neutral-200 dark:bg-neutral-700',
        ring && 'ring-gradient p-[2px]',
        className,
      )}
      style={{ width: ring ? size + 4 : size, height: ring ? size + 4 : size }}
    >
      <img
        src={mediaUrl(src || '')}
        alt={name ? `${name}'s avatar` : 'avatar'}
        className="h-full w-full rounded-full object-cover"
        style={ring ? { border: '2px solid var(--avatar-bg, #fff)' } : undefined}
        onError={(event) => {
          const target = event.currentTarget;
          target.style.display = 'none';
          const parent = target.parentElement;
          if (parent && !parent.querySelector('span[data-fallback]')) {
            const fallback = document.createElement('span');
            fallback.setAttribute('data-fallback', 'true');
            fallback.className = 'text-sm font-bold text-neutral-600 dark:text-neutral-200';
            fallback.textContent = initials(name || '?');
            parent.appendChild(fallback);
          }
        }}
      />
    </span>
  );
}
