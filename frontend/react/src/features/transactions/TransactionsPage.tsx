import {
    Box,
    Button,
    MenuItem,
    Paper,
    Stack,
    Tab,
    Tabs,
    TextField,
    ToggleButton,
    ToggleButtonGroup,
    Typography,
} from '@mui/material';
import { alpha, useTheme } from '@mui/material/styles';
import LocalAtmOutlinedIcon from '@mui/icons-material/LocalAtmOutlined';
import PaymentsOutlinedIcon from '@mui/icons-material/PaymentsOutlined';
import SendRoundedIcon from '@mui/icons-material/SendRounded';
import SwapHorizOutlinedIcon from '@mui/icons-material/SwapHorizOutlined';
import { useEffect, useMemo, useState } from 'react';
import { useSearchParams } from 'react-router-dom';
import { PageHeader } from '../../shared/components/PageHeader';
import { AccountSelect } from '../../shared/components/AccountSelect';
import { AccountPicker } from '../../shared/components/AccountPicker';
import { useAccounts } from '../../shared/context/AccountsContext';
import { useToast } from '../../shared/context/ToastContext';
import { useAsyncData } from '../../shared/hooks/useAsyncData';
import { useSurfaceStyles } from '../../shared/hooks/useSurfaceStyles';
import { useButtonStyles } from '../../shared/hooks/useButtonStyles';
import { ApiError } from '../../shared/apiError';
import { FormInfoNote } from '../../shared/components/FormInfoNote';
import { parseWholeUnits, unitsToCents } from '../../shared/format';
import { requestDashboardRefresh } from '../../shared/refreshEvents';
import { RecentTransactionsPanel } from './RecentTransactionsPanel';
import {
    recordDeposit,
    recordPayment,
    recordSend,
    recordTransfer,
    recordWithdrawal,
} from './api';
import { listAffiliates } from '../affiliates/api';

const TABS = ['Recent', 'ATM', 'Transfer', 'Send', 'Payment'] as const;
type TabKey = (typeof TABS)[number];
type AtmMode = 'deposit' | 'withdraw';

const PAYMENT_CATEGORIES = ['Food', 'Shopping', 'Transport', 'Bills', 'Entertainment', 'Other'];

// Map legacy ?tab=Deposit|Withdraw and ?atm= query params to tab state.
function resolveTabFromParams(tabParam: string | null, atmParam: string | null): { tab: TabKey; atmMode: AtmMode } {
    if (tabParam === 'Deposit') return { tab: 'ATM', atmMode: 'deposit' };
    if (tabParam === 'Withdraw') return { tab: 'ATM', atmMode: 'withdraw' };
    const tab = TABS.includes(tabParam as TabKey) ? (tabParam as TabKey) : 'Recent';
    const atmMode: AtmMode = atmParam === 'withdraw' ? 'withdraw' : 'deposit';
    return { tab, atmMode };
}

