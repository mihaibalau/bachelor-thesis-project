import { createContext } from 'react';
import type { PaletteMode } from '@mui/material';

export type ColorModeContextType = {
    mode: PaletteMode;
    toggleMode: () => void;
};

export const ColorModeContext = createContext<ColorModeContextType | undefined>(
    undefined,
);