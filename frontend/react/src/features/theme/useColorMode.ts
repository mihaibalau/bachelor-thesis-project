import { useContext } from 'react';
import { ColorModeContext } from './ColorModeContext';

export const useColorMode = () => {
    const ctx = useContext(ColorModeContext);
    if (!ctx) {
        throw new Error('useColorMode must be used inside AppThemeProvider');
    }
    return ctx;
};