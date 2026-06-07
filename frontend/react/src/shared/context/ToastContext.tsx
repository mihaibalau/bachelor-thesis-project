import { Alert, Snackbar } from '@mui/material';
import { createContext, useCallback, useContext, useState, type ReactNode } from 'react';

type ToastSeverity = 'success' | 'error' | 'info';

type ToastState = {
    open: boolean;
    message: string;
    severity: ToastSeverity;
};

type ToastContextValue = {
    showSuccess: (message: string) => void;
    showError: (message: string) => void;
    showInfo: (message: string) => void;
};

const ToastContext = createContext<ToastContextValue | undefined>(undefined);

export function ToastProvider({ children }: { children: ReactNode }) {
    // 1. Hold a single snackbar slot shared across the app.
    const [toast, setToast] = useState<ToastState>({
        open: false,
        message: '',
        severity: 'info',
    });

    const show = useCallback((message: string, severity: ToastSeverity) => {
        setToast({ open: true, message, severity });
    }, []);

    // 2. Expose severity-specific helpers for feature pages.
    const value: ToastContextValue = {
        showSuccess: (m) => show(m, 'success'),
        showError: (m) => show(m, 'error'),
        showInfo: (m) => show(m, 'info'),
    };

    return (
        <ToastContext.Provider value={value}>
            {children}
            <Snackbar
                open={toast.open}
                autoHideDuration={5000}
                onClose={() => setToast((t) => ({ ...t, open: false }))}
                anchorOrigin={{ vertical: 'bottom', horizontal: 'center' }}
            >
                <Alert
                    severity={toast.severity}
                    variant="filled"
                    onClose={() => setToast((t) => ({ ...t, open: false }))}
                    sx={{ width: '100%', borderRadius: 2 }}
                >
                    {toast.message}
                </Alert>
            </Snackbar>
        </ToastContext.Provider>
    );
}

// eslint-disable-next-line react-refresh/only-export-components
export function useToast() {
    const ctx = useContext(ToastContext);
    if (!ctx) throw new Error('useToast must be used within ToastProvider');
    return ctx;
}
