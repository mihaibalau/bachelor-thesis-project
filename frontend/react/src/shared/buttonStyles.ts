import { alpha, type Theme } from '@mui/material/styles';
import type { SxProps } from '@mui/material';

/** Preserved look for auth pages — do not change Sign in / Register submit. */
export const AUTH_PRIMARY_BUTTON_SX: SxProps<Theme> = {
    background: 'linear-gradient(135deg, #f5c451 0%, #d79b2d 40%, #b8860b 100%)',
    color: '#050509',
    boxShadow: '0 10px 25px rgba(0,0,0,0.45), 0 0 0 1px rgba(161, 98, 7, 0.6)',
    '&:hover': {
        background: 'linear-gradient(135deg, #ffe29f 0%, #f5c451 40%, #d79b2d 100%)',
        boxShadow: '0 12px 28px rgba(0,0,0,0.5), 0 0 0 1px rgba(161, 98, 7, 0.7)',
    },
};

/** Matches the dashboard "View all" pill — reference style for in-app gold actions. */
export function primaryPillButtonSx(theme: Theme, size: 'small' | 'medium' = 'small'): SxProps<Theme> {
    const isDark = theme.palette.mode === 'dark';
    const sizeSx =
        size === 'small'
            ? { px: 2, py: 0.35, fontSize: '0.75rem', lineHeight: 1.4 }
            : { px: 2.5, py: 0.9, fontSize: '0.875rem' };

    return {
        textTransform: 'none',
        borderRadius: 999,
        minWidth: 0,
        fontWeight: 600,
        color: isDark ? '#050509' : '#1a1206',
        background: isDark
            ? `linear-gradient(135deg, ${theme.palette.primary.light} 0%, ${theme.palette.primary.main} 55%, ${theme.palette.primary.dark} 100%)`
            : `linear-gradient(135deg, ${theme.palette.primary.main} 0%, ${theme.palette.primary.dark} 100%)`,
        border: `1px solid ${alpha(theme.palette.primary.dark, 0.55)}`,
        boxShadow: isDark
            ? `0 4px 14px ${alpha(theme.palette.primary.main, 0.28)}`
            : `0 4px 12px ${alpha(theme.palette.primary.dark, 0.22)}`,
        '&:hover': {
            background: isDark
                ? `linear-gradient(135deg, ${theme.palette.primary.main} 0%, ${theme.palette.primary.dark} 100%)`
                : `linear-gradient(135deg, ${theme.palette.primary.light} 0%, ${theme.palette.primary.main} 100%)`,
            boxShadow: isDark
                ? `0 6px 18px ${alpha(theme.palette.primary.main, 0.38)}`
                : `0 6px 16px ${alpha(theme.palette.primary.dark, 0.28)}`,
        },
        ...sizeSx,
    };
}

export function appPrimaryButtonSx(theme: Theme): SxProps<Theme> {
    const isDark = theme.palette.mode === 'dark';
    return {
        textTransform: 'none',
        fontWeight: 600,
        borderRadius: 2.5,
        color: isDark ? '#050509' : '#1a1206',
        background: isDark
            ? `linear-gradient(135deg, ${theme.palette.primary.light} 0%, ${theme.palette.primary.main} 55%, ${theme.palette.primary.dark} 100%)`
            : `linear-gradient(135deg, ${theme.palette.primary.main} 0%, ${theme.palette.primary.dark} 100%)`,
        border: `1px solid ${alpha(theme.palette.primary.dark, 0.5)}`,
        boxShadow: isDark
            ? `0 4px 14px ${alpha(theme.palette.primary.main, 0.26)}`
            : `0 3px 10px ${alpha(theme.palette.primary.dark, 0.2)}`,
        '&:hover': {
            background: isDark
                ? `linear-gradient(135deg, ${theme.palette.primary.main} 0%, ${theme.palette.primary.dark} 100%)`
                : `linear-gradient(135deg, ${theme.palette.primary.light} 0%, ${theme.palette.primary.main} 100%)`,
            boxShadow: isDark
                ? `0 6px 18px ${alpha(theme.palette.primary.main, 0.34)}`
                : `0 5px 14px ${alpha(theme.palette.primary.dark, 0.26)}`,
        },
        '&.Mui-disabled': {
            opacity: 0.55,
            color: isDark ? '#050509' : '#1a1206',
            background: isDark
                ? alpha(theme.palette.primary.main, 0.45)
                : alpha(theme.palette.primary.main, 0.55),
            boxShadow: 'none',
        },
    };
}

export function softOutlinedButtonSx(theme: Theme): SxProps<Theme> {
    const isDark = theme.palette.mode === 'dark';
    return {
        textTransform: 'none',
        fontWeight: 500,
        borderRadius: 2.5,
        boxShadow: 'none',
        borderColor: alpha(theme.palette.divider, 0.95),
        color: 'text.primary',
        bgcolor: isDark ? alpha(theme.palette.background.paper, 0.45) : theme.palette.background.paper,
        '&:hover': {
            boxShadow: 'none',
            borderColor: alpha(theme.palette.primary.main, 0.55),
            bgcolor: alpha(theme.palette.primary.main, isDark ? 0.09 : 0.05),
        },
    };
}

