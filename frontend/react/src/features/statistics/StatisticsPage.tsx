import {
    Box,
    Button,
    FormControl,
    Grid,
    InputLabel,
    MenuItem,
    Paper,
    Select,
    Skeleton,
    Stack,
    TextField,
    Typography,
    type SelectChangeEvent,
} from '@mui/material';
import { useTheme } from '@mui/material/styles';
import { BarChart } from '@mui/x-charts/BarChart';
import { LineChart } from '@mui/x-charts/LineChart';
import { PieChart } from '@mui/x-charts/PieChart';
import ArrowDownwardRoundedIcon from '@mui/icons-material/ArrowDownwardRounded';
import ArrowUpwardRoundedIcon from '@mui/icons-material/ArrowUpwardRounded';
import { useEffect, useMemo, useState, type ReactNode } from 'react';
import { PageHeader } from '../../shared/components/PageHeader';
import { ErrorAlert } from '../../shared/components/ErrorAlert';
import { useAccounts } from '../../shared/context/AccountsContext';
import { useAsyncData } from '../../shared/hooks/useAsyncData';
import { useSurfaceStyles } from '../../shared/hooks/useSurfaceStyles';
import { useButtonStyles } from '../../shared/hooks/useButtonStyles';
import { centsToUnits, formatAccountLabel, formatChartDayLabel, formatRon, monthStartIso, todayIso } from '../../shared/format';
import { fetchTransactionSummary } from '../transactions/api';
const TX_TYPES = ['All', 'Deposit', 'Withdrawal', 'Payment', 'Transfer', 'Send'] as const;
/** Must exceed benchmark seed size (100k rows); backend default is only 500. */
const STATS_PER_ACCOUNT_LIMIT = 200_000;
const CHART_HEIGHT = 260;
const CHART_PANEL_MIN_HEIGHT = 360;
const METRIC_CARD_HEIGHT = 128;

const DATE_PRESETS = [
    { id: 'month', label: 'This month', from: monthStartIso, to: todayIso },
    {
        id: '30d',
        label: 'Last 30 days',
        from: () => {
            const d = new Date();
            d.setDate(d.getDate() - 30);
            return d.toISOString().slice(0, 10);
        },
        to: todayIso,
    },
    {
        id: '90d',
        label: 'Last 90 days',
        from: () => {
            const d = new Date();
            d.setDate(d.getDate() - 90);
            return d.toISOString().slice(0, 10);
        },
        to: todayIso,
    },
] as const;

const TYPE_COLORS: Record<string, string> = {
    Deposit: '#22c55e',
    Withdrawal: '#ef4444',
    Payment: '#f97316',
    Transfer: '#3b82f6',
    Send: '#a855f7',
};

function MetricCard({
    label,
    value,
    hint,
    positive,
    panelSx,
}: {
    label: string;
    value: string;
    hint?: string;
    positive?: boolean;
    panelSx: object;
}) {
    const theme = useTheme();

    return (
        <Paper
            elevation={0}
            sx={{
                ...panelSx,
                height: METRIC_CARD_HEIGHT,
                display: 'flex',
                flexDirection: 'column',
                justifyContent: 'center',
            }}
        >
            <Typography variant="caption" color="text.secondary" sx={{ textTransform: 'uppercase', letterSpacing: 0.8 }}>
                {label}
            </Typography>
            <Stack direction="row" spacing={1} sx={{ alignItems: 'center', mt: 0.5 }}>
                <Typography variant="h5" sx={{ fontWeight: 600 }}>
                    {value}
                </Typography>
                {positive !== undefined && (
                    positive
                        ? <ArrowUpwardRoundedIcon fontSize="small" sx={{ color: theme.palette.success.main }} />
                        : <ArrowDownwardRoundedIcon fontSize="small" sx={{ color: theme.palette.error.main }} />
                )}
            </Stack>
            <Typography
                variant="body2"
                color="text.secondary"
                sx={{ mt: 0.5, minHeight: 20, visibility: hint ? 'visible' : 'hidden' }}
            >
                {hint ?? '\u00a0'}
            </Typography>
        </Paper>
    );
}

