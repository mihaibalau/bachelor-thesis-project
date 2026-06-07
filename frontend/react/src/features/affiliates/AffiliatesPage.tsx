import {
    Avatar,
    Box,
    Button,
    Chip,
    Dialog,
    DialogActions,
    DialogContent,
    DialogTitle,
    Fade,
    Grid,
    IconButton,
    InputAdornment,
    Menu,
    MenuItem,
    Paper,
    Skeleton,
    Stack,
    TextField,
    Typography,
} from '@mui/material';
import { alpha } from '@mui/material/styles';
import AddRoundedIcon from '@mui/icons-material/AddRounded';
import DeleteOutlineRoundedIcon from '@mui/icons-material/DeleteOutlineRounded';
import EditOutlinedIcon from '@mui/icons-material/EditOutlined';
import MoreVertRoundedIcon from '@mui/icons-material/MoreVertRounded';
import PeopleAltOutlinedIcon from '@mui/icons-material/PeopleAltOutlined';
import SearchRoundedIcon from '@mui/icons-material/SearchRounded';
import SendRoundedIcon from '@mui/icons-material/SendRounded';
import { useMemo, useState, type MouseEvent } from 'react';
import { Link as RouterLink } from 'react-router-dom';
import { PageHeader } from '../../shared/components/PageHeader';
import { ErrorAlert } from '../../shared/components/ErrorAlert';
import { InlineError } from '../../shared/components/InlineError';
import { useAsyncData } from '../../shared/hooks/useAsyncData';
import { useSurfaceStyles } from '../../shared/hooks/useSurfaceStyles';
import { useButtonStyles } from '../../shared/hooks/useButtonStyles';
import { useToast } from '../../shared/context/ToastContext';
import { ApiError } from '../../shared/apiError';
import {
    createAffiliate,
    deleteAffiliate,
    listAffiliates,
    resolveTarget,
    updateAffiliateNickname,
} from './api';
import type { Affiliate, ResolvedTarget } from './types';

const INPUT_HEIGHT = 40;

function initials(name: string): string {
    return name
        .split(/\s+/)
        .filter(Boolean)
        .slice(0, 2)
        .map((p) => p[0]?.toUpperCase() ?? '')
        .join('');
}

