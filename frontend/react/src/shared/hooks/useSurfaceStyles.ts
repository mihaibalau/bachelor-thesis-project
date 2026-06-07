import { alpha, useTheme } from '@mui/material/styles';

export function useSurfaceStyles() {
    const theme = useTheme();
    const isDark = theme.palette.mode === 'dark';

    const cardBg = isDark
        ? alpha(theme.palette.background.paper, 0.9)
        : theme.palette.background.paper;

    const cardBorder = `1px solid ${alpha(theme.palette.divider, 0.9)}`;

    const cardShadow = isDark
        ? '0 18px 40px rgba(0,0,0,0.7)'
        : '0 1px 3px rgba(15,23,42,0.06)';

    const cardHoverShadow = isDark
        ? '0 22px 48px rgba(0,0,0,0.75)'
        : '0 4px 14px rgba(15,23,42,0.08)';

    const heroBg = isDark
        ? `linear-gradient(120deg,
            ${theme.palette.background.default} 0%,
            ${theme.palette.background.default} 35%,
            ${alpha(theme.palette.background.paper, 0.96)} 70%,
            ${alpha(theme.palette.background.paper, 0.98)} 100%)`
        : `linear-gradient(120deg,
            ${theme.palette.background.paper} 0%,
            ${alpha(theme.palette.primary.light, 0.12)} 60%,
            ${alpha(theme.palette.secondary.light, 0.08)} 100%)`;

    const inputBg = isDark
        ? alpha(theme.palette.background.default, 0.55)
        : theme.palette.background.paper;

    const filterFieldSx = {
        '& .MuiOutlinedInput-root': {
            borderRadius: 2.5,
            bgcolor: inputBg,
        },
    };

    return {
        isDark,
        theme,
        cardBg,
        cardBorder,
        cardShadow,
        cardHoverShadow,
        heroBg,
        inputBg,
        filterFieldSx,
        surfaceCard: {
            borderRadius: 3,
            bgcolor: cardBg,
            border: cardBorder,
            boxShadow: cardShadow,
            elevation: 0,
        },
    };
}
