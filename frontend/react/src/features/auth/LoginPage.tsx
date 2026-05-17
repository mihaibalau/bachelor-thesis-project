import { useState } from 'react';
import { useForm } from 'react-hook-form';
import { z } from 'zod';
import { zodResolver } from '@hookform/resolvers/zod';
import {
    Alert,
    Box,
    Button,
    Container,
    Divider,
    IconButton,
    InputAdornment,
    Paper,
    TextField,
    Typography,
} from '@mui/material';
import { alpha, useTheme } from '@mui/material/styles';
import VisibilityOffOutlinedIcon from '@mui/icons-material/VisibilityOffOutlined';
import VisibilityOutlinedIcon from '@mui/icons-material/VisibilityOutlined';
import ArrowForwardRoundedIcon from '@mui/icons-material/ArrowForwardRounded';
import DarkModeOutlinedIcon from '@mui/icons-material/DarkModeOutlined';
import LightModeOutlinedIcon from '@mui/icons-material/LightModeOutlined';
import type { MouseEvent } from 'react';
import { useNavigate } from 'react-router-dom';
import { useAuth } from './AuthContext';
import { useColorMode } from '../theme/useColorMode';
import gentlixLogo from '../../assets/logo.png';

const LoginSchema = z.object({
    email: z.string().email('Please enter a valid email address'),
    password: z.string().min(8, 'Password must be at least 8 characters'),
});

type LoginFormValues = z.infer<typeof LoginSchema>;