export function TransactionsPage() {
    const theme = useTheme();
    const isDark = theme.palette.mode === 'dark';
    const { cardBg, cardBorder, cardShadow } = useSurfaceStyles();
    const { primaryPillAction, filterPill, togglePillGroup } = useButtonStyles();
    const [params, setParams] = useSearchParams();
    const initial = resolveTabFromParams(params.get('tab'), params.get('atm'));
    const [tab, setTab] = useState<TabKey>(initial.tab);
    const [atmMode, setAtmMode] = useState<AtmMode>(initial.atmMode);
    const { accounts, reload: reloadAccounts } = useAccounts();
    const { showSuccess, showError } = useToast();

    const initialAccount = Number(params.get('account')) || accounts[0]?.id || '';
    const [accountId, setAccountId] = useState<number | ''>(initialAccount);

    useEffect(() => {
        // Default to the first account once the list loads (one-time selection).
        // eslint-disable-next-line react-hooks/set-state-in-effect
        if (accounts.length && accountId === '') setAccountId(accounts[0].id);
    }, [accounts, accountId]);

    const [recentRefresh, setRecentRefresh] = useState(0);

    const selectedAccount = accounts.find((a) => a.id === accountId);
    // Load send-eligible affiliates when source account currency changes.
    const { data: affiliates, isLoading: affiliatesLoading } = useAsyncData(
        () =>
            selectedAccount?.currency
                ? listAffiliates({ page_size: 100, for_send_currency: selectedAccount.currency })
                : Promise.resolve({ items: [], page: 1, page_size: 100, total: 0 }),
        [selectedAccount?.currency, accountId],
    );

    const sendRecipients = useMemo(() => {
        const currency = selectedAccount?.currency?.toUpperCase();
        if (!currency) return [];
        return (affiliates?.items ?? []).filter(
            (a) => a.currency.toUpperCase() === currency,
        );
    }, [affiliates?.items, selectedAccount?.currency]);

    const [amount, setAmount] = useState('');
    const [toAccountId, setToAccountId] = useState<number | ''>('');
    const [recipientId, setRecipientId] = useState<number | ''>('');
    const [message, setMessage] = useState('');
    const [category, setCategory] = useState('Food');
    const [merchant, setMerchant] = useState('');
    const [note, setNote] = useState('');
    const [submitting, setSubmitting] = useState(false);

    useEffect(() => {
        if (!recipientId) return;
        const stillValid = sendRecipients.some((a) => a.recipient_sub_account_id === recipientId);
        // Clear the recipient when it is no longer a valid send target.
        // eslint-disable-next-line react-hooks/set-state-in-effect
        if (!stillValid) setRecipientId('');
    }, [sendRecipients, recipientId]);

    const updateParams = (nextTab: TabKey, nextAtm: AtmMode, nextAccount?: number | '') => {
        const p: Record<string, string> = { tab: nextTab };
        if (nextTab === 'ATM') p.atm = nextAtm;
        const acc = nextAccount ?? accountId;
        if (acc) p.account = String(acc);
        setParams(p);
    };

    const handleTab = (_: unknown, v: TabKey) => {
        setTab(v);
        updateParams(v, atmMode);
    };

    const handleAtmMode = (_: unknown, v: AtmMode | null) => {
        if (!v) return;
        setAtmMode(v);
        updateParams('ATM', v);
    };

    const handleAccountChange = (id: number) => {
        setAccountId(id);
        setToAccountId('');
        setRecipientId('');
        updateParams(tab, atmMode, id);
    };

    // Reload accounts, recent list, and dashboard after a successful mutation.
    const refreshAll = async () => {
        await reloadAccounts();
        setRecentRefresh((n) => n + 1);
        requestDashboardRefresh();
    };

    const submit = async (action: () => Promise<number>) => {
        // 1. Call the tab-specific record API.
        setSubmitting(true);
        try {
            const id = await action();
            showSuccess(`Transaction recorded (#${id}).`);
            // 2. Reset form fields and refresh dependent views.
            setAmount('');
            setMessage('');
            setMerchant('');
            setNote('');
            await refreshAll();
            // 3. Switch to Recent tab to show the new entry.
            setTab('Recent');
            setParams(accountId ? { tab: 'Recent', account: String(accountId) } : { tab: 'Recent' });
        } catch (err) {
            const message = err instanceof ApiError
                ? err.message
                : err instanceof Error
                    ? err.message
                    : 'Transaction failed';
            showError(message);
        } finally {
            setSubmitting(false);
        }
    };

    const sameCurrencyAccounts = accounts.filter(
        (a) => !accountId || a.currency === selectedAccount?.currency,
    );

    const panelSx = {
        p: { xs: 2.5, md: 3 },
        borderRadius: 3,
        bgcolor: cardBg,
        border: cardBorder,
        boxShadow: cardShadow,
        width: '100%',
    };

    const tabBarSx = {
        minHeight: 48,
        px: 1,
        '& .MuiTabs-indicator': { display: 'none' },
        '& .MuiTab-root': {
            textTransform: 'none',
            fontWeight: 600,
            minHeight: 48,
            minWidth: 'auto',
            px: 2.5,
            py: 1.5,
            color: 'text.secondary',
            borderBottom: '2px solid transparent',
            transition: 'color 0.15s ease, border-color 0.15s ease',
            '&.Mui-selected': {
                color: 'primary.main',
                borderBottomColor: 'primary.main',
            },
        },
    };

    const formIconBox = (color: string, bgAlpha: number) => ({
        width: 44,
        height: 44,
        borderRadius: 2,
        display: 'grid',
        placeItems: 'center',
        bgcolor: alpha(color, isDark ? bgAlpha : bgAlpha * 0.75),
        color,
    });

    return (
        <>
            <PageHeader
                title="Transactions"
                subtitle="View history and record deposits, withdrawals, transfers, and payments."
                action={
                    <AccountPicker
                        accounts={accounts}
                        value={accountId}
                        onChange={handleAccountChange}
                    />
                }
            />

            <Paper elevation={0} sx={{ borderRadius: 3, mb: 2.5, border: cardBorder, boxShadow: cardShadow, overflow: 'hidden' }}>
                <Tabs
                    value={tab}
                    onChange={handleTab}
                    variant="scrollable"
                    scrollButtons="auto"
                    sx={tabBarSx}
                >
                    {TABS.map((t) => <Tab key={t} label={t} value={t} />)}
                </Tabs>
            </Paper>

            {tab === 'Recent' && (
                <RecentTransactionsPanel
                    accountId={accountId}
                    refreshToken={recentRefresh}
                    panelSx={panelSx}
                />
            )}

            {tab === 'ATM' && (
                <Paper elevation={0} sx={panelSx}>
                    <Stack spacing={2.5}>
                        <Stack direction="row" spacing={1.5} sx={{ alignItems: 'center' }}>
                            <Box sx={formIconBox(theme.palette.primary.main, 0.16)}>
                                <LocalAtmOutlinedIcon />
                            </Box>
                            <Box>
                                <Typography sx={{ fontWeight: 600 }}>ATM</Typography>
                                <Typography variant="body2" color="text.secondary">
                                    {atmMode === 'deposit'
                                        ? `Deposit cash into your ${selectedAccount ? `${selectedAccount.account_type} ${selectedAccount.currency}` : 'account'}.`
                                        : `Withdraw cash from your ${selectedAccount ? `${selectedAccount.account_type} ${selectedAccount.currency}` : 'account'}.`}
                                </Typography>
                            </Box>
                        </Stack>

                        <ToggleButtonGroup
                            value={atmMode}
                            exclusive
                            onChange={handleAtmMode}
                            sx={togglePillGroup}
                        >
                            <ToggleButton value="deposit" sx={filterPill(atmMode === 'deposit')}>
                                Deposit
                            </ToggleButton>
                            <ToggleButton value="withdraw" sx={filterPill(atmMode === 'withdraw')}>
                                Withdraw
                            </ToggleButton>
                        </ToggleButtonGroup>

                        <TextField
                            label="Amount"
                            type="number"
                            placeholder={atmMode === 'deposit' ? 'e.g. 500' : 'e.g. 200'}
                            value={amount}
                            onChange={(e) => setAmount(e.target.value)}
                            slotProps={{ htmlInput: { min: 1 } }}
                            fullWidth
                        />

                        <FormInfoNote>
                            Simulates a bank ATM — deposit or withdraw whole {selectedAccount?.currency ?? 'currency'} units instantly.
                            {atmMode === 'withdraw' ? ' Amount must not exceed your available balance.' : ''}
                        </FormInfoNote>

                        <Box>
                            <Button
                                variant="contained"
                                disableElevation
                                disabled={submitting || !accountId || !amount}
                                onClick={() =>
                                    void submit(() =>
                                        atmMode === 'deposit'
                                            ? recordDeposit(Number(accountId), parseWholeUnits(amount))
                                            : recordWithdrawal(Number(accountId), parseWholeUnits(amount)),
                                    )
                                }
                                sx={{ ...primaryPillAction, minWidth: 200 }}
                            >
                                {atmMode === 'deposit' ? 'Confirm deposit' : 'Confirm withdrawal'}
                            </Button>
                        </Box>
                    </Stack>
                </Paper>
            )}

            {tab === 'Transfer' && (
                <Paper elevation={0} sx={panelSx}>
                    <Stack spacing={2.5}>
                        <Stack direction="row" spacing={1.5} sx={{ alignItems: 'center' }}>
                            <Box sx={formIconBox(theme.palette.secondary.main, 0.16)}>
                                <SwapHorizOutlinedIcon />
                            </Box>
                            <Box>
                                <Typography sx={{ fontWeight: 600 }}>Internal transfer</Typography>
                                <Typography variant="body2" color="text.secondary">
                                    Move money between your own {selectedAccount?.currency ?? ''} accounts.
                                </Typography>
                            </Box>
                        </Stack>
                        <TextField
                            label="Amount"
                            type="number"
                            placeholder="e.g. 150"
                            value={amount}
                            onChange={(e) => setAmount(e.target.value)}
                            slotProps={{ htmlInput: { min: 1 } }}
                            fullWidth
                        />
                        <AccountSelect
                            accounts={sameCurrencyAccounts.filter((a) => a.id !== accountId)}
                            value={toAccountId}
                            onChange={setToAccountId}
                            label="To account"
                            format="type-only"
                        />
                        <Box>
                            <Button
                                variant="contained"
                                disableElevation
                                disabled={submitting || !accountId || !amount || !toAccountId}
                                onClick={() =>
                                    void submit(() =>
                                        recordTransfer(
                                            Number(accountId),
                                            Number(toAccountId),
                                            unitsToCents(parseWholeUnits(amount)),
                                        ),
                                    )
                                }
                                sx={{ ...primaryPillAction, minWidth: 200 }}
                            >
                                Confirm transfer
                            </Button>
                        </Box>
                    </Stack>
                </Paper>
            )}

            {tab === 'Send' && (
                <Paper elevation={0} sx={panelSx}>
                    <Stack spacing={2.5}>
                        <Stack direction="row" spacing={1.5} sx={{ alignItems: 'center' }}>
                            <Box sx={formIconBox(theme.palette.primary.main, 0.16)}>
                                <SendRoundedIcon />
                            </Box>
                            <Box>
                                <Typography sx={{ fontWeight: 600 }}>Send to affiliate</Typography>
                                <Typography variant="body2" color="text.secondary">
                                    Pay a saved recipient from your {selectedAccount ? `${selectedAccount.account_type} ${selectedAccount.currency}` : 'account'}.
                                </Typography>
                            </Box>
                        </Stack>

                        <TextField
                            label="Amount"
                            type="number"
                            placeholder="e.g. 250"
                            value={amount}
                            onChange={(e) => setAmount(e.target.value)}
                            slotProps={{ htmlInput: { min: 1 } }}
                            fullWidth
                        />
                        <TextField
                            select
                            label="Recipient"
                            value={sendRecipients.some((a) => a.recipient_sub_account_id === recipientId) ? recipientId : ''}
                            onChange={(e) => setRecipientId(Number(e.target.value))}
                            fullWidth
                            disabled={affiliatesLoading}
                            helperText={
                                affiliatesLoading
                                    ? 'Loading affiliates…'
                                    : !sendRecipients.length
                                        ? `No affiliates with a Regular ${selectedAccount?.currency ?? ''} account.`
                                        : undefined
                            }
                        >
                            <MenuItem value="" disabled>
                                {affiliatesLoading ? 'Loading…' : 'Select an affiliate…'}
                            </MenuItem>
                            {sendRecipients.map((a) => (
                                <MenuItem key={a.recipient_sub_account_id} value={a.recipient_sub_account_id}>
                                    {a.nickname}
                                </MenuItem>
                            ))}
                        </TextField>
                        <TextField
                            label="Message (optional)"
                            placeholder="What's this for?"
                            value={message}
                            onChange={(e) => setMessage(e.target.value)}
                            fullWidth
                        />

                        <FormInfoNote>
                            You can send money to affiliates who hold a Regular {selectedAccount?.currency ?? ''} account.
                        </FormInfoNote>

                        <Box>
                            <Button
                                variant="contained"
                                disableElevation
                                disabled={submitting || affiliatesLoading || !accountId || !amount || !recipientId || !sendRecipients.length}
                                onClick={() =>
                                    void submit(() =>
                                        recordSend(
                                            Number(accountId),
                                            Number(recipientId),
                                            unitsToCents(parseWholeUnits(amount)),
                                            message || 'Transfer',
                                        ),
                                    )
                                }
                                sx={{ ...primaryPillAction, minWidth: 200 }}
                            >
                                Confirm send
                            </Button>
                        </Box>
                    </Stack>
                </Paper>
            )}

            {tab === 'Payment' && (
                <Paper elevation={0} sx={panelSx}>
                    <Stack spacing={2.5}>
                        <Stack direction="row" spacing={1.5} sx={{ alignItems: 'center' }}>
                            <Box sx={formIconBox('#fb923c', 0.18)}>
                                <PaymentsOutlinedIcon />
                            </Box>
                            <Box>
                                <Typography sx={{ fontWeight: 600 }}>Card payment</Typography>
                                <Typography variant="body2" color="text.secondary">
                                    Record a purchase from your {selectedAccount ? `${selectedAccount.account_type} ${selectedAccount.currency}` : 'account'}.
                                </Typography>
                            </Box>
                        </Stack>
                        <TextField
                            label="Amount"
                            type="number"
                            placeholder="e.g. 90"
                            value={amount}
                            onChange={(e) => setAmount(e.target.value)}
                            slotProps={{ htmlInput: { min: 1 } }}
                            fullWidth
                        />

                        <Box>
                            <Typography variant="body2" color="text.secondary" sx={{ mb: 1 }}>
                                Category
                            </Typography>
                            <ToggleButtonGroup
                                value={category}
                                exclusive
                                onChange={(_, v) => v && setCategory(v)}
                                sx={togglePillGroup}
                            >
                                {PAYMENT_CATEGORIES.map((c) => (
                                    <ToggleButton key={c} value={c} sx={filterPill(category === c)}>
                                        {c}
                                    </ToggleButton>
                                ))}
                            </ToggleButtonGroup>
                        </Box>

                        <TextField
                            label="Merchant"
                            placeholder="e.g. Glovo, Mega Image"
                            value={merchant}
                            onChange={(e) => setMerchant(e.target.value)}
                            fullWidth
                        />
                        <TextField
                            label="Note"
                            placeholder="Dinner with friends"
                            value={note}
                            onChange={(e) => setNote(e.target.value)}
                            fullWidth
                        />
                        <Box>
                            <Button
                                variant="contained"
                                disableElevation
                                disabled={submitting || !accountId || !amount || !merchant.trim()}
                                onClick={() =>
                                    void submit(() =>
                                        recordPayment(Number(accountId), parseWholeUnits(amount), category, merchant.trim(), note || undefined),
                                    )
                                }
                                sx={{ ...primaryPillAction, minWidth: 200 }}
                            >
                                Confirm payment
                            </Button>
                        </Box>
                    </Stack>
                </Paper>
            )}
        </>
    );
}
