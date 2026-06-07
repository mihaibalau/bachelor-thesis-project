import {
    Box,
    Button,
    FormControl,
    InputLabel,
    MenuItem,
    Pagination,
    Paper,
    Select,
    Skeleton,
    Stack,
    TextField,
    Typography,
} from '@mui/material';
import { useEffect, useMemo, useState } from 'react';
import { useSearchParams } from 'react-router-dom';
import { PageHeader } from '../../shared/components/PageHeader';
import { ErrorAlert } from '../../shared/components/ErrorAlert';
import { AccountPicker } from '../../shared/components/AccountPicker';
import { useAccounts } from '../../shared/context/AccountsContext';
import { useAsyncData } from '../../shared/hooks/useAsyncData';
import { useSurfaceStyles } from '../../shared/hooks/useSurfaceStyles';
import { useButtonStyles } from '../../shared/hooks/useButtonStyles';
import { TransactionActivityList } from '../../shared/components/TransactionActivityList';
import { formatRon, monthStartIso, todayIso } from '../../shared/format';
import { statementRangeBalances } from '../../shared/transactionDisplay';
import { fetchStatement } from '../transactions/api';
import type { StatementEntry } from '../transactions/types';

const TX_TYPES = ['All', 'Deposit', 'Withdrawal', 'Payment', 'Transfer', 'Send'] as const;
const PAGE_SIZES = [10, 25, 50, 100] as const;
const FETCH_LIMIT = 500;

type SortOrder = 'oldest' | 'newest';
type ViewMode = 'grouped' | 'flat';