export function backButtonSx(theme: Theme): SxProps<Theme> {
    const isDark = theme.palette.mode === 'dark';
    return {
        mb: 2,
        textTransform: 'none',
        fontWeight: 500,
        color: 'text.secondary',
        boxShadow: 'none',
        px: 1.25,
        py: 0.75,
        borderRadius: 2,
        '&:hover': {
            bgcolor: alpha(theme.palette.primary.main, isDark ? 0.08 : 0.06),
            color: 'text.primary',
            boxShadow: 'none',
        },
    };
}

function goldPillColors(theme: Theme) {
    const isDark = theme.palette.mode === 'dark';
    return {
        isDark,
        textColor: isDark ? '#050509' : '#1a1206',
        activeBg: isDark
            ? `linear-gradient(135deg, ${theme.palette.primary.light} 0%, ${theme.palette.primary.main} 55%, ${theme.palette.primary.dark} 100%)`
            : `linear-gradient(135deg, ${theme.palette.primary.main} 0%, ${theme.palette.primary.dark} 100%)`,
        hoverBg: isDark
            ? `linear-gradient(135deg, ${theme.palette.primary.main} 0%, ${theme.palette.primary.dark} 100%)`
            : `linear-gradient(135deg, ${theme.palette.primary.light} 0%, ${theme.palette.primary.main} 100%)`,
        activeBorder: `1px solid ${alpha(theme.palette.primary.dark, 0.55)}`,
    };
}

/**
 * Medium gold pill for form actions — matches active filter/toggle pills
 * (bright gradient, dark text, no heavy shadow).
 */
export function primaryPillActionButtonSx(theme: Theme): SxProps<Theme> {
    const { isDark, textColor, activeBg, hoverBg, activeBorder } = goldPillColors(theme);

    return {
        textTransform: 'none',
        borderRadius: 999,
        minWidth: 0,
        fontWeight: 600,
        px: 2.5,
        py: 0.9,
        fontSize: '0.875rem',
        color: textColor,
        background: activeBg,
        border: activeBorder,
        boxShadow: 'none',
        '&:hover': {
            color: textColor,
            background: hoverBg,
            boxShadow: 'none',
        },
        '&.MuiButton-contained': {
            color: textColor,
            background: activeBg,
            boxShadow: 'none',
            '&:hover': {
                color: textColor,
                background: hoverBg,
                boxShadow: 'none',
            },
        },
        '&.Mui-disabled': {
            opacity: 1,
            color: alpha(textColor, 0.72),
            background: isDark
                ? alpha(theme.palette.primary.main, 0.38)
                : alpha(theme.palette.primary.main, 0.48),
            border: `1px solid ${alpha(theme.palette.primary.dark, 0.32)}`,
        },
    };
}

/** Shared layout for MUI ToggleButtonGroup pill rows. */
export function togglePillGroupSx(): SxProps<Theme> {
    return {
        display: 'flex',
        flexWrap: 'wrap',
        gap: 1,
        '& .MuiToggleButtonGroup-grouped': {
            border: 0,
            borderRadius: '999px !important',
            mx: 0,
        },
        '& .MuiToggleButton-root': {
            border: 'none',
        },
    };
}

/** Pill toggle for filter rows (Statements, ATM mode, payment categories). */
export function filterPillSx(theme: Theme, active: boolean): SxProps<Theme> {
    if (active) {
        const { textColor, activeBg, hoverBg, activeBorder } = goldPillColors(theme);

        return {
            textTransform: 'none',
            fontWeight: 600,
            borderRadius: 999,
            minWidth: 0,
            px: 2,
            py: 0.5,
            fontSize: '0.8125rem',
            boxShadow: 'none',
            color: textColor,
            background: activeBg,
            border: activeBorder,
            '&:hover': {
                color: textColor,
                background: hoverBg,
                boxShadow: 'none',
            },
            '&.Mui-selected': {
                color: textColor,
                background: activeBg,
                border: activeBorder,
                boxShadow: 'none',
                '&:hover': {
                    color: textColor,
                    background: hoverBg,
                    boxShadow: 'none',
                },
            },
        };
    }
    const isDark = theme.palette.mode === 'dark';
    return {
        textTransform: 'none',
        fontWeight: 600,
        borderRadius: 999,
        minWidth: 0,
        px: 2,
        py: 0.5,
        fontSize: '0.8125rem',
        color: 'text.secondary',
        border: `1px solid ${alpha(theme.palette.divider, 0.95)}`,
        bgcolor: isDark ? alpha(theme.palette.background.paper, 0.45) : theme.palette.background.paper,
        boxShadow: 'none',
        '&:hover': {
            boxShadow: 'none',
            borderColor: alpha(theme.palette.primary.main, 0.55),
            bgcolor: alpha(theme.palette.primary.main, isDark ? 0.09 : 0.05),
            color: 'text.primary',
        },
    };
}

export function cardGhostButtonSx(theme: Theme): SxProps<Theme> {
    const isDark = theme.palette.mode === 'dark';
    return {
        textTransform: 'none',
        borderRadius: 999,
        fontWeight: 500,
        fontSize: '0.8125rem',
        boxShadow: 'none',
        borderColor: alpha(theme.palette.divider, 0.9),
        bgcolor: isDark ? alpha(theme.palette.background.default, 0.55) : alpha(theme.palette.grey[50], 0.9),
        '&:hover': {
            boxShadow: 'none',
            borderColor: alpha(theme.palette.primary.main, 0.45),
            bgcolor: alpha(theme.palette.primary.main, isDark ? 0.1 : 0.06),
        },
    };
}
