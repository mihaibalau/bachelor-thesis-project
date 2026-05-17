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
import LogoutRoundedIcon from '@mui/icons-material/LogoutRounded';
import AccountBalanceWalletOutlinedIcon from '@mui/icons-material/AccountBalanceWalletOutlined';
import PeopleAltOutlinedIcon from '@mui/icons-material/PeopleAltOutlined';
import TrendingUpOutlinedIcon from '@mui/icons-material/TrendingUpOutlined';
import NotificationsNoneOutlinedIcon from '@mui/icons-material/NotificationsNoneOutlined';
import ShoppingCartOutlinedIcon from '@mui/icons-material/ShoppingCartOutlined';
import RestaurantOutlinedIcon from '@mui/icons-material/RestaurantOutlined';
import LocalAtmOutlinedIcon from '@mui/icons-material/LocalAtmOutlined';
import ArrowDownwardRoundedIcon from '@mui/icons-material/ArrowDownwardRounded';
import ArrowUpwardRoundedIcon from '@mui/icons-material/ArrowUpwardRounded';
import DarkModeOutlinedIcon from '@mui/icons-material/DarkModeOutlined';
import LightModeOutlinedIcon from '@mui/icons-material/LightModeOutlined';
import { LineChart } from '@mui/x-charts/LineChart';
import type { ReactNode } from 'react';
import { useAuth } from '../auth/AuthContext';
import { useColorMode } from '../theme/useColorMode';
import gentlixLogo from '../../assets/logo.png';

type TransactionCategory = 'shopping' | 'food' | 'atm' | 'transfer' | 'salary';

type Transaction = {
    id: number;
    label: string;
    description: string;
    date: string;
    time: string;
    amount: number;
    category: TransactionCategory;
};

const mockTransactions: Transaction[] = [
    {
        id: 1,
        label: 'Mega Image',
        description: 'Groceries & household',
        date: 'Mon, 4 May',
        time: '09:36',
        amount: -126.75,
        category: 'shopping',
    },
    {
        id: 2,
        label: 'Glovo',
        description: 'Dinner with friends',
        date: 'Mon, 4 May',
        time: '20:18',
        amount: -89.5,
        category: 'food',
    },
    {
        id: 3,
        label: 'ATM Withdrawal',
        description: 'Cash out',
        date: 'Tue, 5 May',
        time: '13:02',
        amount: -400,
        category: 'atm',
    },
    {
        id: 4,
        label: 'Spotify',
        description: 'Subscription',
        date: 'Wed, 6 May',
        time: '07:11',
        amount: -29.99,
        category: 'shopping',
    },
    {
        id: 5,
        label: 'Transfer – Andrei',
        description: 'Rent share',
        date: 'Thu, 7 May',
        time: '18:42',
        amount: -950,
        category: 'transfer',
    },
    {
        id: 6,
        label: 'Salary',
        description: 'Gentlix Bank',
        date: 'Fri, 8 May',
        time: '08:00',
        amount: 6500,
        category: 'salary',
    },
];

const spendingByDay = [
    { day: 'Mon', amount: 100 },
    { day: 'Tue', amount: 50 },
    { day: 'Wed', amount: 80 },
    { day: 'Thu', amount: 40 },
    { day: 'Fri', amount: 120 },
    { day: 'Sat', amount: 60 },
    { day: 'Sun', amount: 30 },
];

const cumulativeSpending = spendingByDay.reduce<
    { day: string; total: number }[]
>((acc, item) => {
    const prevTotal = acc.length > 0 ? acc[acc.length - 1].total : 0;
    acc.push({ day: item.day, total: prevTotal + item.amount });
    return acc;
}, []);

const monthlyTotal = cumulativeSpending[cumulativeSpending.length - 1]?.total ?? 0;

function getTransactionIcon(category: TransactionCategory) {
    switch (category) {
        case 'shopping':
            return <ShoppingCartOutlinedIcon fontSize="small" />;
        case 'food':
            return <RestaurantOutlinedIcon fontSize="small" />;
        case 'atm':
            return <LocalAtmOutlinedIcon fontSize="small" />;
        case 'transfer':
            return <TrendingUpOutlinedIcon fontSize="small" />;
        case 'salary':
            return <ArrowUpwardRoundedIcon fontSize="small" />;
        default:
            return <ShoppingCartOutlinedIcon fontSize="small" />;
    }
}