export function AffiliatesPage() {
    const { showSuccess, showError } = useToast();
    const { cardBg, cardBorder, cardShadow, cardHoverShadow, isDark, theme } = useSurfaceStyles();
    const { appPrimary, softOutlined, cardGhost, primaryPillMedium } = useButtonStyles();
    const [search, setSearch] = useState('');
    const [addOpen, setAddOpen] = useState(false);
    const [editId, setEditId] = useState<number | null>(null);
    const [editNickname, setEditNickname] = useState('');
    const [menuAnchor, setMenuAnchor] = useState<null | HTMLElement>(null);
    const [menuAffiliate, setMenuAffiliate] = useState<Affiliate | null>(null);

    // Refetch when search changes (server search kicks in at 2+ chars).
    const { data, isLoading, error, reload } = useAsyncData(
        () => listAffiliates({ page: 1, page_size: 50, search: search.length >= 2 ? search : undefined, sort: 'asc' }),
        [search],
    );

    const currencyCounts = useMemo(() => {
        const counts: Record<string, number> = {};
        for (const a of data?.items ?? []) {
            counts[a.currency] = (counts[a.currency] ?? 0) + 1;
        }
        return counts;
    }, [data?.items]);

    const openMenu = (event: MouseEvent<HTMLElement>, affiliate: Affiliate) => {
        setMenuAnchor(event.currentTarget);
        setMenuAffiliate(affiliate);
    };

    const closeMenu = () => {
        setMenuAnchor(null);
        setMenuAffiliate(null);
    };

    const handleDelete = async (affiliate: Affiliate) => {
        closeMenu();
        if (!confirm(`Remove ${affiliate.nickname}?`)) return;
        try {
            await deleteAffiliate(affiliate.recipient_sub_account_id);
            showSuccess('Affiliate removed.');
            void reload();
        } catch (err) {
            showError(err instanceof ApiError ? err.message : 'Delete failed');
        }
    };

    const handleEditSave = async () => {
        if (!editId) return;
        try {
            await updateAffiliateNickname(editId, editNickname);
            showSuccess('Nickname updated.');
            setEditId(null);
            void reload();
        } catch (err) {
            showError(err instanceof ApiError ? err.message : 'Update failed');
        }
    };

    return (
        <>
            <PageHeader
                title="Affiliates"
                subtitle="Saved recipients for quick transfers."
                action={
                    <Button
                        variant="contained"
                        disableElevation
                        startIcon={<AddRoundedIcon />}
                        onClick={() => setAddOpen(true)}
                        sx={primaryPillMedium}
                    >
                        Add affiliate
                    </Button>
                }
            />

            <Paper
                elevation={0}
                sx={{
                    p: 2,
                    mb: 2.5,
                    borderRadius: 3,
                    bgcolor: cardBg,
                    border: cardBorder,
                    boxShadow: cardShadow,
                    display: 'flex',
                    flexWrap: 'wrap',
                    gap: 2,
                    alignItems: 'center',
                    justifyContent: 'space-between',
                }}
            >
                <Stack direction="row" spacing={1.5} sx={{ alignItems: 'center', flexWrap: 'wrap' }}>
                    <Chip
                        icon={<PeopleAltOutlinedIcon />}
                        label={`${data?.total ?? 0} saved`}
                        variant="outlined"
                        sx={{ borderRadius: 999 }}
                    />
                    {Object.entries(currencyCounts).map(([currency, count]) => (
                        <Chip
                            key={currency}
                            label={`${currency} · ${count}`}
                            size="small"
                            sx={{
                                borderRadius: 999,
                                bgcolor: alpha(theme.palette.primary.main, isDark ? 0.12 : 0.08),
                            }}
                        />
                    ))}
                </Stack>

                <TextField
                    size="small"
                    placeholder="Search by name"
                    value={search}
                    onChange={(e) => setSearch(e.target.value)}
                    slotProps={{
                        input: {
                            startAdornment: (
                                <InputAdornment position="start">
                                    <SearchRoundedIcon fontSize="small" color="action" />
                                </InputAdornment>
                            ),
                        },
                    }}
                    sx={{
                        width: { xs: '100%', sm: 320 },
                        '& .MuiOutlinedInput-root': {
                            height: INPUT_HEIGHT,
                            borderRadius: 2,
                            bgcolor: (t) => alpha(t.palette.background.default, t.palette.mode === 'dark' ? 0.45 : 0.35),
                        },
                    }}
                />
            </Paper>

            <ErrorAlert error={error} onRetry={() => void reload()} />

            {isLoading ? (
                <Grid container spacing={2.5}>
                    {[0, 1, 2].map((i) => (
                        <Grid key={i} size={{ xs: 12, sm: 6, md: 4 }}>
                            <Skeleton variant="rounded" height={148} sx={{ borderRadius: 3 }} />
                        </Grid>
                    ))}
                </Grid>
            ) : !data?.items.length ? (
                <Paper
                    elevation={0}
                    sx={{
                        p: 5,
                        textAlign: 'center',
                        borderRadius: 3,
                        border: cardBorder,
                        boxShadow: cardShadow,
                    }}
                >
                    <Avatar
                        sx={{
                            width: 56,
                            height: 56,
                            mx: 'auto',
                            mb: 2,
                            bgcolor: alpha(theme.palette.secondary.main, 0.12),
                            color: 'secondary.main',
                        }}
                    >
                        <PeopleAltOutlinedIcon />
                    </Avatar>
                    <Typography variant="h6" sx={{ mb: 1 }}>
                        {search.length >= 2 ? 'No matches found' : 'No affiliates yet'}
                    </Typography>
                    <Typography color="text.secondary" sx={{ mb: 2.5, maxWidth: 360, mx: 'auto' }}>
                        {search.length >= 2
                            ? 'Try a different nickname or recipient name.'
                            : 'Save people you send money to often — transfers become one tap away.'}
                    </Typography>
                    {search.length < 2 && (
                        <Button
                            variant="contained"
                            disableElevation
                            startIcon={<AddRoundedIcon />}
                            onClick={() => setAddOpen(true)}
                            sx={appPrimary}
                        >
                            Add your first affiliate
                        </Button>
                    )}
                </Paper>
            ) : (
                <Grid container spacing={2.5}>
                    {data.items.map((affiliate, index) => (
                        <Grid key={affiliate.recipient_sub_account_id} size={{ xs: 12, sm: 6, md: 4 }}>
                            <Fade in timeout={320 + index * 80}>
                                <Paper
                                    elevation={0}
                                    sx={{
                                        p: 2.25,
                                        height: '100%',
                                        borderRadius: 3,
                                        bgcolor: cardBg,
                                        border: cardBorder,
                                        boxShadow: cardShadow,
                                        transition: 'transform 160ms ease, box-shadow 160ms ease',
                                        '&:hover': {
                                            transform: 'translateY(-2px)',
                                            boxShadow: cardHoverShadow,
                                        },
                                    }}
                                >
                                    <Stack direction="row" sx={{ justifyContent: 'space-between', mb: 2 }}>
                                        <Stack direction="row" spacing={1.5} sx={{ alignItems: 'center', minWidth: 0 }}>
                                            <Avatar
                                                sx={{
                                                    width: 44,
                                                    height: 44,
                                                    fontWeight: 700,
                                                    bgcolor: alpha(theme.palette.secondary.main, 0.14),
                                                    color: 'secondary.main',
                                                    border: `1px solid ${alpha(theme.palette.secondary.main, 0.3)}`,
                                                }}
                                            >
                                                {initials(affiliate.recipient_full_name)}
                                            </Avatar>
                                            <Box sx={{ minWidth: 0 }}>
                                                <Typography sx={{ fontWeight: 600 }} noWrap>
                                                    {affiliate.nickname}
                                                </Typography>
                                                <Typography variant="body2" color="text.secondary" noWrap>
                                                    {affiliate.recipient_full_name}
                                                </Typography>
                                            </Box>
                                        </Stack>
                                        <IconButton
                                            size="small"
                                            onClick={(e) => openMenu(e, affiliate)}
                                            sx={{ flexShrink: 0 }}
                                        >
                                            <MoreVertRoundedIcon fontSize="small" />
                                        </IconButton>
                                    </Stack>

                                    <Stack direction="row" sx={{ justifyContent: 'space-between', alignItems: 'center' }}>
                                        <Chip
                                            label={affiliate.currency}
                                            size="small"
                                            sx={{
                                                fontWeight: 600,
                                                borderRadius: 999,
                                                bgcolor: alpha(theme.palette.primary.main, isDark ? 0.14 : 0.08),
                                            }}
                                        />
                                        <Button
                                            component={RouterLink}
                                            to={`/app/transactions?tab=Send`}
                                            size="small"
                                            variant="outlined"
                                            color="inherit"
                                            disableElevation
                                            startIcon={<SendRoundedIcon />}
                                            sx={cardGhost}
                                        >
                                            Send
                                        </Button>
                                    </Stack>
                                </Paper>
                            </Fade>
                        </Grid>
                    ))}
                </Grid>
            )}

            <Menu
                anchorEl={menuAnchor}
                open={Boolean(menuAnchor)}
                onClose={closeMenu}
                transformOrigin={{ horizontal: 'right', vertical: 'top' }}
                anchorOrigin={{ horizontal: 'right', vertical: 'bottom' }}
                slotProps={{
                    paper: {
                        elevation: 0,
                        sx: {
                            borderRadius: 2,
                            border: cardBorder,
                            boxShadow: cardHoverShadow,
                            minWidth: 160,
                        },
                    },
                }}
            >
                <MenuItem
                    onClick={() => {
                        if (!menuAffiliate) return;
                        setEditId(menuAffiliate.recipient_sub_account_id);
                        setEditNickname(menuAffiliate.nickname);
                        closeMenu();
                    }}
                >
                    <EditOutlinedIcon fontSize="small" sx={{ mr: 1.5 }} />
                    Edit nickname
                </MenuItem>
                <MenuItem
                    onClick={() => menuAffiliate && void handleDelete(menuAffiliate)}
                    sx={{ color: 'error.main' }}
                >
                    <DeleteOutlineRoundedIcon fontSize="small" sx={{ mr: 1.5 }} />
                    Remove
                </MenuItem>
            </Menu>

            <AddAffiliateDialog open={addOpen} onClose={() => setAddOpen(false)} onCreated={() => void reload()} />

            <Dialog open={editId !== null} onClose={() => setEditId(null)} maxWidth="xs" fullWidth>
                <DialogTitle>Edit nickname</DialogTitle>
                <DialogContent>
                    <TextField
                        fullWidth
                        label="Nickname"
                        value={editNickname}
                        onChange={(e) => setEditNickname(e.target.value)}
                        sx={{ mt: 1 }}
                    />
                </DialogContent>
                <DialogActions sx={{ px: 3, pb: 2, gap: 1 }}>
                    <Button variant="outlined" color="inherit" onClick={() => setEditId(null)} sx={{ minWidth: 96, ...softOutlined }}>
                        Cancel
                    </Button>
                    <Button variant="contained" disableElevation onClick={() => void handleEditSave()} sx={{ minWidth: 96, ...appPrimary }}>
                        Save
                    </Button>
                </DialogActions>
            </Dialog>
        </>
    );
}

