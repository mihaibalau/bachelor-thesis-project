import {
    Avatar,
    Box,
    Button,
    Chip,
    Grid,
    IconButton,
    Paper,
    Stack,
    Typography,
    List,
    ListItem,
    ListItemAvatar,
    ListItemText,
    Divider,
    Tooltip,
} from '@mui/material';
import { alpha, useTheme } from '@mui/material/styles';
import AccountBalanceWalletOutlinedIcon from '@mui/icons-material/AccountBalanceWalletOutlined';
import PeopleAltOutlinedIcon from '@mui/icons-material/PeopleAltOutlined';
import TrendingUpOutlinedIcon from '@mui/icons-material/TrendingUpOutlined';
import ArrowDownwardRoundedIcon from '@mui/icons-material/ArrowDownwardRounded';
import ArrowUpwardRoundedIcon from '@mui/icons-material/ArrowUpwardRounded';
import { LineChart } from '@mui/x-charts/LineChart';
import { Link as RouterLink } from 'react-router-dom';
import { useMemo } from 'react';
import { useAuth } from '../auth/AuthContext';
import { useSurfaceStyles } from '../../shared/hooks/useSurfaceStyles';
import { pickWelcomeMessage } from './welcomeMessage';
import { ErrorAlert } from '../../shared/components/ErrorAlert';
import { useAccounts } from '../../shared/context/AccountsContext';
import { useDashboard } from './useDashboard';
import { DashboardSkeleton } from './components/DashboardSkeleton';
import {
    formatActivityTimestamp,
    formatPercent,
    formatRon,
    formatSignedRon,
    prepareSpendingChartData,
} from './format';
import {
    categoryIconStyle,
    getTransactionCategoryIcon,
    statCardIconStyle,
} from '../../shared/transactionIcons';

const RECENT_ACTIVITY_LIMIT = 5;