function Shell({ children }: { children: ReactNode }) {
    const { logout } = useAuth();
    const { mode, toggleMode } = useColorMode();
    const theme = useTheme();
    const isDark = theme.palette.mode === 'dark';

    const headerBg = isDark
        ? `linear-gradient(90deg,
            ${theme.palette.background.default} 0%,
            ${theme.palette.background.default} 35%,
            ${alpha(theme.palette.background.paper, 0.96)} 75%,
            ${alpha(theme.palette.background.paper, 0.98)} 100%)`
        : `linear-gradient(90deg,
            ${theme.palette.background.paper} 0%,
            ${theme.palette.background.paper} 55%,
            ${alpha(theme.palette.primary.light, 0.18)} 100%)`;

    return (
        <Box
            sx={{
                minHeight: '100vh',
                bgcolor: 'background.default',
                color: 'text.primary',
                display: 'flex',
                flexDirection: 'column',
                transition: 'background-color 200ms ease',
            }}
        >
            {/* Top bar */}
            <Box
                component="header"
                sx={{
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'space-between',
                    px: { xs: 2.5, md: 4 },
                    py: 2,
                    borderBottom: '1px solid',
                    borderColor: 'divider',
                    backdropFilter: 'blur(18px)',
                    background: headerBg,
                    boxShadow: isDark
                        ? '0 18px 40px rgba(0,0,0,0.6)'
                        : '0 10px 24px rgba(15,23,42,0.12)',
                    position: 'sticky',
                    top: 0,
                    zIndex: 10,
                    transition:
                        'background 200ms ease, box-shadow 200ms ease, border-color 200ms ease',
                }}
            >
                <Stack
                    direction="row"
                    spacing={1.5}
                    sx={{ alignItems: 'center' }}
                >
                    <Box
                        component="img"
                        src={gentlixLogo}
                        alt="Gentlix Bank logo"
                        sx={{ width: 36, height: 36, borderRadius: 1.5 }}
                    />
                    <Box>
                        <Typography
                            variant="subtitle1"
                            sx={{ fontWeight: 600, lineHeight: 1.1 }}
                        >
                            Gentlix Bank
                        </Typography>
                        <Typography
                            variant="caption"
                            sx={{
                                color: 'text.secondary',
                                textTransform: 'uppercase',
                                letterSpacing: 1,
                            }}
                        >
                            Client dashboard
                        </Typography>
                    </Box>
                    <Chip
                        size="small"
                        label="Sandbox"
                        sx={{
                            ml: 1,
                            borderRadius: 999,
                            bgcolor: alpha(theme.palette.primary.main, 0.12),
                            color: 'primary.main',
                            border: `1px solid ${alpha(
                                theme.palette.primary.main,
                                0.5,
                            )}`,
                            fontWeight: 500,
                        }}
                    />
                </Stack>

                <Stack
                    direction="row"
                    spacing={1.25}
                    sx={{ alignItems: 'center' }}
                >
                    {/* Light/Dark toggle */}
                    <IconButton
                        color="inherit"
                        onClick={toggleMode}
                        sx={{
                            borderRadius: 10,
                            bgcolor: isDark
                                ? alpha(theme.palette.background.paper, 0.4)
                                : alpha(theme.palette.primary.main, 0.06),
                            border: `1px solid ${alpha(
                                theme.palette.divider,
                                0.9,
                            )}`,
                            '&:hover': {
                                bgcolor: isDark
                                    ? alpha(
                                        theme.palette.background.paper,
                                        0.7,
                                    )
                                    : alpha(
                                        theme.palette.primary.main,
                                        0.12,
                                    ),
                                borderColor: theme.palette.primary.main,
                            },
                        }}
                    >
                        {mode === 'dark' ? (
                            <LightModeOutlinedIcon fontSize="small" />
                        ) : (
                            <DarkModeOutlinedIcon fontSize="small" />
                        )}
                    </IconButton>

                    <IconButton
                        color="inherit"
                        sx={{
                            borderRadius: 10,
                            bgcolor: isDark
                                ? alpha(theme.palette.background.paper, 0.4)
                                : alpha(theme.palette.grey[200], 0.8),
                            border: `1px solid ${alpha(
                                theme.palette.divider,
                                0.9,
                            )}`,
                            '&:hover': {
                                bgcolor: isDark
                                    ? alpha(
                                        theme.palette.background.paper,
                                        0.7,
                                    )
                                    : alpha(theme.palette.grey[200], 1),
                            },
                        }}
                    >
                        <NotificationsNoneOutlinedIcon />
                    </IconButton>

                    <Button
                        variant="outlined"
                        color="inherit"
                        size="small"
                        startIcon={<LogoutRoundedIcon />}
                        onClick={logout}
                        sx={{
                            borderRadius: 10,
                            borderColor: alpha(theme.palette.divider, 0.9),
                            backgroundColor: isDark
                                ? alpha(theme.palette.background.paper, 0.5)
                                : alpha(theme.palette.common.white, 0.8),
                            px: 2.5,
                            '&:hover': {
                                borderColor: theme.palette.primary.main,
                                backgroundColor: isDark
                                    ? alpha(
                                        theme.palette.background.paper,
                                        0.9,
                                    )
                                    : alpha(
                                        theme.palette.primary.light,
                                        0.12,
                                    ),
                                boxShadow: isDark
                                    ? '0 18px 30px rgba(0,0,0,0.7)'
                                    : '0 10px 24px rgba(15,23,42,0.18)',
                            },
                        }}
                    >
                        Sign out
                    </Button>
                </Stack>
            </Box>

            <Box
                component="main"
                sx={{
                    flex: 1,
                    px: { xs: 2.5, md: 4 },
                    py: { xs: 3, md: 4 },
                    maxWidth: 1280,
                    width: '100%',
                    mx: 'auto',
                    transition: 'padding 200ms ease',
                }}
            >
                {children}
            </Box>
        </Box>
    );
}

