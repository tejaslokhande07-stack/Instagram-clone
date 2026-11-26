import {
  createContext,
  useCallback,
  useContext,
  useEffect,
  useMemo,
  useRef,
  useState,
  type ReactNode,
} from 'react';
import { ApiError, request as apiRequest, saveToken, storedToken } from '../lib/api';
import type { User } from '../lib/types';

type Theme = 'light' | 'dark';

interface AuthResponse {
  token: string;
  user: User;
}

interface RegisterPayload {
  username: string;
  email: string;
  password: string;
  fullName?: string;
}

interface AuthContextValue {
  user: User | null;
  token: string | null;
  loading: boolean;
  theme: Theme;
  toggleTheme: () => void;
  login: (username: string, password: string) => Promise<void>;
  register: (payload: RegisterPayload) => Promise<void>;
  logout: () => Promise<void>;
  updateUser: (patch: Partial<User>) => Promise<User>;
  refreshUser: () => Promise<void>;
  request: <T>(path: string, options?: { method?: 'GET' | 'POST' | 'PUT' | 'DELETE'; body?: unknown }) => Promise<T>;
}

const AuthContext = createContext<AuthContextValue | null>(null);

export function AuthProvider({ children }: { children: ReactNode }) {
  const [user, setUser] = useState<User | null>(null);
  const [token, setToken] = useState<string | null>(storedToken());
  const [loading, setLoading] = useState(true);
  const [theme, setTheme] = useState<Theme>(() => {
    const saved = localStorage.getItem('ig_theme');
    if (saved === 'dark' || saved === 'light') return saved;
    return window.matchMedia?.('(prefers-color-scheme: dark)').matches ? 'dark' : 'light';
  });
  const tokenRef = useRef<string | null>(storedToken());

  const clearSession = useCallback(() => {
    tokenRef.current = null;
    setToken(null);
    setUser(null);
    saveToken(null);
  }, []);

  const request = useCallback(async <T,>(
    path: string,
    options: { method?: 'GET' | 'POST' | 'PUT' | 'DELETE'; body?: unknown } = {},
  ): Promise<T> => {
    try {
      return await apiRequest<T>(path, { ...options, token: tokenRef.current });
    } catch (error) {
      if (error instanceof ApiError && error.status === 401) clearSession();
      throw error;
    }
  }, [clearSession]);

  const refreshUser = useCallback(async () => {
    if (!tokenRef.current) return;
    try {
      const profile = await request<User>('/auth/me');
      setUser(profile);
    } catch {
      clearSession();
    }
  }, [request, clearSession]);

  useEffect(() => {
    let active = true;
    if (!tokenRef.current) {
      setLoading(false);
      return;
    }
    apiRequest<User>('/auth/me', { token: tokenRef.current })
      .then((profile) => {
        if (active) setUser(profile);
      })
      .catch(() => {
        if (active) clearSession();
      })
      .finally(() => {
        if (active) setLoading(false);
      });
    return () => {
      active = false;
    };
  }, [clearSession]);

  useEffect(() => {
    document.documentElement.classList.toggle('dark', theme === 'dark');
    localStorage.setItem('ig_theme', theme);
  }, [theme]);

  const toggleTheme = useCallback(() => {
    setTheme((current) => (current === 'dark' ? 'light' : 'dark'));
  }, []);

  const applyAuth = useCallback((result: AuthResponse) => {
    tokenRef.current = result.token;
    setToken(result.token);
    setUser(result.user);
    saveToken(result.token);
  }, []);

  const login = useCallback(async (username: string, password: string) => {
    const result = await apiRequest<AuthResponse>('/auth/login', {
      method: 'POST',
      body: { username, password },
    });
    applyAuth(result);
  }, [applyAuth]);

  const register = useCallback(async (payload: RegisterPayload) => {
    const result = await apiRequest<AuthResponse>('/auth/register', {
      method: 'POST',
      body: payload,
    });
    applyAuth(result);
  }, [applyAuth]);

  const logout = useCallback(async () => {
    const current = tokenRef.current;
    if (current) {
      try {
        await apiRequest('/auth/logout', { method: 'POST', token: current });
      } catch {
        // Local logout still succeeds if the session already expired.
      }
    }
    clearSession();
  }, [clearSession]);

  const updateUser = useCallback(async (patch: Partial<User>) => {
    if (!user) throw new ApiError(401, 'Not signed in');
    const updated = await request<User>(`/users/${user.id}`, {
      method: 'PUT',
      body: patch,
    });
    setUser((current) => (current ? { ...current, ...updated } : current));
    return updated;
  }, [request, user]);

  const value = useMemo<AuthContextValue>(() => ({
    user,
    token,
    loading,
    theme,
    toggleTheme,
    login,
    register,
    logout,
    updateUser,
    refreshUser,
    request,
  }), [user, token, loading, theme, toggleTheme, login, register, logout,
       updateUser, refreshUser, request]);

  return <AuthContext.Provider value={value}>{children}</AuthContext.Provider>;
}

export function useAuth(): AuthContextValue {
  const context = useContext(AuthContext);
  if (!context) throw new Error('useAuth must be used inside AuthProvider');
  return context;
}
