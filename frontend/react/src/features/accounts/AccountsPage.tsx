import {
    Avatar,
    Box,
    Button,
    Grid,
    Paper,
    Skeleton,
    Stack,
    Typography,
} from '@mui/material';
import { alpha } from '@mui/material/styles';
import AccountBalanceWalletOutlinedIcon from '@mui/icons-material/AccountBalanceWalletOutlined';
import AddRoundedIcon from '@mui/icons-material/AddRounded';
import { useState } from 'react';
import { Link as RouterLink } from 'react-router-dom';
import { PageHeader } from '../../shared/components/PageHeader';
import { ErrorAlert } from '../../shared/components/ErrorAlert';
import { useAccounts } from '../../shared/context/AccountsContext';
import { useSurfaceStyles } from '../../shared/hooks/useSurfaceStyles';
import { useButtonStyles } from '../../shared/hooks/useButtonStyles';
import { formatRon } from '../../shared/format';
import { OpenAccountDialog } from './OpenAccountDialog';

const TYPE_COLORS: Record<string, 'primary' | 'secondary' | 'success'> = {
    Regular: 'primary',
    Savings: 'success',
    Credit: 'secondary',
};

export function AccountsPage() {
    const { cardBg, cardBorder, cardShadow, cardHoverShadow, isDark, theme } = useSurfaceStyles();
    const { primaryPillMedium, appPrimary } = useButtonStyles();
    // Accounts list comes from AccountsContext (loaded on app entry).
    const { accounts, isLoading, error, reload } = useAccounts();
    const [dialogOpen, setDialogOpen] = useState(false);

    return (
        <>
            <PageHeader
                title="Accounts"
                subtitle="Manage your Gentlix accounts and balances."
                action={
                    <Button
                        variant="contained"
                        disableElevation
                        startIcon={<AddRoundedIcon />}
                        onClick={() => setDialogOpen(true)}
                        sx={primaryPillMedium}
                    >
                        Open account
                    </Button>
                }
            />

            <ErrorAlert error={error} onRetry={() => void reload()} />

            {isLoading ? (
                <Grid container spacing={2.5}>
                    {[0, 1].map((i) => (
                        <Grid key={i} size={{ xs: 12, md: 6 }}>
                            <Skeleton variant="rounded" height={160} sx={{ borderRadius: 3 }} />
                        </Grid>
                    ))}
                </Grid>
            ) : accounts.length === 0 ? (
                <Paper
                    elevation={0}
                    sx={{ p: 4, textAlign: 'center', borderRadius: 3, border: cardBorder, boxShadow: cardShadow }}
                >
                    <Typography variant="h6" sx={{ mb: 1 }}>No accounts yet</Typography>
                    <Typography color="text.secondary" sx={{ mb: 2 }}>
                        Open your first account to start banking with Gentlix.
                    </Typography>
                    <Button variant="contained" disableElevation onClick={() => setDialogOpen(true)} sx={appPrimary}>
                        Open account
                    </Button>
                </Paper>
            ) : (
                <Grid container spacing={2.5}>
                    {accounts.map((account, index) => {
                        const accent = TYPE_COLORS[account.account_type] ?? 'primary';
                        const paletteMain = theme.palette[accent].main;

                        return (
                            <Grid key={account.id} size={{ xs: 12, md: 6 }}>
                                <Paper
                                    component={RouterLink}
                                    to={`/app/accounts/${account.id}`}
                                    elevation={0}
                                    sx={{
                                        p: 2.5,
                                        borderRadius: 3,
                                        bgcolor: cardBg,
                                        border: cardBorder,
                                        boxShadow: cardShadow,
                                        textDecoration: 'none',
                                        color: 'inherit',
                                        display: 'block',
                                        transition: 'transform 160ms ease, box-shadow 160ms ease',
                                        animation: 'accountCardIn 360ms ease both',
                                        animationDelay: `${index * 60}ms`,
                                        '@keyframes accountCardIn': {
                                            from: { opacity: 0, transform: 'translateY(8px)' },
                                            to: { opacity: 1, transform: 'translateY(0)' },
                                        },
                                        '&:hover': {
                                            transform: 'translateY(-2px)',
                                            boxShadow: cardHoverShadow,
                                        },
                                    }}
                                >
                                    <Stack direction="row" sx={{ justifyContent: 'space-between', mb: 2 }}>
                                        <Box>
                                            <Typography variant="subtitle2" color="text.secondary">
                                                {account.account_type}
                                            </Typography>
                                            <Typography variant="h5" sx={{ fontWeight: 600, color: `${accent}.main` }}>
                                                {formatRon(account.balance_cents)}
                                            </Typography>
                                        </Box>
                                        <Avatar
                                            sx={{
                                                bgcolor: alpha(paletteMain, 0.1),
                                                color: `${accent}.main`,
                                                border: `1px solid ${alpha(paletteMain, 0.35)}`,
                                            }}
                                        >
                                            <AccountBalanceWalletOutlinedIcon />
                                        </Avatar>
                                    </Stack>
                                    <Stack direction="row" spacing={1} sx={{ alignItems: 'center' }}>
                                        <Typography
                                            component="span"
                                            variant="caption"
                                            sx={{
                                                px: 1,
                                                py: 0.25,
                                                borderRadius: 999,
                                                fontWeight: 600,
                                                bgcolor: alpha(paletteMain, isDark ? 0.14 : 0.08),
                                                color: `${accent}.main`,
                                            }}
                                        >
                                            {account.currency}
                                        </Typography>
                                        <Typography variant="body2" color="text.secondary" sx={{ fontFamily: 'monospace', fontSize: '0.8rem' }}>
                                            {account.iban}
                                        </Typography>
                                    </Stack>
                                </Paper>
                            </Grid>
                        );
                    })}
                </Grid>
            )}

            <OpenAccountDialog
                open={dialogOpen}
                onClose={() => setDialogOpen(false)}
                onCreated={() => void reload()}
            />
        </>
    );
}
