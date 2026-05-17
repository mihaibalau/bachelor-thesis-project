import { useMemo, useState, type ReactNode } from 'react';
import { CssBaseline, ThemeProvider } from '@mui/material';
import type { PaletteMode } from '@mui/material';
import { createAppTheme } from '../../theme.ts';
import { ColorModeContext } from './ColorModeContext';

export function AppThemeProvider({ children }: { children: ReactNode }) {
    const [mode, setMode] = useState<PaletteMode>(() => {
        const stored = localStorage.getItem('gentlix-color-mode');
        return stored === 'light' || stored === 'dark' ? stored : 'dark';
    });

    const toggleMode = () => {
        setMode((prev) => {
            const next = prev === 'dark' ? 'light' : 'dark';
            localStorage.setItem('gentlix-color-mode', next);
            return next;
        });
    };

    const theme = useMemo(() => createAppTheme(mode), [mode]);

    return (
        <ColorModeContext.Provider value={{ mode, toggleMode }}>
            <ThemeProvider theme={theme}>
                <CssBaseline />
                {children}
            </ThemeProvider>
        </ColorModeContext.Provider>
    );
}