import { alpha, type Theme } from '@mui/material/styles';
import type { SxProps } from '@mui/material';
import ShoppingCartOutlinedIcon from '@mui/icons-material/ShoppingCartOutlined';
import RestaurantOutlinedIcon from '@mui/icons-material/RestaurantOutlined';
import LocalAtmOutlinedIcon from '@mui/icons-material/LocalAtmOutlined';
import SwapHorizOutlinedIcon from '@mui/icons-material/SwapHorizOutlined';
import ArrowUpwardRoundedIcon from '@mui/icons-material/ArrowUpwardRounded';
import PaymentsOutlinedIcon from '@mui/icons-material/PaymentsOutlined';
import type { TransactionCategory } from './transactionDisplay';

export function categoryIconStyle(category: TransactionCategory, isDark: boolean): SxProps<Theme> {
    switch (category) {
        case 'food':
            return {
                bgcolor: alpha('#fb923c', isDark ? 0.22 : 0.18),
                color: isDark ? '#fdba74' : '#c2410c',
            };
        case 'shopping':
            return {
                bgcolor: alpha('#60a5fa', isDark ? 0.22 : 0.18),
                color: isDark ? '#93c5fd' : '#1d4ed8',
            };
        case 'atm':
            return {
                bgcolor: alpha('#a78bfa', isDark ? 0.22 : 0.18),
                color: isDark ? '#c4b5fd' : '#6d28d9',
            };
        case 'transfer':
            return {
                bgcolor: alpha('#38bdf8', isDark ? 0.22 : 0.18),
                color: isDark ? '#7dd3fc' : '#0369a1',
            };
        case 'salary':
            return {
                bgcolor: alpha('#4ade80', isDark ? 0.22 : 0.18),
                color: isDark ? '#86efac' : '#15803d',
            };
        default:
            return {
                bgcolor: alpha('#94a3b8', isDark ? 0.22 : 0.16),
                color: isDark ? '#cbd5e1' : '#475569',
            };
    }
}

export function statCardIconStyle(
    theme: Theme,
    color: 'primary' | 'secondary' | 'success',
): SxProps<Theme> {
    const isDark = theme.palette.mode === 'dark';
    const palette = theme.palette[color];
    return {
        width: 30,
        height: 30,
        bgcolor: alpha(palette.main, isDark ? 0.2 : 0.14),
        color: isDark ? palette.light : palette.dark,
        border: `1px solid ${alpha(palette.main, isDark ? 0.5 : 0.38)}`,
    };
}

export function getTransactionCategoryIcon(category: TransactionCategory, size: 'small' | 'medium' = 'medium') {
    const fontSize = size === 'small' ? 16 : 18;
    const sx = { fontSize };

    switch (category) {
        case 'shopping':
            return <ShoppingCartOutlinedIcon sx={sx} />;
        case 'food':
            return <RestaurantOutlinedIcon sx={sx} />;
        case 'atm':
            return <LocalAtmOutlinedIcon sx={sx} />;
        case 'transfer':
            return <SwapHorizOutlinedIcon sx={sx} />;
        case 'salary':
            return <ArrowUpwardRoundedIcon sx={sx} />;
        default:
            return <PaymentsOutlinedIcon sx={sx} />;
    }
}