export function StatementsPage() {
    const [params, setParams] = useSearchParams();
    const { accounts } = useAccounts();
    const { cardBg, cardBorder, cardShadow, filterFieldSx } = useSurfaceStyles();
    const { softOutlined } = useButtonStyles();

    const [accountId, setAccountId] = useState<number | ''>(Number(params.get('account')) || '');
    const [from, setFrom] = useState(params.get('from') || monthStartIso());
    const [to, setTo] = useState(params.get('to') || todayIso());
    const [typeFilter, setTypeFilter] = useState<string>('All');
    const [sortOrder, setSortOrder] = useState<SortOrder>('oldest');
    const [viewMode, setViewMode] = useState<ViewMode>('grouped');
    const [pageSize, setPageSize] = useState<number>(25);
    const [page, setPage] = useState(1);
    const [serverPage, setServerPage] = useState(0);

    useEffect(() => {
        // Default to the first account once the list loads (one-time selection).
        // eslint-disable-next-line react-hooks/set-state-in-effect
        if (accounts.length && accountId === '') setAccountId(accounts[0].id);
    }, [accounts, accountId]);

    const selectedAccount = accounts.find((a) => a.id === accountId);

    // Fetch up to FETCH_LIMIT rows per server page for the selected account/range.
    const { data, isLoading, error, reload } = useAsyncData(
        () =>
            accountId
                ? fetchStatement({
                    account_id: Number(accountId),
                    from,
                    to,
                    limit: FETCH_LIMIT,
                    offset: serverPage * FETCH_LIMIT,
                })
                : Promise.resolve({ items: [] }),
        [accountId, from, to, serverPage],
    );

    // Client-side type filter on the fetched batch.
    const filtered = useMemo(() => {
        let items: StatementEntry[] = data?.items ?? [];
        if (typeFilter !== 'All') {
            items = items.filter((i) => i.transaction_type === typeFilter);
        }
        return items;
    }, [data?.items, typeFilter]);

    const signedAmounts = useMemo(() => {
        const map = new Map<number, number>();
        for (const row of filtered) {
            map.set(row.transaction_id, row.value_cents);
        }
        return map;
    }, [filtered]);

    const balancesVisible = typeFilter === 'All';
    const { opening: openingBalance, closing: closingBalance } = useMemo(
        () => (balancesVisible ? statementRangeBalances(filtered, signedAmounts) : { opening: null, closing: null }),
        [filtered, signedAmounts, balancesVisible],
    );

    const processed = useMemo(() => {
        if (sortOrder === 'newest') return [...filtered].reverse();
        return filtered;
    }, [filtered, sortOrder]);

    const totalPages = Math.max(1, Math.ceil(processed.length / pageSize));
    const safePage = Math.min(page, totalPages);
    const paged = processed.slice((safePage - 1) * pageSize, safePage * pageSize);

    // Reset pagination, sync URL params, and refetch statement data.
    const applyFilters = () => {
        setPage(1);
        setServerPage(0);
        if (accountId) {
            setParams({
                account: String(accountId),
                from,
                to,
            });
        }
        void reload();
    };

    const handleAccountChange = (id: number) => {
        setAccountId(id);
        setPage(1);
        setServerPage(0);
    };

    const panelSx = {
        borderRadius: 3,
        border: cardBorder,
        boxShadow: cardShadow,
        width: '100%',
    };

    const filterControlProps = {
        size: 'small' as const,
        fullWidth: true,
        sx: filterFieldSx,
    };

    const highlight = { color: 'primary.main', fontWeight: 700 };

    return (
        <>
            <PageHeader
                title="Statements"
                subtitle="Account statement with running balance for any date range."
                action={
                    <AccountPicker
                        accounts={accounts}
                        value={accountId}
                        onChange={handleAccountChange}
                    />
                }
            />

            <Paper
                elevation={0}
                sx={{
                    ...panelSx,
                    p: 2,
                    mb: 1.5,
                    bgcolor: cardBg,
                }}
            >
                <Stack spacing={2}>
                    <Stack direction={{ xs: 'column', md: 'row' }} spacing={2}>
                        <TextField
                            type="date"
                            label="From"
                            value={from}
                            onChange={(e) => setFrom(e.target.value)}
                            slotProps={{ inputLabel: { shrink: true } }}
                            size="small"
                            fullWidth
                            sx={filterFieldSx}
                        />
                        <TextField
                            type="date"
                            label="To"
                            value={to}
                            onChange={(e) => setTo(e.target.value)}
                            slotProps={{ inputLabel: { shrink: true } }}
                            size="small"
                            fullWidth
                            sx={filterFieldSx}
                        />
                        <FormControl {...filterControlProps}>
                            <InputLabel>Type</InputLabel>
                            <Select
                                label="Type"
                                value={typeFilter}
                                onChange={(e) => { setTypeFilter(e.target.value); setPage(1); }}
                            >
                                {TX_TYPES.map((t) => (
                                    <MenuItem key={t} value={t}>{t}</MenuItem>
                                ))}
                            </Select>
                        </FormControl>
                    </Stack>

                    <Stack direction={{ xs: 'column', sm: 'row' }} spacing={1.5} sx={{ alignItems: { sm: 'center' } }}>
                        <FormControl {...filterControlProps} sx={{ ...filterFieldSx, minWidth: { sm: 160 } }}>
                            <InputLabel>Sort</InputLabel>
                            <Select
                                label="Sort"
                                value={sortOrder}
                                onChange={(e) => { setSortOrder(e.target.value as SortOrder); setPage(1); }}
                            >
                                <MenuItem value="oldest">Oldest first</MenuItem>
                                <MenuItem value="newest">Newest first</MenuItem>
                            </Select>
                        </FormControl>
                        <FormControl {...filterControlProps} sx={{ ...filterFieldSx, minWidth: { sm: 160 } }}>
                            <InputLabel>View</InputLabel>
                            <Select
                                label="View"
                                value={viewMode}
                                onChange={(e) => setViewMode(e.target.value as ViewMode)}
                            >
                                <MenuItem value="grouped">Grouped by day</MenuItem>
                                <MenuItem value="flat">Flat list</MenuItem>
                            </Select>
                        </FormControl>
                        <FormControl {...filterControlProps} sx={{ ...filterFieldSx, minWidth: { sm: 130 } }}>
                            <InputLabel>Page size</InputLabel>
                            <Select
                                label="Page size"
                                value={pageSize}
                                onChange={(e) => { setPageSize(Number(e.target.value)); setPage(1); }}
                            >
                                {PAGE_SIZES.map((s) => (
                                    <MenuItem key={s} value={s}>{s} rows</MenuItem>
                                ))}
                            </Select>
                        </FormControl>
                        <Box sx={{ flex: 1, display: { xs: 'none', sm: 'block' } }} />
                        <Button
                            variant="outlined"
                            color="inherit"
                            disableElevation
                            onClick={() => void applyFilters()}
                            sx={{ ...softOutlined, minWidth: { sm: 140 }, alignSelf: { xs: 'stretch', sm: 'auto' } }}
                        >
                            Apply filters
                        </Button>
                    </Stack>
                </Stack>
            </Paper>

            {processed.length > 0 && openingBalance !== null && closingBalance !== null && (
                <Typography
                    variant="body2"
                    color="text.secondary"
                    sx={{ mb: 2.5, lineHeight: 1.7 }}
                >
                    Opening{' '}
                    <Box component="span" sx={highlight}>{formatRon(openingBalance)}</Box>
                    {' · '}
                    Closing{' '}
                    <Box component="span" sx={highlight}>{formatRon(closingBalance)}</Box>
                    {' · '}
                    <Box component="span" sx={{ color: 'text.primary', fontWeight: 600 }}>
                        {processed.length}
                    </Box>
                    {' '}
                    transaction{processed.length === 1 ? '' : 's'}
                    {selectedAccount && (
                        <>
                            {' · '}
                            <Box component="span" sx={{ color: 'text.primary', fontWeight: 500 }}>
                                {selectedAccount.account_type} {selectedAccount.currency}
                            </Box>
                        </>
                    )}
                </Typography>
            )}

            <ErrorAlert error={error} onRetry={() => void reload()} />

            <Paper elevation={0} sx={{ ...panelSx, overflow: 'hidden' }}>
                {isLoading ? (
                    <Stack sx={{ p: 2 }} spacing={1}>{[0, 1, 2, 3, 4].map((i) => <Skeleton key={i} height={56} />)}</Stack>
                ) : !paged.length ? (
                    <Typography sx={{ p: 3 }} color="text.secondary">No statement entries match your filters.</Typography>
                ) : (
                    <TransactionActivityList
                        items={paged.map((row) => ({
                            id: row.transaction_id,
                            from_account_id: 0,
                            to_account_id: 0,
                            transaction_type: row.transaction_type,
                            value_cents: 0,
                            recorded_on: row.recorded_on,
                            description: row.description,
                            signed_value_cents: signedAmounts.get(row.transaction_id) ?? row.value_cents,
                        }))}
                        accountId={Number(accountId)}
                        groupByDate={viewMode === 'grouped'}
                        showBalanceAfter={(row) => {
                            const entry = paged.find((e) => e.transaction_id === row.id);
                            return entry ? formatRon(entry.balance_after_cents) : undefined;
                        }}
                    />
                )}
            </Paper>

            <Stack direction="row" sx={{ justifyContent: 'center', mt: 2.5, alignItems: 'center', gap: 1.5 }}>
                <Pagination
                    count={totalPages}
                    page={safePage}
                    onChange={(_, p) => setPage(p)}
                    color="primary"
                    shape="rounded"
                    disabled={isLoading}
                />
                <Typography variant="body2" color="text.secondary">
                    Page {safePage} of {totalPages}
                </Typography>
            </Stack>

            {data && data.items.length >= FETCH_LIMIT && (
                <Stack direction="row" spacing={1} sx={{ justifyContent: 'center', mt: 2 }}>
                    <Button
                        size="small"
                        variant="outlined"
                        color="inherit"
                        disabled={serverPage === 0}
                        onClick={() => { setServerPage((p) => p - 1); setPage(1); }}
                        sx={softOutlined}
                    >
                        Earlier period
                    </Button>
                    <Button
                        size="small"
                        variant="outlined"
                        color="inherit"
                        onClick={() => { setServerPage((p) => p + 1); setPage(1); }}
                        sx={softOutlined}
                    >
                        Load more history
                    </Button>
                </Stack>
            )}
        </>
    );
}
