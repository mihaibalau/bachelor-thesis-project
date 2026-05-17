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
import ArrowForwardRoundedIcon from '@mui/icons-material/ArrowForwardRounded';
import CalendarMonthOutlinedIcon from '@mui/icons-material/CalendarMonthOutlined';
import PersonOutlineOutlinedIcon from '@mui/icons-material/PersonOutlineOutlined';
import TagOutlinedIcon from '@mui/icons-material/TagOutlined';
import PhoneIphoneOutlinedIcon from '@mui/icons-material/PhoneIphoneOutlined';
import MailOutlineOutlinedIcon from '@mui/icons-material/MailOutlineOutlined';
import DarkModeOutlinedIcon from '@mui/icons-material/DarkModeOutlined';
import LightModeOutlinedIcon from '@mui/icons-material/LightModeOutlined';
import type { MouseEvent } from 'react';
import VisibilityOffOutlinedIcon from '@mui/icons-material/VisibilityOffOutlined';
import VisibilityOutlinedIcon from '@mui/icons-material/VisibilityOutlined';
import { useNavigate } from 'react-router-dom';
import { useColorMode } from '../theme/useColorMode';
import gentlixLogo from '../../assets/logo.png';

// Zod schema matching backend payload
const RegisterSchema = z.object({
    tag: z
        .string()
        .min(3, 'Tag must be at least 3 characters')
        .max(32, 'Tag must be at most 32 characters'),
    email: z.string().email('Please enter a valid email address'),
    first_name: z.string().min(2, 'First name is required'),
    last_name: z.string().min(2, 'Last name is required'),
    phone: z
        .string()
        .min(7, 'Please enter a valid phone number')
        .max(20, 'Phone number is too long'),
    birth_date: z
        .string()
        .regex(/^\d{4}-\d{2}-\d{2}$/, 'Use format YYYY-MM-DD'),
    password: z
        .string()
        .min(8, 'Password must be at least 8 characters'),
});

type RegisterFormValues = z.infer<typeof RegisterSchema>;

