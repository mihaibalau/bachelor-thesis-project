import { Box, CircularProgress, Skeleton, Stack, Typography } from '@mui/material';
import { useCallback, useEffect, useRef, useState } from 'react';
import { ErrorAlert } from '../../shared/components/ErrorAlert';
import { TransactionActivityList } from '../../shared/components/TransactionActivityList';
import { ApiError } from '../../shared/apiError';
import { fetchRecentTransactions } from './api';
import type { Transaction } from './types';

/** Rows loaded per infinite-scroll page (viewport-sized batch). */
export const RECENT_PAGE_SIZE = 20;

type Props = {
    accountId: number | '';
    refreshToken?: number;
    panelSx: object;
};

export function RecentTransactionsPanel({ accountId, refreshToken = 0, panelSx }: Props) {
    const [items, setItems] = useState<Transaction[]>([]);
    const [hasMore, setHasMore] = useState(true);
    const [loading, setLoading] = useState(false);
    const [loadingMore, setLoadingMore] = useState(false);
    const [error, setError] = useState<ApiError | null>(null);
    const sentinelRef = useRef<HTMLDivElement | null>(null);
    const offsetRef = useRef(0);
    const inFlightRef = useRef(false);

    const loadPage = useCallback(async (offset: number, append: boolean) => {
        if (!accountId || inFlightRef.current) return;
        inFlightRef.current = true;
        // 1. Show initial or "load more" spinner.
        if (append) {
            setLoadingMore(true);
        } else {
            setLoading(true);
            offsetRef.current = 0;
        }
        setError(null);
        try {
            // 2. Fetch one page and merge or replace the list.
            const res = await fetchRecentTransactions(Number(accountId), RECENT_PAGE_SIZE, offset);
            const batch = res.items;
            setItems((prev) => (append ? [...prev, ...batch] : batch));
            offsetRef.current = offset + batch.length;
            setHasMore(batch.length >= RECENT_PAGE_SIZE);
        } catch (err) {
            setError(
                err instanceof ApiError
                    ? err
                    : new ApiError(500, 'unknown_error', err instanceof Error ? err.message : 'Failed to load transactions'),
            );
            if (!append) setItems([]);
        } finally {
            setLoading(false);
            setLoadingMore(false);
            inFlightRef.current = false;
        }
    }, [accountId]);

    // 3. Reset and load first page when account or refresh token changes.
    useEffect(() => {
        // Reset the list when the account/refresh token changes, then load page 1.
        /* eslint-disable react-hooks/set-state-in-effect */
        if (!accountId) {
            setItems([]);
            setHasMore(false);
            return;
        }
        setItems([]);
        setHasMore(true);
        /* eslint-enable react-hooks/set-state-in-effect */
        void loadPage(0, false);
    }, [accountId, refreshToken, loadPage]);

    // 4. Infinite scroll: load next page when sentinel enters viewport.
    useEffect(() => {
        const el = sentinelRef.current;
        if (!el || !accountId || !hasMore || loading || loadingMore) return;

        const observer = new IntersectionObserver(
            (entries) => {
                if (!entries[0]?.isIntersecting || inFlightRef.current || !hasMore) return;
                void loadPage(offsetRef.current, true);
            },
            { root: null, rootMargin: '240px', threshold: 0 },
        );
        observer.observe(el);
        return () => observer.disconnect();
    }, [accountId, hasMore, loading, loadingMore, loadPage]);

    return (
        <Box sx={{ ...panelSx, p: 0, overflow: 'hidden' }}>
            <ErrorAlert error={error} onRetry={() => void loadPage(0, false)} />
            {loading && !items.length ? (
                <Stack sx={{ p: 2 }} spacing={1}>
                    {Array.from({ length: 6 }, (_, i) => (
                        <Skeleton key={i} height={56} />
                    ))}
                </Stack>
            ) : !items.length ? (
                <Typography sx={{ p: 3 }} color="text.secondary">
                    No transactions for this account.
                </Typography>
            ) : (
                <>
                    <TransactionActivityList
                        items={items}
                        accountId={Number(accountId)}
                    />
                    <Box ref={sentinelRef} sx={{ height: 1 }} />
                    {loadingMore && (
                        <Stack sx={{ py: 2, alignItems: 'center' }}>
                            <CircularProgress size={28} />
                        </Stack>
                    )}
                    {!hasMore && items.length > RECENT_PAGE_SIZE && (
                        <Typography
                            variant="body2"
                            color="text.secondary"
                            sx={{ py: 2, textAlign: 'center' }}
                        >
                            All transactions loaded
                        </Typography>
                    )}
                </>
            )}
        </Box>
    );
}