export function DashboardPage() {
    const { userId } = useAuth();
    const { user } = useAccounts();
    // useDashboard fetches /api/dashboard; AccountsContext supplies profile name.
    const { data, isLoading, error, reload } = useDashboard();
    const theme = useTheme();
    const { isDark, cardBg, cardBorder, cardShadow, heroBg } = useSurfaceStyles();
    const welcome = useMemo(() => pickWelcomeMessage(), []);

    const balanceChangePositive = (data?.balance_change_percent ?? 0) >= 0;
    const spendingChangeNegative =
        (data?.spending.change_percent_vs_last_month ?? 0) < 0;

    const { labels: chartLabels, values: chartValues } = prepareSpendingChartData(
        data?.spending.daily_cumulative ?? [],
    );

    return (
        <>
            <ErrorAlert error={error} onRetry={() => void reload()} />

            {isLoading || !data ? (
                <DashboardSkeleton />
            ) : (
                <Stack spacing={3}>
                    <Paper
                        sx={{
                            p: { xs: 2.5, md: 3 },
                            display: 'flex',
                            flexDirection: { xs: 'column', md: 'row' },
                            alignItems: { xs: 'flex-start', md: 'center' },
                            justifyContent: 'space-between',
                            gap: 2,
                            borderRadius: 4,
                            background: heroBg,
                            border: `1px solid ${alpha(
                                theme.palette.divider,
                                0.9,
                            )}`,
                            boxShadow: cardShadow,
                            transition: 'background 200ms ease, box-shadow 200ms ease',
                        }}
                    >
                        <Box>
                            <Typography variant="h4" sx={{ mb: 1 }}>
                                {welcome.heading}
                            </Typography>
                            <Typography
                                variant="body2"
                                sx={{ color: 'text.secondary' }}
                            >
                            {user ? (
                                welcome.subtitle(user.first_name)
                            ) : (
                                <>You are signed in with user ID <strong>{userId}</strong>.</>
                            )}
                            </Typography>
                        </Box>

                        <Stack
                            sx={{
                                display: 'flex',
                                flexDirection: { xs: 'row', md: 'column' },
                                gap: 1.5,
                                alignItems: { xs: 'center', md: 'flex-end' },
                            }}
                        >
                            <Typography
                                variant="caption"
                                sx={{
                                    color: 'text.secondary',
                                    textTransform: 'uppercase',
                                    letterSpacing: 1,
                                }}
                            >
                                Total balance
                            </Typography>
                            <Typography
                                variant="h5"
                                sx={{
                                    fontWeight: 600,
                                    color: 'primary.main',
                                    textShadow: isDark
                                        ? '0 0 16px rgba(245,196,81,0.35)'
                                        : 'none',
                                }}
                            >
                                {formatRon(data.total_balance_cents)}
                            </Typography>
                            <Typography
                                variant="body2"
                                sx={{
                                    color: balanceChangePositive
                                        ? theme.palette.success.main
                                        : theme.palette.error.main,
                                    display: 'flex',
                                    gap: 0.5,
                                    alignItems: 'center',
                                }}
                            >
                                {balanceChangePositive ? (
                                    <TrendingUpOutlinedIcon fontSize="small" />
                                ) : (
                                    <ArrowDownwardRoundedIcon fontSize="small" />
                                )}
                                {formatPercent(data.balance_change_percent)} this
                                month
                            </Typography>
                        </Stack>
                    </Paper>

                    <Grid
                        container
                        spacing={2.5}
                        sx={{
                            mt: 2.5,
                            alignItems: 'stretch',
                        }}
                    >
                    <Grid size={{ xs: 12, md: 4 }}>
                        <Paper
                            component={RouterLink}
                            to="/app/accounts"
                            sx={{
                                p: 2.5,
                                minHeight: 140,
                                height: '100%',
                                display: 'flex',
                                flexDirection: 'column',
                                justifyContent: 'space-between',
                                borderRadius: 3,
                                bgcolor: cardBg,
                                border: cardBorder,
                                boxShadow: cardShadow,
                                textDecoration: 'none',
                                color: 'inherit',
                                transition: 'transform 120ms ease',
                                '&:hover': { transform: 'translateY(-2px)' },
                            }}
                        >
                            <Stack
                                sx={{
                                    display: 'flex',
                                    flexDirection: 'row',
                                    justifyContent: 'space-between',
                                    alignItems: 'center',
                                    mb: 1.5,
                                }}
                            >
                                <Typography
                                    variant="subtitle2"
                                    sx={{ color: 'text.secondary' }}
                                >
                                    Accounts
                                </Typography>
                                    <Avatar sx={statCardIconStyle(theme, 'primary')}>
                                        <AccountBalanceWalletOutlinedIcon fontSize="small" />
                                    </Avatar>
                                </Stack>

                                <Typography variant="h5">
                                    {data.active_accounts_count} active
                                </Typography>
                            </Paper>
                        </Grid>

                    <Grid size={{ xs: 12, md: 4 }}>
                        <Paper
                            component={RouterLink}
                            to="/app/affiliates"
                            sx={{
                                p: 2.5,
                                minHeight: 140,
                                height: '100%',
                                display: 'flex',
                                flexDirection: 'column',
                                justifyContent: 'space-between',
                                borderRadius: 3,
                                bgcolor: cardBg,
                                border: `1px solid ${alpha(
                                    theme.palette.divider,
                                    0.9,
                                )}`,
                                boxShadow: cardShadow,
                                textDecoration: 'none',
                                color: 'inherit',
                                transition: 'transform 120ms ease',
                                '&:hover': { transform: 'translateY(-2px)' },
                            }}
                        >
                            <Stack
                                sx={{
                                    display: 'flex',
                                    flexDirection: 'row',
                                    justifyContent: 'space-between',
                                    alignItems: 'center',
                                    mb: 1.5,
                                }}
                            >
                                <Typography
                                    variant="subtitle2"
                                    sx={{ color: 'text.secondary' }}
                                >
                                    Affiliates
                                </Typography>
                                    <Avatar sx={statCardIconStyle(theme, 'secondary')}>
                                        <PeopleAltOutlinedIcon fontSize="small" />
                                    </Avatar>
                                </Stack>

                                <Typography variant="h5">
                                    {data.affiliates_count} saved
                                </Typography>
                            </Paper>
                        </Grid>

                    <Grid size={{ xs: 12, md: 4 }}>
                        <Paper
                            component={RouterLink}
                            to="/app/transactions?tab=Send"
                            sx={{
                                p: 2.5,
                                minHeight: 140,
                                height: '100%',
                                display: 'flex',
                                flexDirection: 'column',
                                justifyContent: 'space-between',
                                borderRadius: 3,
                                bgcolor: cardBg,
                                border: `1px solid ${alpha(
                                    theme.palette.divider,
                                    0.9,
                                )}`,
                                boxShadow: cardShadow,
                                textDecoration: 'none',
                                color: 'inherit',
                                transition: 'transform 120ms ease',
                                '&:hover': { transform: 'translateY(-2px)' },
                            }}
                        >
                            <Stack
                                sx={{
                                    display: 'flex',
                                    flexDirection: 'row',
                                    justifyContent: 'space-between',
                                    alignItems: 'center',
                                    mb: 1.5,
                                }}
                            >
                                <Typography
                                    variant="subtitle2"
                                    sx={{ color: 'text.secondary' }}
                                >
                                    Transfers
                                </Typography>
                                    <Avatar sx={statCardIconStyle(theme, 'success')}>
                                        <TrendingUpOutlinedIcon fontSize="small" />
                                    </Avatar>
                                </Stack>

                                <Typography variant="h5">
                                    {formatRon(data.transfers_total_cents, false)}
                                </Typography>
                            </Paper>
                        </Grid>
                    </Grid>

                    <Grid
                        container
                        spacing={2.5}
                        sx={{
                            mt: 1,
                            alignItems: 'stretch',
                        }}
                    >
                        <Grid size={{ xs: 12, md: 7 }}>
                            <Paper
                                sx={{
                                    p: 2.5,
                                    height: '100%',
                                    borderRadius: 3,
                                    bgcolor: cardBg,
                                    border: `1px solid ${alpha(
                                        theme.palette.divider,
                                        0.9,
                                    )}`,
                                    boxShadow: cardShadow,
                                    display: 'flex',
                                    flexDirection: 'column',
                                }}
                            >
                                <Stack
                                    direction="row"
                                    sx={{
                                        justifyContent: 'space-between',
                                        alignItems: 'center',
                                        mb: 1.5,
                                    }}
                                >
                                    <Box>
                                        <Typography variant="subtitle1">
                                            Recent activity
                                        </Typography>
                                        <Typography
                                            variant="caption"
                                            sx={{ color: 'text.secondary' }}
                                        >
                                            Latest {RECENT_ACTIVITY_LIMIT} transactions
                                        </Typography>
                                    </Box>
                                    <Stack
                                        direction="row"
                                        spacing={1}
                                        sx={{ alignItems: 'center' }}
                                    >
                                        <Chip
                                            size="small"
                                            label="This month"
                                            color="primary"
                                            variant="outlined"
                                            sx={{
                                                borderRadius: 999,
                                                borderColor: alpha(
                                                    theme.palette.primary.main,
                                                    0.6,
                                                ),
                                                color: isDark
                                                    ? alpha(
                                                        theme.palette
                                                            .primary.light,
                                                        0.96,
                                                    )
                                                    : theme.palette.primary.dark,
                                                backgroundColor: isDark
                                                    ? alpha(
                                                        theme.palette
                                                            .background.default,
                                                        0.9,
                                                    )
                                                    : alpha(
                                                        theme.palette
                                                            .primary.light,
                                                        0.06,
                                                    ),
                                            }}
                                        />
                                        <Button
                                            component={RouterLink}
                                            to="/app/transactions"
                                            size="small"
                                            variant="contained"
                                            disableElevation
                                            sx={{
                                                textTransform: 'none',
                                                borderRadius: 999,
                                                px: 2,
                                                py: 0.35,
                                                minWidth: 0,
                                                fontWeight: 600,
                                                fontSize: '0.75rem',
                                                lineHeight: 1.4,
                                                color: isDark
                                                    ? '#050509'
                                                    : '#1a1206',
                                                background: isDark
                                                    ? `linear-gradient(135deg, ${theme.palette.primary.light} 0%, ${theme.palette.primary.main} 55%, ${theme.palette.primary.dark} 100%)`
                                                    : `linear-gradient(135deg, ${theme.palette.primary.main} 0%, ${theme.palette.primary.dark} 100%)`,
                                                border: `1px solid ${alpha(theme.palette.primary.dark, 0.55)}`,
                                                boxShadow: isDark
                                                    ? `0 4px 14px ${alpha(theme.palette.primary.main, 0.28)}`
                                                    : `0 4px 12px ${alpha(theme.palette.primary.dark, 0.25)}`,
                                                '&:hover': {
                                                    background: isDark
                                                        ? `linear-gradient(135deg, ${theme.palette.primary.main} 0%, ${theme.palette.primary.dark} 100%)`
                                                        : `linear-gradient(135deg, ${theme.palette.primary.light} 0%, ${theme.palette.primary.main} 100%)`,
                                                    boxShadow: isDark
                                                        ? `0 6px 18px ${alpha(theme.palette.primary.main, 0.38)}`
                                                        : `0 6px 16px ${alpha(theme.palette.primary.dark, 0.32)}`,
                                                },
                                            }}
                                        >
                                            View all
                                        </Button>
                                    </Stack>
                                </Stack>

                                <Divider
                                    sx={{
                                        mb: 1.5,
                                        borderColor: alpha(
                                            theme.palette.divider,
                                            0.9,
                                        ),
                                    }}
                                />

                                <List
                                    dense
                                    sx={{
                                        m: 0,
                                        p: 0,
                                        minHeight: 260,
                                        '& .MuiListItem-root': {
                                            borderRadius: 1.5,
                                        },
                                    }}
                                >
                                    {data.recent_activity.slice(0, RECENT_ACTIVITY_LIMIT).map((tx) => {
                                        const { date, time } =
                                            formatActivityTimestamp(
                                                tx.recorded_on,
                                            );
                                        const amountColor = tx.is_income
                                            ? theme.palette.success.main
                                            : theme.palette.text.primary;

                                        return (
                                            <ListItem
                                                key={tx.id}
                                                sx={{
                                                    mb: 0.5,
                                                    px: 1.25,
                                                    transition:
                                                        'background-color 120ms ease',
                                                    '&:hover': {
                                                        bgcolor: isDark
                                                            ? alpha(
                                                                theme.palette
                                                                    .background
                                                                    .default,
                                                                0.9,
                                                            )
                                                            : alpha(
                                                                theme.palette
                                                                    .grey['100'],
                                                                0.8,
                                                            ),
                                                    },
                                                }}
                                                secondaryAction={
                                                    <Typography
                                                        variant="subtitle2"
                                                        sx={{
                                                            color: amountColor,
                                                            fontWeight: 600,
                                                        }}
                                                    >
                                                        {formatSignedRon(
                                                            tx.amount_cents,
                                                        )}
                                                    </Typography>
                                                }
                                            >
                                                <ListItemAvatar>
                                                    <Avatar
                                                        sx={{
                                                            width: 36,
                                                            height: 36,
                                                            ...categoryIconStyle(tx.category, isDark),
                                                        }}
                                                    >
                                                        {getTransactionCategoryIcon(tx.category, 'small')}
                                                    </Avatar>
                                                </ListItemAvatar>
                                                <ListItemText
                                                    primary={
                                                        <Stack
                                                            direction="row"
                                                            spacing={1}
                                                            sx={{
                                                                alignItems:
                                                                    'center',
                                                            }}
                                                        >
                                                            <Typography
                                                                variant="body2"
                                                                sx={{
                                                                    fontWeight: 500,
                                                                }}
                                                            >
                                                                {tx.label}
                                                            </Typography>
                                                            {tx.category ===
                                                                'salary' && (
                                                                <Chip
                                                                    size="small"
                                                                    label="Income"
                                                                    color="success"
                                                                    sx={{
                                                                        height: 18,
                                                                        fontSize: 10,
                                                                        borderRadius: 999,
                                                                    }}
                                                                />
                                                            )}
                                                        </Stack>
                                                    }
                                                    secondary={
                                                        <Typography
                                                            variant="caption"
                                                            sx={{
                                                                color: 'text.secondary',
                                                            }}
                                                        >
                                                            {tx.description} ·{' '}
                                                            {date} · {time}
                                                        </Typography>
                                                    }
                                                />
                                            </ListItem>
                                        );
                                    })}
                                </List>
                            </Paper>
                        </Grid>

                        <Grid size={{ xs: 12, md: 5 }}>
                            <Paper
                                sx={{
                                    p: 2.5,
                                    height: '100%',
                                    borderRadius: 3,
                                    background: isDark
                                        ? `linear-gradient(135deg,
                                            ${theme.palette.background.default} 0%,
                                            ${alpha(
                                            theme.palette.success.main,
                                            0.12,
                                        )} 100%)`
                                        : `linear-gradient(135deg,
                                            ${theme.palette.background.paper} 0%,
                                            ${alpha(
                                            theme.palette.success.light,
                                            0.16,
                                        )} 100%)`,
                                    border: `1px solid ${alpha(
                                        theme.palette.success.main,
                                        0.6,
                                    )}`,
                                    boxShadow: cardShadow,
                                    display: 'flex',
                                    flexDirection: 'column',
                                    overflow: 'hidden',
                                }}
                            >
                                <Stack
                                    direction="row"
                                    sx={{
                                        justifyContent: 'space-between',
                                        alignItems: 'flex-start',
                                        mb: 1.5,
                                    }}
                                >
                                    <Box>
                                        <Typography
                                            variant="subtitle1"
                                            sx={{ display: 'flex', gap: 0.75 }}
                                        >
                                            Spending this month
                                            <Tooltip title="Cumulative spending — each point shows your total spend from the start of the month through that day.">
                                                <IconButton
                                                    size="small"
                                                    sx={{
                                                        p: 0,
                                                        color: 'text.secondary',
                                                    }}
                                                >
                                                    <ArrowUpwardRoundedIcon fontSize="inherit" />
                                                </IconButton>
                                            </Tooltip>
                                        </Typography>
                                        <Typography
                                            variant="caption"
                                            sx={{ color: 'text.secondary' }}
                                        >
                                            Cumulative by day
                                        </Typography>
                                    </Box>

                                    <Stack
                                        spacing={0.5}
                                        sx={{
                                            alignItems: 'flex-end',
                                            textAlign: 'right',
                                        }}
                                    >
                                        <Typography
                                            variant="caption"
                                            sx={{
                                                color: 'text.secondary',
                                                textTransform: 'uppercase',
                                            }}
                                        >
                                            Total spent
                                        </Typography>
                                        <Typography
                                            variant="h5"
                                            sx={{
                                                fontWeight: 600,
                                                color: theme.palette.error.main,
                                            }}
                                        >
                                            {formatRon(
                                                data.spending.total_spent_cents,
                                            )}
                                        </Typography>
                                        <Stack
                                            direction="row"
                                            spacing={0.5}
                                            sx={{ alignItems: 'center' }}
                                        >
                                            {spendingChangeNegative ? (
                                                <ArrowDownwardRoundedIcon
                                                    fontSize="small"
                                                    color="success"
                                                />
                                            ) : (
                                                <ArrowUpwardRoundedIcon
                                                    fontSize="small"
                                                    color="error"
                                                />
                                            )}
                                            <Typography
                                                variant="caption"
                                                sx={{
                                                    color: spendingChangeNegative
                                                        ? theme.palette.success
                                                            .main
                                                        : theme.palette.error
                                                            .main,
                                                }}
                                            >
                                                {formatPercent(
                                                    data.spending
                                                        .change_percent_vs_last_month,
                                                )}{' '}
                                                vs last month
                                            </Typography>
                                        </Stack>
                                    </Stack>
                                </Stack>

                                <Box
                                    sx={{
                                        mt: 1,
                                        flex: 1,
                                        display: 'flex',
                                        alignItems: 'center',
                                        justifyContent: 'center',
                                        px: 1,
                                        '& .MuiChartsLegend-root': {
                                            display: 'none',
                                        },
                                    }}
                                >
                                    <LineChart
                                        height={220}
                                        xAxis={[
                                            {
                                                data: chartLabels,
                                                scaleType: 'point',
                                            },
                                        ]}
                                        series={[
                                            {
                                                data: chartValues,
                                                area: true,
                                                showMark: false,
                                                color: theme.palette.success
                                                    .main,
                                            },
                                        ]}
                                        margin={{
                                            left: 32,
                                            right: 32,
                                            top: 10,
                                            bottom: 32,
                                        }}
                                        grid={{ horizontal: true }}
                                        sx={{
                                            width: '100%',
                                            '& .MuiChartsAxis-left .MuiChartsAxis-line':
                                                {
                                                    stroke: alpha(
                                                        theme.palette.divider,
                                                        0.8,
                                                    ),
                                                },
                                            '& .MuiChartsAxis-bottom .MuiChartsAxis-line':
                                                {
                                                    stroke: alpha(
                                                        theme.palette.divider,
                                                        0.8,
                                                    ),
                                                },
                                            '& .MuiChartsAxis-tickLabel': {
                                                fill: alpha(
                                                    theme.palette.text.secondary,
                                                    0.9,
                                                ),
                                                fontSize: 11,
                                            },
                                            '& .MuiLineElement-root': {
                                                strokeWidth: 2.4,
                                            },
                                            '& .MuiAreaElement-root': {
                                                fill: alpha(
                                                    theme.palette.success.main,
                                                    0.3,
                                                ),
                                            },
                                            '& .MuiChartsGrid-root line': {
                                                stroke: alpha(
                                                    theme.palette.divider,
                                                    0.6,
                                                ),
                                            },
                                        }}
                                    />
                                </Box>
                            </Paper>
                        </Grid>
                    </Grid>
                </Stack>
            )}
        </>
    );
}
