import { createContext, useCallback, useContext, useEffect, useRef, useState, type ReactNode } from 'react';
import { useAuth } from '../../features/auth/AuthContext';
import { fetchUserWithAccounts } from '../../features/users/api';
import type { Account, User } from '../../features/users/types';
import { ApiError } from '../apiError';
import { setAuthToken } from '../apiClient';

type AccountsContextValue = {
    user: User | null;
    accounts: Account[];
    isLoading: boolean;
    error: ApiError | null;
    reload: () => Promise<void>;
};

const AccountsContext = createContext<AccountsContextValue | undefined>(undefined);

export function AccountsProvider({ children }: { children: ReactNode }) {
    const { userId, token, isReady } = useAuth();
    const [user, setUser] = useState<User | null>(null);
    const [accounts, setAccounts] = useState<Account[]>([]);
    const [isLoading, setIsLoading] = useState(true);
    const [error, setError] = useState<ApiError | null>(null);
    const loadSeq = useRef(0);

    const reload = useCallback(async () => {
        // 1. Clear state when signed out or auth not ready.
        if (!isReady || !userId || !token) {
            setUser(null);
            setAccounts([]);
            setIsLoading(false);
            return;
        }

        // 2. Sync token and fetch user + accounts; ignore stale responses.
        setAuthToken(token);
        const seq = ++loadSeq.current;
        setIsLoading(true);
        setError(null);
        try {
            const data = await fetchUserWithAccounts(userId);
            if (seq !== loadSeq.current) return;
            setUser(data.user);
            setAccounts(data.accounts);
        } catch (err) {
            if (seq !== loadSeq.current) return;
            setError(err instanceof ApiError ? err : new ApiError(500, 'unknown_error', 'Failed to load accounts'));
            setUser(null);
            setAccounts([]);
        } finally {
            if (seq === loadSeq.current) {
                setIsLoading(false);
            }
        }
    }, [userId, token, isReady]);

    // 3. Load on mount and whenever auth identity changes.
    useEffect(() => {
        // reload() owns the loading/data setState; intentional fetch-on-mount.
        // eslint-disable-next-line react-hooks/set-state-in-effect
        void reload();
    }, [reload]);

    return (
        <AccountsContext.Provider value={{ user, accounts, isLoading, error, reload }}>
            {children}
        </AccountsContext.Provider>
    );
}

// eslint-disable-next-line react-refresh/only-export-components
export function useAccounts() {
    const ctx = useContext(AccountsContext);
    if (!ctx) throw new Error('useAccounts must be used within AccountsProvider');
    return ctx;
}
