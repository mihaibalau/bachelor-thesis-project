import ErrorOutlineRoundedIcon from '@mui/icons-material/ErrorOutlineRounded';
import { Box, Typography } from '@mui/material';
import { alpha } from '@mui/material/styles';

type Props = {
    message: string | null | undefined;
};

export function InlineError({ message }: Props) {
    if (!message) return null;

    return (
        <Box
            sx={{
                display: 'flex',
                alignItems: 'flex-start',
                gap: 1,
                px: 1.5,
                py: 1,
                borderRadius: 1.5,
                bgcolor: (t) => alpha(t.palette.error.main, 0.08),
                border: (t) => `1px solid ${alpha(t.palette.error.main, 0.25)}`,
            }}
        >
            <ErrorOutlineRoundedIcon color="error" sx={{ fontSize: 18, mt: 0.15 }} />
            <Typography variant="body2" color="error.main" sx={{ lineHeight: 1.45 }}>
                {message}
            </Typography>
        </Box>
    );
}
