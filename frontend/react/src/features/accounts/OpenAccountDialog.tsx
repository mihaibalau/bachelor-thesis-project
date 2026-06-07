import {
    Button,
    Dialog,
    DialogActions,
    DialogContent,
    DialogTitle,
    FormControl,
    InputLabel,
    MenuItem,
    Select,
    Stack,
    Typography,
} from '@mui/material';
import { useEffect, useMemo, useState } from 'react';
import { fetchAccountAvailability, openAccount } from './api';
import type { AccountTypeAvailability } from './types';
import { ErrorAlert } from '../../shared/components/ErrorAlert';
import { useToast } from '../../shared/context/ToastContext';
import { ApiError } from '../../shared/apiError';
import { useButtonStyles } from '../../shared/hooks/useButtonStyles';

type Props = {
    open: boolean;
    onClose: () => void;
    onCreated: () => void;
};

export function OpenAccountDialog({ open, onClose, onCreated }: Props) {
    const { showSuccess, showError } = useToast();
    const { appPrimary, softOutlined } = useButtonStyles();
    const [availability, setAvailability] = useState<AccountTypeAvailability[]>([]);
    const [isLoading, setIsLoading] = useState(true);
    const [error, setError] = useState<ApiError | null>(null);
    const [accountType, setAccountType] = useState('');
    const [currency, setCurrency] = useState('');
    const [isSubmitting, setIsSubmitting] = useState(false);

    const openableTypes = useMemo(
        () => availability.filter((t) => t.currencies.some((c) => c.available)),
        [availability],
    );

    const selectedType = openableTypes.find((t) => t.account_type === accountType);
    const availableCurrencies = selectedType?.currencies.filter((c) => c.available) ?? [];

    // 1. On open, load which type/currency combos the user can still create.
    useEffect(() => {
        if (!open) return;
        // Reset to loading state before the availability fetch resolves.
        // eslint-disable-next-line react-hooks/set-state-in-effect
        setIsLoading(true);
        setError(null);
        fetchAccountAvailability()
            .then((res) => {
                setAvailability(res.types);
                const first = res.types.find((t) => t.currencies.some((c) => c.available));
                if (first) {
                    setAccountType(first.account_type);
                    const cur = first.currencies.find((c) => c.available);
                    setCurrency(cur?.currency ?? '');
                }
            })
            .catch((err) => {
                setError(err instanceof ApiError ? err : new ApiError(500, 'unknown_error', 'Failed to load availability'));
            })
            .finally(() => setIsLoading(false));
    }, [open]);

    const handleSubmit = async () => {
        if (!accountType || !currency) return;
        // 2. Post open request, notify parent to reload accounts list.
        setIsSubmitting(true);
        try {
            await openAccount({ account_type: accountType, currency, initial_balance_cents: 0 });
            showSuccess(`${accountType} ${currency} account opened successfully.`);
            onCreated();
            onClose();
        } catch (err) {
            const msg = err instanceof ApiError ? err.message : 'Could not open account';
            showError(msg);
        } finally {
            setIsSubmitting(false);
        }
    };

    return (
        <Dialog open={open} onClose={onClose} maxWidth="sm" fullWidth>
            <DialogTitle>Open a new account</DialogTitle>
            <DialogContent>
                <Stack spacing={2.5} sx={{ mt: 1 }}>
                    <Typography variant="body2" color="text.secondary">
                        Pick an account type and currency you have not opened yet. You can hold one account per type and currency.
                    </Typography>
                    <ErrorAlert error={error} />
                    {isLoading ? (
                        <Typography color="text.secondary">Loading available account types…</Typography>
                    ) : openableTypes.length === 0 ? (
                        <Typography color="text.secondary">
                            You already have every account type and currency combination. No more accounts can be opened.
                        </Typography>
                    ) : (
                        <>
                            <FormControl fullWidth>
                                <InputLabel>Account type</InputLabel>
                                <Select
                                    label="Account type"
                                    value={accountType}
                                    onChange={(e) => {
                                        setAccountType(e.target.value);
                                        const t = openableTypes.find((x) => x.account_type === e.target.value);
                                        const cur = t?.currencies.find((c) => c.available);
                                        setCurrency(cur?.currency ?? '');
                                    }}
                                >
                                    {openableTypes.map((t) => {
                                        const openCurrencies = t.currencies
                                            .filter((c) => c.available)
                                            .map((c) => c.currency)
                                            .join(', ');
                                        return (
                                            <MenuItem key={t.account_type} value={t.account_type}>
                                                {t.account_type}
                                                {openCurrencies ? ` · ${openCurrencies}` : ''}
                                            </MenuItem>
                                        );
                                    })}
                                </Select>
                            </FormControl>
                            <FormControl fullWidth>
                                <InputLabel>Currency</InputLabel>
                                <Select
                                    label="Currency"
                                    value={currency}
                                    onChange={(e) => setCurrency(e.target.value)}
                                >
                                    {availableCurrencies.map((c) => (
                                        <MenuItem key={c.currency} value={c.currency}>
                                            {c.currency}
                                        </MenuItem>
                                    ))}
                                </Select>
                            </FormControl>
                        </>
                    )}
                </Stack>
            </DialogContent>
            <DialogActions sx={{ px: 3, pb: 2 }}>
                <Button variant="outlined" color="inherit" disableElevation onClick={onClose} sx={softOutlined}>
                    Cancel
                </Button>
                <Button
                    variant="contained"
                    disableElevation
                    onClick={handleSubmit}
                    disabled={isLoading || isSubmitting || openableTypes.length === 0 || !currency}
                    sx={appPrimary}
                >
                    Open account
                </Button>
            </DialogActions>
        </Dialog>
    );
}