function ChartPanel({
    title,
    subtitle,
    panelSx,
    children,
    empty,
}: {
    title: string;
    subtitle?: string;
    panelSx: object;
    children: ReactNode;
    empty?: boolean;
}) {
    return (
        <Paper
            elevation={0}
            sx={{
                ...panelSx,
                height: CHART_PANEL_MIN_HEIGHT,
                width: '100%',
                display: 'flex',
                flexDirection: 'column',
            }}
        >
            <Typography variant="subtitle1" sx={{ fontWeight: 600 }}>
                {title}
            </Typography>
            {subtitle && (
                <Typography variant="body2" color="text.secondary" sx={{ mt: 0.5, mb: 1.5 }}>
                    {subtitle}
                </Typography>
            )}
            {!subtitle && <Box sx={{ mb: 1.5 }} />}
            <Box
                sx={{
                    flex: 1,
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'center',
                    minHeight: CHART_HEIGHT,
                }}
            >
                {empty ? (
                    <Typography color="text.secondary">No data for the selected filters.</Typography>
                ) : (
                    children
                )}
            </Box>
        </Paper>
    );
}

export function StatisticsPage() {
    const theme = useTheme();
    const { accounts } = useAccounts();
    const { cardBg, cardBorder, cardShadow, filterFieldSx } = useSurfaceStyles();
    const { softOutlined } = useButtonStyles();

    const [accountId, setAccountId] = useState<number | 'all'>('all');
    const [from, setFrom] = useState(monthStartIso());
    const [to, setTo] = useState(todayIso());
    const [typeFilter, setTypeFilter] = useState<string>('All');
    const [applied, setApplied] = useState({
        accountId: 'all' as number | 'all',
        from: monthStartIso(),
        to: todayIso(),
        typeFilter: 'All',
    });

    // Reset scope when the selected account no longer exists.
    useEffect(() => {
        if (accounts.length && accountId !== 'all' && !accounts.some((a) => a.id === accountId)) {
            // Fall back to "all" when the selected account disappears.
            // eslint-disable-next-line react-hooks/set-state-in-effect
            setAccountId('all');
        }
    }, [accounts, accountId]);

    // Fetch summary when applied filters change (not on every keystroke).
    const { data, isLoading, error } = useAsyncData(
        () =>
            fetchTransactionSummary({
                from: applied.from,
                to: applied.to,
                account_id: applied.accountId === 'all' ? undefined : Number(applied.accountId),
                transaction_type: applied.typeFilter,
                per_account_limit: STATS_PER_ACCOUNT_LIMIT,
            }),
        [applied.accountId, applied.from, applied.to, applied.typeFilter],
    );

    const typeChart = useMemo(() => ({
        labels: (data?.per_type_totals ?? []).map((t) => t.transaction_type),
        values: (data?.per_type_totals ?? []).map((t) => centsToUnits(t.total_cents)),
        colors: (data?.per_type_totals ?? []).map(
            (t) => TYPE_COLORS[t.transaction_type] ?? theme.palette.primary.main,
        ),
    }), [data?.per_type_totals, theme.palette.primary.main]);

    const balanceChart = useMemo(() => ({
        labels: (data?.account_balances ?? []).map((a) => `${a.account_type} ${a.currency}`),
        values: (data?.account_balances ?? []).map((a) => centsToUnits(a.balance_cents)),
    }), [data?.account_balances]);

    const netFlowChart = useMemo(() => ({
        labels: (data?.daily_net ?? []).map((d) => formatChartDayLabel(d.date)),
        values: (data?.daily_net ?? []).map((d) => centsToUnits(d.net_cents)),
    }), [data?.daily_net]);

    const spendingChart = useMemo(() => ({
        labels: (data?.daily_cumulative_spending ?? []).map((d) => formatChartDayLabel(d.date)),
        values: (data?.daily_cumulative_spending ?? []).map((d) => centsToUnits(d.cumulative_spending_cents)),
    }), [data?.daily_cumulative_spending]);

    const paymentPie = useMemo(
        () =>
            (data?.payment_category_totals ?? []).map((p, i) => ({
                id: i,
                label: p.category,
                value: centsToUnits(p.total_cents),
            })),
        [data?.payment_category_totals],
    );

    // Copy draft filter controls into applied state to trigger refetch.
    const applyFilters = () => {
        setApplied({ accountId, from, to, typeFilter });
    };

    // Set date range from preset and apply immediately.
    const applyPreset = (preset: (typeof DATE_PRESETS)[number]) => {
        const fromVal = typeof preset.from === 'function' ? preset.from() : preset.from;
        const toVal = typeof preset.to === 'function' ? preset.to() : preset.to;
        setFrom(fromVal);
        setTo(toVal);
        setApplied({ accountId, from: fromVal, to: toVal, typeFilter });
    };

    const panelSx = {
        p: 2.5,
        borderRadius: 3,
        bgcolor: cardBg,
        border: cardBorder,
        boxShadow: cardShadow,
    };

    return (
        <>
            <PageHeader
                title="Statistics"
                subtitle="All aggregates are computed by the backend service layer via GET /api/transactions/summary."
            />

            <ErrorAlert error={error} />

            <Paper elevation={0} sx={{ ...panelSx, mb: 3 }}>
                <Stack spacing={2}>
                    <Typography variant="subtitle1" sx={{ fontWeight: 600 }}>
                        Filters
                    </Typography>
                    <Grid container spacing={2} sx={{ alignItems: 'center' }}>
                        <Grid size={{ xs: 12, md: 3 }}>
                            <FormControl fullWidth sx={filterFieldSx}>
                                <InputLabel>Scope</InputLabel>
                                <Select
                                    label="Scope"
                                    value={accountId === 'all' ? 'all' : String(accountId)}
                                    onChange={(e: SelectChangeEvent) => {
                                        const v = e.target.value;
                                        setAccountId(v === 'all' ? 'all' : Number(v));
                                    }}
                                >
                                    <MenuItem value="all">All accounts</MenuItem>
                                    {accounts.map((a) => (
                                        <MenuItem key={a.id} value={String(a.id)}>
                                            {formatAccountLabel(a)}
                                        </MenuItem>
                                    ))}
                                </Select>
                            </FormControl>
                        </Grid>
                        <Grid size={{ xs: 12, sm: 6, md: 2 }}>
                            <TextField
                                label="From"
                                type="date"
                                value={from}
                                onChange={(e) => setFrom(e.target.value)}
                                fullWidth
                                slotProps={{ inputLabel: { shrink: true } }}
                                sx={filterFieldSx}
                            />
                        </Grid>
                        <Grid size={{ xs: 12, sm: 6, md: 2 }}>
                            <TextField
                                label="To"
                                type="date"
                                value={to}
                                onChange={(e) => setTo(e.target.value)}
                                fullWidth
                                slotProps={{ inputLabel: { shrink: true } }}
                                sx={filterFieldSx}
                            />
                        </Grid>
                        <Grid size={{ xs: 12, sm: 6, md: 2 }}>
                            <FormControl fullWidth sx={filterFieldSx}>
                                <InputLabel>Type</InputLabel>
                                <Select
                                    label="Type"
                                    value={typeFilter}
                                    onChange={(e: SelectChangeEvent) => setTypeFilter(e.target.value)}
                                >
                                    {TX_TYPES.map((t) => (
                                        <MenuItem key={t} value={t}>{t}</MenuItem>
                                    ))}
                                </Select>
                            </FormControl>
                        </Grid>
                        <Grid size={{ xs: 12, sm: 6, md: 3 }}>
                            <Button variant="outlined" onClick={applyFilters} sx={softOutlined} fullWidth>
                                Apply filters
                            </Button>
                        </Grid>
                    </Grid>
                    <Stack direction="row" spacing={1} sx={{ flexWrap: 'wrap', gap: 1 }}>
                        {DATE_PRESETS.map((preset) => (
                            <Button
                                key={preset.id}
                                size="small"
                                variant="text"
                                onClick={() => applyPreset(preset)}
                                sx={{ borderRadius: 999 }}
                            >
                                {preset.label}
                            </Button>
                        ))}
                    </Stack>
                </Stack>
            </Paper>

            {isLoading || !data ? (
                <Stack spacing={2}>
                    <Skeleton variant="rounded" height={120} />
                    <Grid container spacing={2}>
                        <Grid size={{ xs: 12, lg: 6 }}><Skeleton variant="rounded" height={CHART_PANEL_MIN_HEIGHT} /></Grid>
                        <Grid size={{ xs: 12, lg: 6 }}><Skeleton variant="rounded" height={CHART_PANEL_MIN_HEIGHT} /></Grid>
                    </Grid>
                </Stack>
            ) : (
                <Stack spacing={3}>
                    <Grid container spacing={2}>
                        <Grid size={{ xs: 12, sm: 6, md: 3 }}>
                            <MetricCard label="Money in" value={formatRon(data.total_incoming_cents)} positive panelSx={panelSx} />
                        </Grid>
                        <Grid size={{ xs: 12, sm: 6, md: 3 }}>
                            <MetricCard label="Money out" value={formatRon(data.total_outgoing_cents)} positive={false} panelSx={panelSx} />
                        </Grid>
                        <Grid size={{ xs: 12, sm: 6, md: 3 }}>
                            <MetricCard
                                label="Net flow"
                                value={formatRon(Math.abs(data.net_flow_cents))}
                                hint={data.net_flow_cents >= 0 ? 'Positive period' : 'Negative period'}
                                positive={data.net_flow_cents >= 0}
                                panelSx={panelSx}
                            />
                        </Grid>
                        <Grid size={{ xs: 12, sm: 6, md: 3 }}>
                            <MetricCard
                                label="Total volume"
                                value={formatRon(data.total_volume_cents)}
                                hint={`${data.transaction_count} transactions`}
                                panelSx={panelSx}
                            />
                        </Grid>
                    </Grid>

                    <Grid container spacing={2.5} sx={{ alignItems: 'stretch' }}>
                        <Grid size={{ xs: 12, lg: 6 }} sx={{ display: 'flex' }}>
                            <ChartPanel title="Volume by transaction type" panelSx={panelSx} empty={!typeChart.labels.length}>
                                <BarChart
                                    height={CHART_HEIGHT}
                                    xAxis={[{ scaleType: 'band', data: typeChart.labels }]}
                                    series={[{ data: typeChart.values }]}
                                    colors={typeChart.colors}
                                    margin={{ left: 56, right: 16, top: 16, bottom: 48 }}
                                />
                            </ChartPanel>
                        </Grid>

                        <Grid size={{ xs: 12, lg: 6 }} sx={{ display: 'flex' }}>
                            <ChartPanel title="Account balances" panelSx={panelSx} empty={!balanceChart.labels.length}>
                                <BarChart
                                    height={CHART_HEIGHT}
                                    xAxis={[{ scaleType: 'band', data: balanceChart.labels }]}
                                    series={[{
                                        data: balanceChart.values,
                                        color: theme.palette.secondary.main,
                                    }]}
                                    margin={{ left: 56, right: 16, top: 16, bottom: 56 }}
                                />
                            </ChartPanel>
                        </Grid>

                        <Grid size={{ xs: 12, lg: 6 }} sx={{ display: 'flex' }}>
                            <ChartPanel title="Daily net cash flow" panelSx={panelSx} empty={!netFlowChart.labels.length}>
                                <LineChart
                                    height={CHART_HEIGHT}
                                    xAxis={[{ scaleType: 'point', data: netFlowChart.labels }]}
                                    series={[{
                                        data: netFlowChart.values,
                                        color: theme.palette.info.main,
                                        showMark: false,
                                        curve: 'monotoneX',
                                    }]}
                                    margin={{ left: 56, right: 16, top: 16, bottom: 48 }}
                                />
                            </ChartPanel>
                        </Grid>

                        <Grid size={{ xs: 12, lg: 6 }} sx={{ display: 'flex' }}>
                            <ChartPanel
                                title="Cumulative spending"
                                subtitle="Outgoing payments and withdrawals only."
                                panelSx={panelSx}
                                empty={!spendingChart.labels.length}
                            >
                                <LineChart
                                    height={CHART_HEIGHT}
                                    xAxis={[{ scaleType: 'point', data: spendingChart.labels }]}
                                    series={[{
                                        data: spendingChart.values,
                                        color: theme.palette.warning.main,
                                        showMark: false,
                                        curve: 'monotoneX',
                                    }]}
                                    margin={{ left: 56, right: 16, top: 16, bottom: 48 }}
                                />
                            </ChartPanel>
                        </Grid>

                        <Grid size={12}>
                            <ChartPanel title="Payment categories" panelSx={panelSx} empty={!paymentPie.length}>
                                <PieChart
                                    height={CHART_HEIGHT}
                                    width={420}
                                    series={[{
                                        data: paymentPie,
                                        innerRadius: 48,
                                        paddingAngle: 2,
                                        cornerRadius: 4,
                                    }]}
                                />
                            </ChartPanel>
                        </Grid>
                    </Grid>
                </Stack>
            )}
        </>
    );
}
