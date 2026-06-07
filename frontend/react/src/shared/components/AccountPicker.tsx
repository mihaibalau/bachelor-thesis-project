import { useState } from 'react';
import {
    Box,
    Button,
    Menu,
    MenuItem,
    Typography,
} from '@mui/material';
import { alpha, useTheme } from '@mui/material/styles';
import KeyboardArrowDownRoundedIcon from '@mui/icons-material/KeyboardArrowDownRounded';
import AccountBalanceWalletOutlinedIcon from '@mui/icons-material/AccountBalanceWalletOutlined';
import type { Account } from '../../features/users/types';
import { formatAccountLabel } from '../format';

type Props = {
    accounts: Account[];
    value: number | '';
    onChange: (accountId: number) => void;
    disabled?: boolean;
};

export function AccountPicker({ accounts, value, onChange, disabled }: Props) {
    const theme = useTheme();
    const isDark = theme.palette.mode === 'dark';
    const [anchor, setAnchor] = useState<null | HTMLElement>(null);

    const selected = accounts.find((a) => a.id === value);

    return (
        <>
            <Button
                variant="outlined"
                color="inherit"
                disableElevation
                disabled={disabled || accounts.length === 0}
                onClick={(e) => setAnchor(e.currentTarget)}
                endIcon={<KeyboardArrowDownRoundedIcon />}
                sx={{
                    textTransform: 'none',
                    fontWeight: 600,
                    borderRadius: 999,
                    px: 2,
                    py: 0.75,
                    boxShadow: 'none',
                    borderColor: alpha(theme.palette.primary.main, 0.45),
                    bgcolor: isDark
                        ? alpha(theme.palette.background.paper, 0.55)
                        : alpha(theme.palette.primary.main, 0.06),
                    '&:hover': {
                        boxShadow: 'none',
                        borderColor: alpha(theme.palette.primary.main, 0.7),
                        bgcolor: alpha(theme.palette.primary.main, isDark ? 0.12 : 0.1),
                    },
                }}
            >
                <Box sx={{ display: 'flex', alignItems: 'center', gap: 1 }}>
                    <AccountBalanceWalletOutlinedIcon sx={{ fontSize: 18, color: 'primary.main' }} />
                    <Typography variant="body2" sx={{ fontWeight: 600 }}>
                        {selected ? formatAccountLabel(selected) : 'Select account'}
                    </Typography>
                </Box>
            </Button>
            <Menu
                anchorEl={anchor}
                open={Boolean(anchor)}
                onClose={() => setAnchor(null)}
                slotProps={{
                    paper: {
                        elevation: 0,
                        sx: {
                            mt: 0.75,
                            borderRadius: 2,
                            border: `1px solid ${alpha(theme.palette.divider, 0.9)}`,
                            boxShadow: isDark
                                ? '0 18px 40px rgba(0,0,0,0.7)'
                                : '0 4px 14px rgba(15,23,42,0.08)',
                            minWidth: 200,
                        },
                    },
                }}
            >
                {accounts.map((a) => (
                    <MenuItem
                        key={a.id}
                        selected={a.id === value}
                        onClick={() => {
                            onChange(a.id);
                            setAnchor(null);
                        }}
                        sx={{ fontWeight: a.id === value ? 600 : 500 }}
                    >
                        {formatAccountLabel(a)}
                    </MenuItem>
                ))}
            </Menu>
        </>
    );
}
