import type { ReactNode } from 'react';
import { Navigate } from 'react-router-dom';
import { Box, CircularProgress } from '@mui/material';
import { useAuth } from './AuthContext';

export function ProtectedRoute({ children }: { children: ReactNode }) {
    const { token, isReady } = useAuth();

    if (!isReady) {
        return (
            <Box
                sx={{
                    minHeight: '100vh',
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'center',
                    bgcolor: 'background.default',
                }}
            >
                <CircularProgress color="primary" />
            </Box>
        );
    }

    if (!token) {
        return <Navigate to="/login" replace />;
    }

    return <>{children}</>;
}