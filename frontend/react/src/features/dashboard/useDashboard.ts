import { useCallback, useEffect, useState } from 'react';
import { fetchDashboard } from './api';
import type { DashboardData } from './types';
import { ApiError } from '../../shared/apiError';
import { DASHBOARD_REFRESH_EVENT } from '../../shared/refreshEvents';

type DashboardState = {
    data: DashboardData | null;
    isLoading: boolean;
    error: ApiError | null;
};

export function useDashboard() {
    const [state, setState] = useState<DashboardState>({
        data: null,
        isLoading: true,
        error: null,
    });

    const reload = useCallback(async () => {
        // 1. Enter loading state and fetch aggregated dashboard metrics.
        setState((prev) => ({ ...prev, isLoading: true, error: null }));
        try {
            const data = await fetchDashboard();
            setState({ data, isLoading: false, error: null });
        } catch (err) {
            setState({
                data: null,
                isLoading: false,
                error: err instanceof ApiError
                    ? err
                    : new ApiError(500, 'unknown_error', err instanceof Error ? err.message : 'Failed to load dashboard'),
            });
        }
    }, []);

    useEffect(() => {
        // 2. Load on mount; re-fetch when transactions trigger a refresh event.
        // reload() owns the loading/data setState; intentional fetch-on-mount.
        // eslint-disable-next-line react-hooks/set-state-in-effect
        void reload();
        const onRefresh = () => void reload();
        window.addEventListener(DASHBOARD_REFRESH_EVENT, onRefresh);
        return () => window.removeEventListener(DASHBOARD_REFRESH_EVENT, onRefresh);
    }, [reload]);

    return { ...state, reload };
}
