import { Box, Grid, Paper, Skeleton, Stack } from '@mui/material';
import { alpha, useTheme } from '@mui/material/styles';

export function DashboardSkeleton() {
    const theme = useTheme();
    const isDark = theme.palette.mode === 'dark';
    const cardBg = isDark
        ? alpha(theme.palette.background.paper, 0.9)
        : theme.palette.background.paper;

    return (
        <Stack spacing={3}>
            <Paper
                sx={{
                    p: { xs: 2.5, md: 3 },
                    borderRadius: 4,
                    bgcolor: cardBg,
                }}
            >
                <Stack
                    direction={{ xs: 'column', md: 'row' }}
                    spacing={2}
                    sx={{ justifyContent: 'space-between' }}
                >
                    <Box sx={{ width: { xs: '100%', md: '50%' } }}>
                        <Skeleton variant="text" width="60%" height={40} />
                        <Skeleton variant="text" width="80%" />
                    </Box>
                    <Stack
                        spacing={1}
                        sx={{ alignItems: { xs: 'flex-start', md: 'flex-end' } }}
                    >
                        <Skeleton variant="text" width={100} />
                        <Skeleton variant="text" width={160} height={36} />
                        <Skeleton variant="text" width={120} />
                    </Stack>
                </Stack>
            </Paper>

            <Grid container spacing={2.5}>
                {[0, 1, 2].map((i) => (
                    <Grid key={i} size={{ xs: 12, md: 4 }}>
                        <Paper sx={{ p: 2.5, minHeight: 140, borderRadius: 3, bgcolor: cardBg }}>
                            <Skeleton variant="text" width="40%" />
                            <Skeleton variant="text" width="30%" height={36} sx={{ mt: 2 }} />
                        </Paper>
                    </Grid>
                ))}
            </Grid>

            <Grid container spacing={2.5}>
                <Grid size={{ xs: 12, md: 7 }}>
                    <Paper sx={{ p: 2.5, borderRadius: 3, bgcolor: cardBg }}>
                        <Skeleton variant="text" width="30%" height={28} />
                        <Skeleton variant="text" width="40%" />
                        <Stack spacing={1.5} sx={{ mt: 2 }}>
                            {[0, 1, 2, 3, 4, 5].map((i) => (
                                <Stack
                                    key={i}
                                    direction="row"
                                    spacing={1.5}
                                    sx={{ alignItems: 'center' }}
                                >
                                    <Skeleton variant="circular" width={32} height={32} />
                                    <Box sx={{ flex: 1 }}>
                                        <Skeleton variant="text" width="50%" />
                                        <Skeleton variant="text" width="70%" />
                                    </Box>
                                    <Skeleton variant="text" width={80} />
                                </Stack>
                            ))}
                        </Stack>
                    </Paper>
                </Grid>
                <Grid size={{ xs: 12, md: 5 }}>
                    <Paper sx={{ p: 2.5, minHeight: 320, borderRadius: 3, bgcolor: cardBg }}>
                        <Skeleton variant="text" width="50%" height={28} />
                        <Skeleton variant="text" width="60%" />
                        <Skeleton
                            variant="rounded"
                            height={220}
                            sx={{ mt: 2, borderRadius: 2 }}
                        />
                    </Paper>
                </Grid>
            </Grid>
        </Stack>
    );
}
