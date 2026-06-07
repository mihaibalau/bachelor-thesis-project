import { Alert, AlertTitle, Box, Button } from '@mui/material';
import type { ApiError } from '../apiError';

type Props = {
    error: ApiError | string | null;
    onRetry?: () => void;
    title?: string;
};

export function ErrorAlert({ error, onRetry, title }: Props) {
    if (!error) return null;

    const apiError = typeof error === 'string' ? null : error;
    const message = typeof error === 'string' ? error : error.message;
    const heading = title ?? apiError?.title ?? 'Error';

    return (
        <Alert
            severity="error"
            sx={{ borderRadius: 2 }}
            action={
                onRetry ? (
                    <Button color="inherit" size="small" onClick={onRetry}>
                        Retry
                    </Button>
                ) : undefined
            }
        >
            <AlertTitle sx={{ fontWeight: 600 }}>{heading}</AlertTitle>
            <Box component="span" sx={{ display: 'block' }}>
                {message}
            </Box>
            {apiError?.code && apiError.code !== 'unknown_error' && (
                <Box
                    component="span"
                    sx={{ display: 'block', mt: 0.5, fontSize: '0.75rem', opacity: 0.8 }}
                >
                    Code: {apiError.code}
                </Box>
            )}
        </Alert>
    );
}
