import { Box, Button, Paper, Skeleton, Stack, Typography } from '@mui/material';
import { useSurfaceStyles } from '../../shared/hooks/useSurfaceStyles';
import { useButtonStyles } from '../../shared/hooks/useButtonStyles';
import { Link as RouterLink, useParams } from 'react-router-dom';
import { useEffect } from 'react';
import { useAsyncData } from '../../shared/hooks/useAsyncData';
import { fetchAccount } from './api';
import { ErrorAlert } from '../../shared/components/ErrorAlert';
import { formatRon } from '../../shared/format';
import { useAccounts } from '../../shared/context/AccountsContext';
import ArrowBackRoundedIcon from '@mui/icons-material/ArrowBackRounded';

export function AccountDetailPage() {
    const { id } = useParams<{ id: string }>();
    const parsedId = Number(id);
    const accountId = Number.isFinite(parsedId) && parsedId > 0 ? parsedId : null;
    const { accounts, reload: reloadAccounts } = useAccounts();
    const { cardBg, cardBorder, cardShadow } = useSurfaceStyles();
    const { back, appPrimary, softOutlined } = useButtonStyles();

    const contextAccount = accountId ? accounts.find((a) => a.id === accountId) : undefined;

    // Fetch fresh account details; fall back to context snapshot while loading.
    const { data: account, isLoading, error, reload } = useAsyncData(
        () => (accountId ? fetchAccount(accountId) : Promise.reject(new Error('Invalid account id'))),
        [accountId],
    );

    // Re-fetch when balance changes in shared context (e.g. after a transaction).
    useEffect(() => {
        if (accountId && contextAccount) void reload();
        // Depends on balance_cents only (not the whole account) to avoid refetch loops.
        // eslint-disable-next-line react-hooks/exhaustive-deps
    }, [accountId, contextAccount?.balance_cents, reload]);

    if (!accountId) {
        return (
            <Box>
                <Button component={RouterLink} to="/app/accounts" startIcon={<ArrowBackRoundedIcon />} variant="text" color="inherit" disableElevation sx={back}>
                    Back to accounts
                </Button>
                <Typography sx={{ mt: 2 }} color="text.secondary">Invalid account id in URL.</Typography>
            </Box>
        );
    }

    const display = account ?? contextAccount;

    return (
        <Box>
            <Button
                component={RouterLink}
                to="/app/accounts"
                startIcon={<ArrowBackRoundedIcon />}
                variant="text"
                color="inherit"
                disableElevation
                sx={back}
            >
                Back to accounts
            </Button>

            <ErrorAlert error={error} onRetry={() => { void reload(); void reloadAccounts(); }} />

            {isLoading && !display ? (
                <Skeleton variant="rounded" height={200} />
            ) : display ? (
                <Paper elevation={0} sx={{ p: 3, borderRadius: 3, bgcolor: cardBg, border: cardBorder, boxShadow: cardShadow }}>
                    <Typography variant="overline" color="text.secondary">Account details</Typography>
                    <Typography variant="h4" sx={{ fontWeight: 600, color: 'primary.main', mt: 1 }}>
                        {formatRon(display.balance_cents)}
                    </Typography>
                    <Stack spacing={1.5} sx={{ mt: 3 }}>
                        <Row label="Type" value={display.account_type} />
                        <Row label="Currency" value={display.currency} />
                        <Row label="IBAN" value={display.iban} />
                    </Stack>
                    <Stack direction="row" spacing={1.5} sx={{ mt: 3 }}>
                        <Button component={RouterLink} to={`/app/transactions?account=${display.id}`} variant="contained" disableElevation sx={appPrimary}>
                            View transactions
                        </Button>
                        <Button component={RouterLink} to={`/app/statements?account=${display.id}`} variant="outlined" disableElevation sx={softOutlined}>
                            View statement
                        </Button>
                    </Stack>
                </Paper>
            ) : null}
        </Box>
    );
}

function Row({ label, value }: { label: string; value: string }) {
    return (
        <Stack direction="row" spacing={2}>
            <Typography variant="body2" color="text.secondary" sx={{ minWidth: 80 }}>{label}</Typography>
            <Typography variant="body2" sx={{ fontWeight: 500 }}>{value}</Typography>
        </Stack>
    );
}
