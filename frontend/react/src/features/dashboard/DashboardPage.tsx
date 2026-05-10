// src/features/dashboard/DashboardPage.tsx
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
} from '@mui/material';
import LogoutRoundedIcon from '@mui/icons-material/LogoutRounded';
import AccountBalanceWalletOutlinedIcon from '@mui/icons-material/AccountBalanceWalletOutlined';
import PeopleAltOutlinedIcon from '@mui/icons-material/PeopleAltOutlined';
import TrendingUpOutlinedIcon from '@mui/icons-material/TrendingUpOutlined';
import NotificationsNoneOutlinedIcon from '@mui/icons-material/NotificationsNoneOutlined';
import type { ReactNode } from 'react';
import { useAuth } from '../auth/AuthContext';
import gentlixLogo from '../../assets/logo.png';

function Shell({ children }: { children: ReactNode }) {
    const { logout } = useAuth();

    return (
        <Box
            sx={{
                minHeight: '100vh',
                bgcolor: 'background.default',
                color: 'text.primary',
                display: 'flex',
                flexDirection: 'column',
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
                    background:
                        'linear-gradient(to right, rgba(15,23,42,0.92), rgba(15,23,42,0.86))',
                    position: 'sticky',
                    top: 0,
                    zIndex: 10,
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
                            sx={{ color: 'text.secondary', textTransform: 'uppercase' }}
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
                            bgcolor: 'rgba(56,189,248,0.16)',
                            color: 'secondary.main',
                            border: '1px solid rgba(56,189,248,0.4)',
                        }}
                    />
                </Stack>

                <Stack
                    direction="row"
                    spacing={1.5}
                    sx={{ alignItems: 'center' }}
                >
                    <IconButton color="inherit">
                        <NotificationsNoneOutlinedIcon />
                    </IconButton>

                    <Button
                        variant="outlined"
                        color="inherit"
                        size="small"
                        startIcon={<LogoutRoundedIcon />}
                        onClick={logout}
                        sx={{
                            borderRadius: 999,
                            borderColor: 'rgba(148,163,184,0.6)',
                            '&:hover': {
                                borderColor: '#f5c451',
                                backgroundColor: 'rgba(15,23,42,0.9)',
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
                }}
            >
                {children}
            </Box>
        </Box>
    );
}

export function DashboardPage() {
    const { userId } = useAuth();

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
                        background:
                            'radial-gradient(circle at 0% 0%, rgba(245,196,81,0.18), transparent 60%), radial-gradient(circle at 120% 0%, rgba(56,189,248,0.2), transparent 60%), linear-gradient(135deg, #020617, #020617)',
                    }}
                >
                    <Box>
                        <Typography variant="h4" sx={{ mb: 1 }}>
                            Welcome back
                        </Typography>
                        <Typography variant="body2" sx={{ color: 'text.secondary' }}>
                            You are signed in with user ID <strong>{userId}</strong>.
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
                            sx={{ color: 'text.secondary', textTransform: 'uppercase' }}
                        >
                            Total balance
                        </Typography>
                        <Typography
                            variant="h5"
                            sx={{ fontWeight: 600, color: 'primary.main' }}
                        >
                            RON 12,480.23
                        </Typography>
                        <Typography
                            variant="body2"
                            sx={{ color: 'success.main', display: 'flex', gap: 0.5 }}
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
                                <Typography variant="subtitle2" sx={{ color: 'text.secondary' }}>
                                    Accounts
                                </Typography>
                                <Avatar
                                    sx={{
                                        width: 30,
                                        height: 30,
                                        bgcolor: 'rgba(245,196,81,0.08)',
                                        color: 'primary.main',
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
                                <Typography variant="subtitle2" sx={{ color: 'text.secondary' }}>
                                    Affiliates
                                </Typography>
                                <Avatar
                                    sx={{
                                        width: 30,
                                        height: 30,
                                        bgcolor: 'rgba(56,189,248,0.1)',
                                        color: 'secondary.main',
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
                                <Typography variant="subtitle2" sx={{ color: 'text.secondary' }}>
                                    Transfers
                                </Typography>
                                <Avatar
                                    sx={{
                                        width: 30,
                                        height: 30,
                                        bgcolor: 'rgba(34,197,94,0.12)',
                                        color: 'success.main',
                                    }}
                                >
                                    <TrendingUpOutlinedIcon fontSize="small" />
                                </Avatar>
                            </Stack>

                            <Typography variant="h5">RON 4,200</Typography>
                        </Paper>
                    </Grid>
                </Grid>

            </Stack>
        </Shell>
    );
}