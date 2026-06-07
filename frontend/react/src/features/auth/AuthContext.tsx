import { createContext, useCallback, useContext, useState, useEffect } from 'react';
import type { ReactNode } from 'react';
import { setAuthToken, setUnauthorizedHandler } from '../../shared/apiClient';
import { login, type LoginRequest, type LoginResponse } from './api';

interface AuthState {
    token: string | null;
    userId: number | null;
}

interface AuthContextValue extends AuthState {
    isReady: boolean;
    loginUser: (credentials: LoginRequest) => Promise<void>;
    logout: () => void;
}

const AuthContext = createContext<AuthContextValue | undefined>(undefined);

const STORAGE_KEY = 'gentlix_auth';

function getInitialAuthState(): AuthState {
    // 1. Read persisted session; discard corrupt JSON.
    const raw = localStorage.getItem(STORAGE_KEY);
    if (!raw) {
        return {
            token: null,
            userId: null,
        };
    }

    try {
        return JSON.parse(raw) as AuthState;
    } catch {
        localStorage.removeItem(STORAGE_KEY);
        return {
            token: null,
            userId: null,
        };
    }
}

export function AuthProvider({ children }: { children: ReactNode }) {
    // 1. Restore persisted session and sync token before child effects run.
    const [state, setState] = useState<AuthState>(() => {
        const initial = getInitialAuthState();
        setAuthToken(initial.token);
        return initial;
    });

    // 2. Keep apiClient token in sync with React state.
    useEffect(() => {
        setAuthToken(state.token);
    }, [state.token]);

    const loginUser = async (credentials: LoginRequest) => {
        const res: LoginResponse = await login(credentials);
        const newState: AuthState = {
            token: res.token,
            userId: res.user_id,
        };
        setState(newState);
        localStorage.setItem(STORAGE_KEY, JSON.stringify(newState));
    };

    const logout = useCallback(() => {
        const emptyState: AuthState = {
            token: null,
            userId: null,
        };
        setState(emptyState);
        localStorage.removeItem(STORAGE_KEY);
    }, []);

    // 3. On 401 from apiClient, clear session and redirect to login.
    useEffect(() => {
        setUnauthorizedHandler(() => {
            logout();
            if (window.location.pathname !== '/login') {
                window.location.replace('/login');
            }
        });
        return () => setUnauthorizedHandler(null);
    }, [logout]);

    const value: AuthContextValue = {
        ...state,
        isReady: true,
        loginUser,
        logout,
    };

    return <AuthContext.Provider value={value}>{children}</AuthContext.Provider>;
}

// eslint-disable-next-line react-refresh/only-export-components
export function useAuth() {
    const ctx = useContext(AuthContext);
    if (!ctx) {
        throw new Error('useAuth must be used within AuthProvider');
    }
    return ctx;
}