function AddAffiliateDialog({ open, onClose, onCreated }: { open: boolean; onClose: () => void; onCreated: () => void }) {
    const { showSuccess, showError } = useToast();
    const { appPrimary, softOutlined } = useButtonStyles();
    const [tag, setTag] = useState('');
    const [nickname, setNickname] = useState('');
    const [resolved, setResolved] = useState<ResolvedTarget | null>(null);
    const [selectedAccountId, setSelectedAccountId] = useState<number | ''>('');
    const [isResolving, setIsResolving] = useState(false);
    const [isSubmitting, setIsSubmitting] = useState(false);
    const [resolveError, setResolveError] = useState<string | null>(null);

    const reset = () => {
        setTag('');
        setNickname('');
        setResolved(null);
        setSelectedAccountId('');
        setResolveError(null);
    };

    const handleResolve = async () => {
        // 1. Look up recipient by tag and pre-select first currency account.
        setIsResolving(true);
        setResolveError(null);
        setResolved(null);
        try {
            const result = await resolveTarget({ identifier_type: 'tag', identifier: tag.trim() });
            setResolved(result);
            if (result.currencies.length > 0) {
                setSelectedAccountId(result.currencies[0].recipient_sub_account_id);
            }
        } catch (err) {
            setResolveError(err instanceof ApiError ? err.message : 'Lookup failed');
        } finally {
            setIsResolving(false);
        }
    };

    const handleCreate = async () => {
        if (!selectedAccountId || !nickname.trim()) return;
        // 2. Save affiliate and refresh parent list.
        setIsSubmitting(true);
        try {
            await createAffiliate({ recipient_sub_account_id: Number(selectedAccountId), nickname: nickname.trim() });
            showSuccess('Affiliate saved.');
            reset();
            onCreated();
            onClose();
        } catch (err) {
            showError(err instanceof ApiError ? err.message : 'Could not save affiliate');
        } finally {
            setIsSubmitting(false);
        }
    };

    const fieldSx = {
        '& .MuiOutlinedInput-root': { height: INPUT_HEIGHT },
    };

    return (
        <Dialog open={open} onClose={() => { reset(); onClose(); }} maxWidth="sm" fullWidth>
            <DialogTitle>Add affiliate</DialogTitle>
            <DialogContent>
                <Stack spacing={2} sx={{ mt: 1 }}>
                    <Typography variant="body2" color="text.secondary">
                        Look up a Gentlix user by their tag, then pick which currency account to save.
                    </Typography>
                    <Stack direction="row" spacing={1} sx={{ alignItems: 'flex-start' }}>
                        <TextField
                            fullWidth
                            label="Recipient tag"
                            value={tag}
                            onChange={(e) => setTag(e.target.value)}
                            onKeyDown={(e) => {
                                if (e.key === 'Enter' && tag.trim()) void handleResolve();
                            }}
                            sx={fieldSx}
                        />
                        <Button
                            variant="outlined"
                            color="inherit"
                            disableElevation
                            onClick={() => void handleResolve()}
                            disabled={!tag.trim() || isResolving}
                            sx={{ height: INPUT_HEIGHT, minWidth: 96, flexShrink: 0, ...softOutlined }}
                        >
                            {isResolving ? '…' : 'Look up'}
                        </Button>
                    </Stack>
                    <InlineError message={resolveError} />
                    {resolved && (
                        <>
                            <Stack direction="row" spacing={1.5} sx={{ alignItems: 'center' }}>
                                <Avatar sx={{ bgcolor: (t) => alpha(t.palette.secondary.main, 0.15) }}>
                                    <PeopleAltOutlinedIcon color="secondary" />
                                </Avatar>
                                <Typography sx={{ fontWeight: 600 }}>{resolved.recipient_full_name}</Typography>
                            </Stack>
                            <TextField
                                select
                                fullWidth
                                label="Currency"
                                value={selectedAccountId}
                                onChange={(e) => setSelectedAccountId(Number(e.target.value))}
                                sx={fieldSx}
                                slotProps={{ select: { native: true } }}
                            >
                                {resolved.currencies.map((c) => (
                                    <option key={c.recipient_sub_account_id} value={c.recipient_sub_account_id}>
                                        {c.currency}
                                    </option>
                                ))}
                            </TextField>
                            <TextField
                                fullWidth
                                label="Nickname"
                                placeholder="e.g. Mihai – RON"
                                value={nickname}
                                onChange={(e) => setNickname(e.target.value)}
                                sx={fieldSx}
                            />
                        </>
                    )}
                </Stack>
            </DialogContent>
            <DialogActions sx={{ px: 3, pb: 2, gap: 1 }}>
                <Button
                    variant="outlined"
                    color="inherit"
                    disableElevation
                    onClick={() => { reset(); onClose(); }}
                    sx={{ minWidth: 96, ...softOutlined }}
                >
                    Cancel
                </Button>
                <Button
                    variant="contained"
                    disableElevation
                    disabled={!resolved || !nickname.trim() || isSubmitting}
                    onClick={() => void handleCreate()}
                    sx={{ minWidth: 120, ...appPrimary }}
                >
                    Save affiliate
                </Button>
            </DialogActions>
        </Dialog>
    );
}
