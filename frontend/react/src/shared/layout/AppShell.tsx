import {
    Box,
    Button,
    Chip,
    Drawer,
    IconButton,
    List,
    ListItemButton,
    ListItemIcon,
    ListItemText,
    Stack,
    Typography,
    useMediaQuery,
} from '@mui/material';
import { alpha, useTheme } from '@mui/material/styles';
import DashboardOutlinedIcon from '@mui/icons-material/DashboardOutlined';
import AccountBalanceWalletOutlinedIcon from '@mui/icons-material/AccountBalanceWalletOutlined';
import PeopleAltOutlinedIcon from '@mui/icons-material/PeopleAltOutlined';
import SwapHorizOutlinedIcon from '@mui/icons-material/SwapHorizOutlined';
import ReceiptLongOutlinedIcon from '@mui/icons-material/ReceiptLongOutlined';
import InsightsOutlinedIcon from '@mui/icons-material/InsightsOutlined';
import LogoutRoundedIcon from '@mui/icons-material/LogoutRounded';
import DarkModeOutlinedIcon from '@mui/icons-material/DarkModeOutlined';
import LightModeOutlinedIcon from '@mui/icons-material/LightModeOutlined';
import MenuIcon from '@mui/icons-material/Menu';
import { useState, type ReactNode } from 'react';
import { NavLink, useLocation, useNavigate } from 'react-router-dom';
import { useAuth } from '../../features/auth/AuthContext';
import { useAccounts } from '../context/AccountsContext';
import { useColorMode } from '../../features/theme/useColorMode';
import gentlixLogo from '../../assets/logo.png';

const NAV = [
    { to: '/app/dashboard', label: 'Dashboard', icon: <DashboardOutlinedIcon /> },
    { to: '/app/accounts', label: 'Accounts', icon: <AccountBalanceWalletOutlinedIcon /> },
    { to: '/app/affiliates', label: 'Affiliates', icon: <PeopleAltOutlinedIcon /> },
    { to: '/app/transactions', label: 'Transactions', icon: <SwapHorizOutlinedIcon /> },
    { to: '/app/statements', label: 'Statements', icon: <ReceiptLongOutlinedIcon /> },
    { to: '/app/statistics', label: 'Statistics', icon: <InsightsOutlinedIcon /> },
];

function NavItems({ onNavigate }: { onNavigate?: () => void }) {
    const location = useLocation();

    return (
        <List sx={{ px: 1 }}>
            {NAV.map((item) => {
                const active = location.pathname === item.to || location.pathname.startsWith(`${item.to}/`);
                return (
                    <ListItemButton
                        key={item.to}
                        component={NavLink}
                        to={item.to}
                        onClick={onNavigate}
                        sx={{
                            borderRadius: 2,
                            mb: 0.5,
                            bgcolor: active ? (t) => alpha(t.palette.primary.main, 0.14) : 'transparent',
                            '&:hover': {
                                bgcolor: (t) => alpha(t.palette.primary.main, 0.08),
                            },
                        }}
                    >
                        <ListItemIcon sx={{ minWidth: 40, color: active ? 'primary.main' : 'text.secondary' }}>
                            {item.icon}
                        </ListItemIcon>
                        <ListItemText
                            primary={item.label}
                            sx={{
                                '& .MuiTypography-root': {
                                    fontWeight: active ? 600 : 500,
                                    color: active ? 'primary.main' : 'text.primary',
                                },
                            }}
                        />
                    </ListItemButton>
                );
            })}
        </List>
    );
}

export function AppShell({ children }: { children: ReactNode }) {
    const { logout } = useAuth();
    const { user } = useAccounts();
    const { mode, toggleMode } = useColorMode();
    const theme = useTheme();
    const isDark = theme.palette.mode === 'dark';
    const isMobile = useMediaQuery(theme.breakpoints.down('md'));
    const navigate = useNavigate();
    const [drawerOpen, setDrawerOpen] = useState(false);

    // Clear auth state and return to public login route.
    const handleLogout = () => {
        logout();
        navigate('/login', { replace: true });
    };

    const headerBg = isDark
        ? `linear-gradient(90deg, ${theme.palette.background.default} 0%, ${alpha(theme.palette.background.paper, 0.96)} 100%)`
        : `linear-gradient(90deg, ${theme.palette.background.paper} 0%, ${alpha(theme.palette.primary.light, 0.18)} 100%)`;

    const sidebar = (
        <Box sx={{ width: 260, py: 2, height: '100%', display: 'flex', flexDirection: 'column' }}>
            <Stack direction="row" spacing={1.5} sx={{ px: 2, mb: 2, alignItems: 'center' }}>
                <Box component="img" src={gentlixLogo} alt="Gentlix Bank" sx={{ width: 32, height: 32, borderRadius: 1.5 }} />
                <Box>
                    <Typography variant="subtitle2" sx={{ fontWeight: 600 }}>Gentlix Bank</Typography>
                    <Typography variant="caption" sx={{ color: 'text.secondary' }}>Client portal</Typography>
                </Box>
            </Stack>
            <NavItems onNavigate={() => setDrawerOpen(false)} />
            <Box sx={{ flex: 1 }} />
            {user && (
                <Typography variant="caption" sx={{ px: 2.5, color: 'text.secondary', mb: 1 }}>
                    {user.first_name} {user.last_name}
                </Typography>
            )}
        </Box>
    );

    return (
        <Box sx={{ minHeight: '100vh', bgcolor: 'background.default', display: 'flex' }}>
            {!isMobile && (
                <Box
                    component="nav"
                    sx={{
                        width: 260,
                        flexShrink: 0,
                        borderRight: '1px solid',
                        borderColor: 'divider',
                        bgcolor: isDark ? alpha(theme.palette.background.paper, 0.5) : theme.palette.background.paper,
                    }}
                >
                    {sidebar}
                </Box>
            )}

            <Box sx={{ flex: 1, display: 'flex', flexDirection: 'column', minWidth: 0 }}>
                <Box
                    component="header"
                    sx={{
                        display: 'flex',
                        alignItems: 'center',
                        justifyContent: 'space-between',
                        px: { xs: 2, md: 3 },
                        py: 1.5,
                        borderBottom: '1px solid',
                        borderColor: 'divider',
                        background: headerBg,
                        position: 'sticky',
                        top: 0,
                        zIndex: 10,
                    }}
                >
                    <Stack direction="row" spacing={1} sx={{ alignItems: 'center' }}>
                        {isMobile && (
                            <IconButton onClick={() => setDrawerOpen(true)}>
                                <MenuIcon />
                            </IconButton>
                        )}
                        <Chip size="small" label="Sandbox" sx={{ borderRadius: 999, fontWeight: 500 }} color="primary" variant="outlined" />
                    </Stack>
                    <Stack direction="row" spacing={1} sx={{ alignItems: 'center' }}>
                        <IconButton onClick={toggleMode} size="small">
                            {mode === 'dark' ? <LightModeOutlinedIcon /> : <DarkModeOutlinedIcon />}
                        </IconButton>
                        <Button size="small" variant="outlined" color="inherit" startIcon={<LogoutRoundedIcon />} onClick={handleLogout}>
                            Sign out
                        </Button>
                    </Stack>
                </Box>

                <Box component="main" sx={{ flex: 1, px: { xs: 2, md: 3 }, py: { xs: 2.5, md: 3 }, maxWidth: 1280, width: '100%', mx: 'auto' }}>
                    {children}
                </Box>
            </Box>

            <Drawer open={drawerOpen} onClose={() => setDrawerOpen(false)}>
                {sidebar}
            </Drawer>
        </Box>
    );
}
