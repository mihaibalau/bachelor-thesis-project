import { createContext } from 'react';
import type { PaletteMode } from '@mui/material';

// Provided by AppThemeProvider; consumed via useColorMode().
export type ColorModeContextType = {
    mode: PaletteMode;
    toggleMode: () => void;
};

export const ColorModeContext = createContext<ColorModeContextType | undefined>(
    undefined,
);