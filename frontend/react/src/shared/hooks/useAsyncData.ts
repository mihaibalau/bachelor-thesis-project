import { useCallback, useEffect, useState } from 'react';
import { ApiError } from '../apiError';

type AsyncState<T> = {
    data: T | null;
    isLoading: boolean;
    error: ApiError | null;
};

export function useAsyncData<T>(
    fetcher: () => Promise<T>,
    deps: unknown[] = [],
) {
    const [state, setState] = useState<AsyncState<T>>({
        data: null,
        isLoading: true,
        error: null,
    });

    const reload = useCallback(async () => {
        // 1. Reset to loading and clear prior data/error.
        setState({ data: null, isLoading: true, error: null });
        try {
            // 2. Run fetcher and store result.
            const data = await fetcher();
            setState({ data, isLoading: false, error: null });
        } catch (err) {
            // 3. Normalize unknown errors into ApiError.
            setState({
                data: null,
                isLoading: false,
                error: err instanceof ApiError ? err : new ApiError(500, 'unknown_error', err instanceof Error ? err.message : 'Unknown error'),
            });
        }
        // Caller-supplied deps are intentionally spread into useCallback.
        // eslint-disable-next-line react-hooks/exhaustive-deps, react-hooks/use-memo
    }, deps);

    // 4. Fetch on mount and when deps change.
    useEffect(() => {
        void reload();
    }, [reload]);

    return { ...state, reload };
}