export function RegisterPage() {
    const navigate = useNavigate();
    const [serverError, setServerError] = useState<string | null>(null);
    const [serverSuccess, setServerSuccess] = useState<string | null>(null);
    const [showPassword, setShowPassword] = useState(false);
    const { mode, toggleMode } = useColorMode();
    const theme = useTheme();
    const isDark = theme.palette.mode === 'dark';

    const {
        register,
        handleSubmit,
        formState: { errors, isSubmitting },
    } = useForm<RegisterFormValues>({
        resolver: zodResolver(RegisterSchema),
        mode: 'onBlur', // validate when leaving a field
        reValidateMode: 'onBlur',
        defaultValues: {
            tag: '',
            email: '',
            first_name: '',
            last_name: '',
            phone: '',
            birth_date: '',
            password: '',
        },
    });

    const onSubmit = async (data: RegisterFormValues) => {
        setServerError(null);
        setServerSuccess(null);

        try {
            const response = await fetch('http://localhost:6767/api/users', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify(data),
            });

            if (!response.ok) {
                const text = await response.text();
                throw new Error(text || 'Registration failed.');
            }

            setServerSuccess(
                'Account created successfully. You can now sign in.',
            );
            // small delay so the user sees the message, then go to login
            setTimeout(() => {
                navigate('/login');
            }, 900);
        } catch (err) {
            console.error(err);
            setServerError(
                err instanceof Error
                    ? err.message
                    : 'Registration failed. Please check your details and try again.',
            );
        }
    };

    const handleTogglePassword = (event: MouseEvent<HTMLButtonElement>) => {
        event.preventDefault();
        setShowPassword((prev) => !prev);
    };

    const registerBackground = isDark
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
                background: registerBackground,
                transition: 'background 200ms ease',
            }}
        >
            {/* Toggle sus dreapta */}
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
                    {/* Left side – brand + copy */}
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
                                account features
                            </Typography>
                            <Typography variant="body2" color="text.secondary">
                                One main account, flexible sub-accounts, and affiliate shortcuts
                                tailored to how you move your money.
                            </Typography>
                        </Box>
                    </Box>

                    {/* Right side – register card */}
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
                            Create your Gentlix account
                        </Typography>
                        <Typography
                            variant="body2"
                            sx={{ mb: 3, color: 'text.secondary' }}
                        >
                            Fill in your details. All fields are required.
                        </Typography>

                        {serverError && (
                            <Alert severity="error" sx={{ mb: 2 }}>
                                {serverError}
                            </Alert>
                        )}

                        {serverSuccess && (
                            <Alert severity="success" sx={{ mb: 2 }}>
                                {serverSuccess}
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
                                label="Tag"
                                autoComplete="off"
                                {...register('tag')}
                                error={!!errors.tag}
                                helperText={errors.tag?.message}
                                slotProps={{
                                    input: {
                                        startAdornment: (
                                            <InputAdornment position="start">
                                                <TagOutlinedIcon fontSize="small" />
                                            </InputAdornment>
                                        ),
                                    },
                                }}
                            />

                            <TextField
                                label="Email address"
                                type="email"
                                autoComplete="email"
                                {...register('email')}
                                error={!!errors.email}
                                helperText={errors.email?.message}
                                slotProps={{
                                    input: {
                                        startAdornment: (
                                            <InputAdornment position="start">
                                                <MailOutlineOutlinedIcon fontSize="small" />
                                            </InputAdornment>
                                        ),
                                    },
                                }}
                            />

                            <Box
                                sx={{
                                    display: 'grid',
                                    gridTemplateColumns: {
                                        xs: '1fr',
                                        sm: '1fr 1fr',
                                    },
                                    gap: 2,
                                }}
                            >
                                <TextField
                                    label="First name"
                                    autoComplete="given-name"
                                    {...register('first_name')}
                                    error={!!errors.first_name}
                                    helperText={errors.first_name?.message}
                                    slotProps={{
                                        input: {
                                            startAdornment: (
                                                <InputAdornment position="start">
                                                    <PersonOutlineOutlinedIcon fontSize="small" />
                                                </InputAdornment>
                                            ),
                                        },
                                    }}
                                />

                                <TextField
                                    label="Last name"
                                    autoComplete="family-name"
                                    {...register('last_name')}
                                    error={!!errors.last_name}
                                    helperText={errors.last_name?.message}
                                />
                            </Box>

                            <Box
                                sx={{
                                    display: 'grid',
                                    gridTemplateColumns: {
                                        xs: '1fr',
                                        sm: '1fr 1fr',
                                    },
                                    gap: 2,
                                }}
                            >
                                <TextField
                                    label="Phone number"
                                    placeholder="07XXXXXXXX"
                                    autoComplete="tel"
                                    {...register('phone')}
                                    error={!!errors.phone}
                                    helperText={errors.phone?.message}
                                    slotProps={{
                                        input: {
                                            startAdornment: (
                                                <InputAdornment position="start">
                                                    <PhoneIphoneOutlinedIcon fontSize="small" />
                                                </InputAdornment>
                                            ),
                                        },
                                    }}
                                />

                                <TextField
                                    label="Birth date"
                                    placeholder="2004-10-08"
                                    {...register('birth_date')}
                                    error={!!errors.birth_date}
                                    helperText={errors.birth_date?.message}
                                    slotProps={{
                                        input: {
                                            startAdornment: (
                                                <InputAdornment position="start">
                                                    <CalendarMonthOutlinedIcon fontSize="small" />
                                                </InputAdornment>
                                            ),
                                        },
                                    }}
                                />
                            </Box>

                            <TextField
                                label="Password"
                                type={showPassword ? 'text' : 'password'}
                                autoComplete="new-password"
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
                                {isSubmitting
                                    ? 'Creating account…'
                                    : 'Create account'}
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
                                    Already registered?
                                </Typography>
                            </Divider>

                            <Button
                                fullWidth
                                variant="outlined"
                                color="inherit"
                                onClick={() => navigate('/login')}
                                sx={{
                                    borderRadius: 999,
                                    borderColor: 'rgba(148,163,184,0.4)',
                                    '&:hover': {
                                        borderColor: '#f5c451',
                                        backgroundColor: 'rgba(15,23,42,0.9)',
                                    },
                                }}
                            >
                                Back to sign in
                            </Button>
                        </Box>
                    </Paper>
                </Box>
            </Container>
        </Box>
    );
}