export function LoginPage() {
    const { loginUser } = useAuth();
    const navigate = useNavigate();
    const [serverError, setServerError] = useState<string | null>(null);
    const [showPassword, setShowPassword] = useState(false);
    const { mode, toggleMode } = useColorMode();
    const theme = useTheme();
    const isDark = theme.palette.mode === 'dark';

    const {
        register,
        handleSubmit,
        formState: { errors, isSubmitting },
    } = useForm<LoginFormValues>({
        resolver: zodResolver(LoginSchema),
        defaultValues: {
            email: '',
            password: '',
        },
    });

    const onSubmit = async (data: LoginFormValues) => {
        setServerError(null);
        try {
            await loginUser({
                email: data.email,
                password: data.password,
            });
            navigate('/app');
        } catch (err) {
            console.error(err);
            setServerError(
                'Invalid credentials or server unavailable. Please try again.',
            );
        }
    };

    const handleTogglePassword = (event: MouseEvent<HTMLButtonElement>) => {
        event.preventDefault();
        setShowPassword((prev) => !prev);
    };

    const loginBackground = isDark
        ? 'radial-gradient(circle at 0% 0%, rgba(245,196,81,0.16), transparent 55%), radial-gradient(circle at 100% 0%, rgba(56,189,248,0.26), transparent 55%), linear-gradient(145deg, #050509 0%, #050814 45%, #040612 100%)'
        : `linear-gradient(145deg,
            ${theme.palette.background.default} 0%,
            ${theme.palette.grey['100']} 35%,
            ${alpha(theme.palette.primary.light, 0.35)} 70%,
            ${alpha(theme.palette.secondary.light, 0.22)} 100%)`;

    return (
        <Box
            sx={{
                position: 'relative',
                minHeight: '100vh',
                display: 'flex',
                alignItems: 'stretch',
                justifyContent: 'center',
                bgcolor: 'background.default',
                background: loginBackground,
                transition: 'background 200ms ease',
            }}
        >
            <IconButton
                onClick={toggleMode}
                color="inherit"
                sx={{
                    position: 'absolute',
                    top: { xs: 12, md: 16 },
                    right: { xs: 12, md: 24 },
                    borderRadius: 10,
                    bgcolor: isDark
                        ? 'rgba(15,23,42,0.85)'
                        : 'rgba(255,255,255,0.9)',
                    border: '1px solid rgba(148,163,184,0.6)',
                    boxShadow: '0 10px 25px rgba(0,0,0,0.35)',
                    zIndex: 2,
                    '&:hover': {
                        borderColor: theme.palette.primary.main,
                        bgcolor: isDark
                            ? 'rgba(15,23,42,0.98)'
                            : 'rgba(249,250,251,1)',
                    },
                }}
            >
                {mode === 'dark' ? (
                    <LightModeOutlinedIcon fontSize="small" />
                ) : (
                    <DarkModeOutlinedIcon fontSize="small" />
                )}
            </IconButton>

            <Container
                maxWidth="lg"
                sx={{
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'center',
                    py: { xs: 4, md: 6 },
                }}
            >
                <Box
                    sx={{
                        display: 'grid',
                        gridTemplateColumns: { xs: '1fr', md: '1.1fr 1fr' },
                        gap: { xs: 4, md: 6 },
                        width: '100%',
                        alignItems: 'stretch',
                    }}
                >
                    <Box
                        sx={{
                            display: 'flex',
                            flexDirection: 'column',
                            justifyContent: 'space-between',
                        }}
                    >
                        <Box sx={{ mb: 4 }}>
                            <Box
                                sx={{
                                    display: 'flex',
                                    alignItems: 'center',
                                    mb: 3,
                                }}
                            >
                                <Box
                                    component="img"
                                    src={gentlixLogo}
                                    alt="Gentlix Bank logo"
                                    sx={{
                                        width: 52,
                                        height: 52,
                                        mr: 1.5,
                                        borderRadius: 2,
                                    }}
                                />
                                <Typography
                                    variant="h5"
                                    sx={{
                                        fontWeight: 600,
                                        letterSpacing: 0.16,
                                    }}
                                >
                                    Gentlix Bank
                                </Typography>
                            </Box>

                            <Typography
                                variant="h3"
                                sx={{
                                    fontWeight: 600,
                                    mb: 2,
                                    maxWidth: 480,
                                }}
                            >
                                Banking for people who don&apos;t hit limits.
                            </Typography>

                            <Typography
                                variant="body1"
                                sx={{
                                    color: 'text.secondary',
                                    maxWidth: 480,
                                }}
                            >
                                Manage your accounts, transfers, and affiliates in real-time
                                with a banking experience built for developers and power users.
                            </Typography>
                        </Box>

                        <Box
                            sx={{
                                display: { xs: 'none', md: 'flex' },
                                flexDirection: 'column',
                                gap: 1,
                            }}
                        >
                            <Typography
                                variant="caption"
                                sx={{
                                    textTransform: 'uppercase',
                                    letterSpacing: 1.6,
                                }}
                                color="text.secondary"
                            >
                                security & compliance
                            </Typography>
                            <Typography variant="body2" color="text.secondary">
                                JWT-secured sessions and encrypted connections.
                            </Typography>
                        </Box>
                    </Box>

                    {/* Right side – sign-in card */}
                    <Paper
                        sx={{
                            p: { xs: 3, md: 4 },
                            bgcolor: 'background.paper',
                            backdropFilter: 'blur(24px)',
                            border: '1px solid',
                            borderColor: isDark
                                ? 'rgba(30,41,59,0.9)'
                                : 'rgba(203,213,225,0.8)',
                        }}
                    >
                        <Typography
                            variant="h5"
                            sx={{ mb: 1.5, fontWeight: 600 }}
                        >
                            Sign in to Gentlix
                        </Typography>
                        <Typography
                            variant="body2"
                            sx={{ mb: 3, color: 'text.secondary' }}
                        >
                            Enter your credentials to access your account.
                        </Typography>

                        {serverError && (
                            <Alert severity="error" sx={{ mb: 2 }}>
                                {serverError}
                            </Alert>
                        )}

                        <Box
                            component="form"
                            noValidate
                            onSubmit={handleSubmit(onSubmit)}
                            sx={{
                                display: 'flex',
                                flexDirection: 'column',
                                gap: 2,
                            }}
                        >
                            <TextField
                                label="Email address"
                                type="email"
                                autoComplete="email"
                                autoFocus
                                {...register('email')}
                                error={!!errors.email}
                                helperText={errors.email?.message}
                            />

                            <TextField
                                label="Password"
                                type={showPassword ? 'text' : 'password'}
                                autoComplete="current-password"
                                {...register('password')}
                                error={!!errors.password}
                                helperText={errors.password?.message}
                                slotProps={{
                                    input: {
                                        endAdornment: (
                                            <InputAdornment position="end">
                                                <IconButton
                                                    onClick={handleTogglePassword}
                                                    edge="end"
                                                    aria-label="Toggle password visibility"
                                                >
                                                    {showPassword ? (
                                                        <VisibilityOffOutlinedIcon />
                                                    ) : (
                                                        <VisibilityOutlinedIcon />
                                                    )}
                                                </IconButton>
                                            </InputAdornment>
                                        ),
                                    },
                                }}
                            />

                            <Box
                                sx={{
                                    display: 'flex',
                                    justifyContent: 'space-between',
                                    alignItems: 'center',
                                    mt: 0.5,
                                }}
                            />

                            <Button
                                type="submit"
                                variant="contained"
                                fullWidth
                                disabled={isSubmitting}
                                endIcon={
                                    !isSubmitting ? (
                                        <ArrowForwardRoundedIcon />
                                    ) : undefined
                                }
                                sx={{ mt: 1 }}
                            >
                                {isSubmitting ? 'Signing in…' : 'Sign in'}
                            </Button>

                            <Divider
                                sx={{
                                    my: 2,
                                    borderColor: 'rgba(148,163,184,0.3)',
                                }}
                            >
                                <Typography
                                    variant="caption"
                                    sx={{
                                        color: 'text.secondary',
                                        textTransform: 'uppercase',
                                    }}
                                >
                                    Or
                                </Typography>
                            </Divider>

                            <Button
                                fullWidth
                                variant="outlined"
                                color="inherit"
                                onClick={() => navigate('/register')}
                                sx={{
                                    borderRadius: 999,
                                    borderColor: 'rgba(148,163,184,0.4)',
                                    '&:hover': {
                                        borderColor: '#f5c451',
                                        backgroundColor: 'rgba(15,23,42,0.9)',
                                    },
                                }}
                            >
                                Create a new account
                            </Button>
                        </Box>
                    </Paper>
                </Box>
            </Container>
        </Box>
    );
}