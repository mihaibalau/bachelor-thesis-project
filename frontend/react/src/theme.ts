import { alpha, createTheme } from '@mui/material/styles';
import type { PaletteMode, ThemeOptions } from '@mui/material';

// MUI design tokens for Gentlix (gold primary, dark-first palette).
const getDesignTokens = (mode: PaletteMode): ThemeOptions => ({
    palette: {
        mode,
        background:
            mode === 'dark'
                ? {
                    default: '#050509',
                    paper: '#101018',
                }
                : {
                    default: '#f3f4f6',
                    paper: '#ffffff',
                },
        primary: {
            main: '#f5c451',
            light: '#ffe29f',
            dark: '#d79b2d',
        },
        secondary: {
            main: '#38bdf8',
        },
        text:
            mode === 'dark'
                ? {
                    primary: '#f9fafb',
                    secondary: '#9ca3af',
                }
                : {
                    primary: '#020617',
                    secondary: '#6b7280',
                },
        divider: 'rgba(148, 163, 184, 0.4)',
    },
    typography: {
        fontFamily:
            '"Inter", system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif',
        h1: {
            fontWeight: 600,
            letterSpacing: 0.02,
        },
        h4: {
            fontWeight: 600,
            letterSpacing: 0.02,
        },
        button: {
            textTransform: 'none',
            fontWeight: 500,
            letterSpacing: 0.04,
        },
    },
    shape: {
        borderRadius: 10,
    },
    components: {
        MuiPaper: {
            styleOverrides: {
                root: {
                    backgroundImage: 'none',
                    borderRadius: 14,
                    ...(mode === 'dark'
                        ? {
                            border: '1px solid rgba(148, 163, 184, 0.3)',
                            boxShadow:
                                '0 18px 45px rgba(0,0,0,0.55), 0 0 0 1px rgba(15,23,42,0.8)',
                        }
                        : {
                            border: '1px solid rgba(148, 163, 184, 0.22)',
                            boxShadow: '0 1px 3px rgba(15, 23, 42, 0.06)',
                        }),
                },
            },
        },
        MuiButton: {
            defaultProps: {
                disableElevation: true,
            },
            styleOverrides: {
                root: {
                    borderRadius: 10,
                    paddingInline: 20,
                    paddingBlock: 9,
                    boxShadow: 'none',
                },
                contained: ({ theme }) => {
                    const isDark = theme.palette.mode === 'dark';
                    return {
                        fontWeight: 600,
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
                            boxShadow: 'none',
                        },
                    };
                },
                outlined: ({ theme }) => {
                    const isDark = theme.palette.mode === 'dark';
                    return {
                        boxShadow: 'none',
                        borderColor: alpha(theme.palette.divider, 0.95),
                        color: theme.palette.text.primary,
                        backgroundColor: isDark
                            ? alpha(theme.palette.background.paper, 0.45)
                            : theme.palette.background.paper,
                        '&:hover': {
                            boxShadow: 'none',
                            borderColor: alpha(theme.palette.primary.main, 0.55),
                            backgroundColor: alpha(
                                theme.palette.primary.main,
                                isDark ? 0.09 : 0.05,
                            ),
                        },
                    };
                },
                text: {
                    boxShadow: 'none',
                    '&:hover': {
                        boxShadow: 'none',
                    },
                },
            },
        },
        MuiTextField: {
            defaultProps: {
                variant: 'outlined',
            },
            styleOverrides: {
                root: {
                    '& .MuiOutlinedInput-root': {
                        borderRadius: 12,
                        backgroundColor:
                            mode === 'dark'
                                ? 'rgba(15,23,42,0.85)'
                                : '#f9fafb',
                        '& fieldset': {
                            borderColor: 'rgba(148,163,184,0.4)',
                        },
                        '&:hover fieldset': {
                            borderColor: '#f5c451',
                        },
                        '&.Mui-focused fieldset': {
                            borderColor: '#f5c451',
                            boxShadow: '0 0 0 1px rgba(245, 196, 81, 0.4)',
                        },
                    },
                    '& .MuiInputLabel-root': {
                        color: 'rgba(148,163,184,0.9)',
                    },
                },
            },
        },
    },
});

export const createAppTheme = (mode: PaletteMode) =>
    createTheme(getDesignTokens(mode));
