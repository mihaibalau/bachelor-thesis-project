import { createTheme } from '@mui/material/styles';

export const theme = createTheme({
    palette: {
        mode: 'dark',
        background: {
            default: '#050509',
            paper: '#101018',
        },
        primary: {
            main: '#f5c451',
            light: '#ffe29f',
            dark: '#d79b2d',
        },
        secondary: {
            main: '#38bdf8',
        },
        text: {
            primary: '#f9fafb',
            secondary: '#9ca3af',
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
        borderRadius: 16,
    },
    components: {
        MuiPaper: {
            styleOverrides: {
                root: {
                    backgroundImage: 'none',
                    borderRadius: 20,
                    border: '1px solid rgba(148, 163, 184, 0.3)',
                    boxShadow:
                        '0 18px 45px rgba(0,0,0,0.55), 0 0 0 1px rgba(15,23,42,0.8)',
                },
            },
        },
        MuiButton: {
            defaultProps: {
                variant: 'contained',
                color: 'primary',
            },
            styleOverrides: {
                root: {
                    borderRadius: 999,
                    paddingInline: 24,
                    paddingBlock: 10,
                    boxShadow:
                        '0 10px 25px rgba(0,0,0,0.45), 0 0 0 1px rgba(161, 98, 7, 0.6)',
                },
                contained: {
                    background:
                        'linear-gradient(135deg, #f5c451 0%, #d79b2d 40%, #b8860b 100%)',
                    color: '#050509',
                    '&:hover': {
                        background:
                            'linear-gradient(135deg, #ffe29f 0%, #f5c451 40%, #d79b2d 100%)',
                    },
                },
                outlined: {
                    borderColor: 'rgba(148,163,184,0.45)',
                    '&:hover': {
                        borderColor: '#f5c451',
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
                        borderRadius: 999,
                        backgroundColor: 'rgba(15,23,42,0.85)',
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