export function DashboardPage() {
    const { userId } = useAuth();
    const theme = useTheme();
    const isDark = theme.palette.mode === 'dark';

    const heroBg = isDark
        ? `linear-gradient(120deg,
            ${theme.palette.background.default} 0%,
            ${theme.palette.background.default} 35%,
            ${alpha(theme.palette.background.paper, 0.96)} 70%,
            ${alpha(theme.palette.background.paper, 0.98)} 100%)`
        : `linear-gradient(120deg,
            ${theme.palette.background.paper} 0%,
            ${alpha(theme.palette.primary.light, 0.12)} 60%,
            ${alpha(theme.palette.secondary.light, 0.08)} 100%)`;

    const cardBg = isDark
        ? alpha(theme.palette.background.paper, 0.9)
        : theme.palette.background.paper;

    const cardShadow = isDark
        ? '0 18px 40px rgba(0,0,0,0.7)'
        : '0 12px 30px rgba(15,23,42,0.12)';

    return (
        <Shell>
            <Stack spacing={3}>
                {/* Greeting + highlight band */}
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
                            Welcome back
                        </Typography>
                        <Typography
                            variant="body2"
                            sx={{ color: 'text.secondary' }}
                        >
                            You are signed in with user ID{' '}
                            <strong>{userId}</strong>.
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
                            RON 12,480.23
                        </Typography>
                        <Typography
                            variant="body2"
                            sx={{
                                color: theme.palette.success.main,
                                display: 'flex',
                                gap: 0.5,
                                alignItems: 'center',
                            }}
                        >
                            <TrendingUpOutlinedIcon fontSize="small" />
                            +3.8% this month
                        </Typography>
                    </Stack>
                </Paper>

                {/* KPI cards row – Accounts / Affiliates / Transfers */}
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
                                <Avatar
                                    sx={{
                                        width: 30,
                                        height: 30,
                                        bgcolor: alpha(
                                            theme.palette.primary.main,
                                            0.08,
                                        ),
                                        color: 'primary.main',
                                        border: `1px solid ${alpha(
                                            theme.palette.primary.main,
                                            0.6,
                                        )}`,
                                    }}
                                >
                                    <AccountBalanceWalletOutlinedIcon fontSize="small" />
                                </Avatar>
                            </Stack>

                            <Typography variant="h5">1 active</Typography>
                        </Paper>
                    </Grid>

                    <Grid size={{ xs: 12, md: 4 }}>
                        <Paper
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
                                <Avatar
                                    sx={{
                                        width: 30,
                                        height: 30,
                                        bgcolor: alpha(
                                            theme.palette.secondary.main,
                                            0.08,
                                        ),
                                        color: 'secondary.main',
                                        border: `1px solid ${alpha(
                                            theme.palette.secondary.main,
                                            0.6,
                                        )}`,
                                    }}
                                >
                                    <PeopleAltOutlinedIcon fontSize="small" />
                                </Avatar>
                            </Stack>

                            <Typography variant="h5">3 saved</Typography>
                        </Paper>
                    </Grid>

                    <Grid size={{ xs: 12, md: 4 }}>
                        <Paper
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
                                <Avatar
                                    sx={{
                                        width: 30,
                                        height: 30,
                                        bgcolor: alpha(
                                            theme.palette.success.main,
                                            0.12,
                                        ),
                                        color: 'success.main',
                                        border: `1px solid ${alpha(
                                            theme.palette.success.main,
                                            0.6,
                                        )}`,
                                    }}
                                >
                                    <TrendingUpOutlinedIcon fontSize="small" />
                                </Avatar>
                            </Stack>

                            <Typography variant="h5">RON 4,200</Typography>
                        </Paper>
                    </Grid>
                </Grid>

                {/* Bottom section – Transactions list + cumulative spending chart */}
                <Grid
                    container
                    spacing={2.5}
                    sx={{
                        mt: 1,
                        alignItems: 'stretch',
                    }}
                >
                    {/* Left: recent transactions */}
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
                                        Last 7 days · Mock data
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
                                        size="small"
                                        color="inherit"
                                        sx={{
                                            textTransform: 'none',
                                            color: 'text.secondary',
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
                                    '& .MuiListItem-root': {
                                        borderRadius: 1.5,
                                    },
                                }}
                            >
                                {mockTransactions.map((tx) => {
                                    const isPositive = tx.amount > 0;
                                    const amountColor = isPositive
                                        ? theme.palette.success.main
                                        : theme.palette.text.primary;

                                    return (
                                        <ListItem
                                            key={tx.id}
                                            sx={{
                                                mb: 0.5,
                                                px: 1.25,
                                                transition: 'background-color 120ms ease',
                                                '&:hover': {
                                                    bgcolor: isDark
                                                        ? alpha(theme.palette.background.default, 0.9)
                                                        : alpha(theme.palette.grey['100'], 0.8),
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
                                                    {isPositive ? '+' : '-'}
                                                    {`RON ${Math.abs(tx.amount).toFixed(2)}`}
                                                </Typography>
                                            }
                                        >
                                            <ListItemAvatar>
                                                <Avatar
                                                    sx={{
                                                        width: 32,
                                                        height: 32,
                                                        bgcolor: isPositive
                                                            ? alpha(
                                                                theme
                                                                    .palette
                                                                    .success
                                                                    .main,
                                                                0.16,
                                                            )
                                                            : alpha(
                                                                theme
                                                                    .palette
                                                                    .primary
                                                                    .main,
                                                                0.08,
                                                            ),
                                                    }}
                                                >
                                                    {getTransactionIcon(
                                                        tx.category,
                                                    )}
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
                                                                        fontSize:
                                                                            10,
                                                                        borderRadius:
                                                                            999,
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
                                                        {tx.date} · {tx.time}
                                                    </Typography>
                                                }
                                            />
                                        </ListItem>
                                    );
                                })}
                            </List>
                        </Paper>
                    </Grid>

                    {/* Right: cumulative spending chart */}
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
                                        <Tooltip title="Cumulative spend – each day includes tot ce ai cheltuit până atunci.">
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
                                        Cumulative by day · Mock data
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
                                        RON {monthlyTotal.toFixed(2)}
                                    </Typography>
                                    <Stack
                                        direction="row"
                                        spacing={0.5}
                                        sx={{ alignItems: 'center' }}
                                    >
                                        <ArrowDownwardRoundedIcon
                                            fontSize="small"
                                            color="success"
                                        />
                                        <Typography
                                            variant="caption"
                                            sx={{
                                                color: theme.palette.success
                                                    .main,
                                            }}
                                        >
                                            -8.2% vs last month
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
                                            data: cumulativeSpending.map(
                                                (d) => d.day,
                                            ),
                                            scaleType: 'point',
                                        },
                                    ]}
                                    series={[
                                        {
                                            data: cumulativeSpending.map(
                                                (d) => d.total,
                                            ),
                                            area: true,
                                            showMark: false,
                                            color: theme.palette.success.main,
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
        </Shell>
    );
}