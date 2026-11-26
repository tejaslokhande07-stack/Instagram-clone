/** @type {import('tailwindcss').Config} */
export default {
  darkMode: 'class',
  content: ['./index.html', './src/**/*.{js,ts,jsx,tsx}'],
  theme: {
    extend: {
      colors: {
        ig: {
          bg: '#fafafa',
          panel: '#ffffff',
          border: '#dbdbdb',
          text: '#262626',
          muted: '#8e8e8e',
          accent: '#0095f6',
          hover: '#fafafa',
        },
        darkig: {
          bg: '#000000',
          panel: '#121212',
          border: '#262626',
          text: '#f5f5f5',
          muted: '#a8a8a8',
          hover: '#1a1a1a',
        },
      },
      boxShadow: {
        card: '0 1px 2px rgba(0,0,0,0.06), 0 4px 16px rgba(0,0,0,0.04)',
        modal: '0 20px 60px rgba(0,0,0,0.25)',
      },
      keyframes: {
        'fade-in': { from: { opacity: '0' }, to: { opacity: '1' } },
        'slide-up': {
          from: { opacity: '0', transform: 'translateY(12px)' },
          to: { opacity: '1', transform: 'translateY(0)' },
        },
        'heart-pop': {
          '0%': { transform: 'scale(0.3)', opacity: '0' },
          '50%': { transform: 'scale(1.3)', opacity: '1' },
          '100%': { transform: 'scale(1)', opacity: '0' },
        },
      },
      animation: {
        'fade-in': 'fade-in 180ms ease-out',
        'slide-up': 'slide-up 220ms ease-out',
        'heart-pop': 'heart-pop 700ms ease-out',
      },
    },
  },
  plugins: [],
};
