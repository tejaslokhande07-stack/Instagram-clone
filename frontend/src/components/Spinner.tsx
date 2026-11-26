import { Loader2 } from 'lucide-react';

export default function Spinner({ label = 'Loading…' }: { label?: string }) {
  return (
    <div className="flex flex-col items-center justify-center gap-2 py-14 text-sm text-ig-muted dark:text-darkig-muted">
      <Loader2 size={24} className="animate-spin" />
      {label}
    </div>
  );
}
