import { Box, Stack, Typography } from '@mui/material';
import type { ReactNode } from 'react';

type Props = {
    title: string;
    subtitle?: string;
    action?: ReactNode;
};

export function PageHeader({ title, subtitle, action }: Props) {
    return (
        <Stack
            direction={{ xs: 'column', sm: 'row' }}
            sx={{ justifyContent: 'space-between', alignItems: { xs: 'flex-start', sm: 'center' }, mb: 3, gap: 2 }}
        >
            <Box>
                <Typography variant="h4" sx={{ fontWeight: 600 }}>
                    {title}
                </Typography>
                {subtitle && (
                    <Typography variant="body2" sx={{ color: 'text.secondary', mt: 0.5 }}>
                        {subtitle}
                    </Typography>
                )}
            </Box>
            {action}
        </Stack>
    );
}
