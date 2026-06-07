import InfoOutlinedIcon from '@mui/icons-material/InfoOutlined';
import { Stack, Typography } from '@mui/material';
import type { ReactNode } from 'react';

type FormInfoNoteProps = {
    children: ReactNode;
};

/** Inline info hint — same placement and look across transaction forms. */
export function FormInfoNote({ children }: FormInfoNoteProps) {
    return (
        <Stack direction="row" spacing={1.25} sx={{ alignItems: 'flex-start' }}>
            <InfoOutlinedIcon sx={{ fontSize: 20, color: 'info.main', mt: 0.25, flexShrink: 0 }} />
            <Typography variant="body2" color="text.secondary" component="div">
                {children}
            </Typography>
        </Stack>
    );
}
