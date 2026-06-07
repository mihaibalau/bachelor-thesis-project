type WelcomeCopy = {
    heading: string;
    subtitle: (firstName: string) => string;
};

const WELCOME_VARIANTS: WelcomeCopy[] = [
    {
        heading: 'Welcome back',
        subtitle: (name) => `Good to see you again, ${name}.`,
    },
    {
        heading: 'Welcome back',
        subtitle: (name) => `Here's what's happening with your money today, ${name}.`,
    },
    {
        heading: 'Hello again',
        subtitle: (name) => `Your Gentlix overview is ready, ${name}.`,
    },
    {
        heading: 'Welcome back',
        subtitle: (name) => `Pick up where you left off, ${name}.`,
    },
    {
        heading: 'Good to see you',
        subtitle: (name) => `Accounts, spending, and activity — all in one place, ${name}.`,
    },
    {
        heading: 'Welcome back',
        subtitle: (name) => `A quick snapshot of your finances, ${name}.`,
    },
];

export function pickWelcomeMessage(): WelcomeCopy {
    const index = Math.floor(Math.random() * WELCOME_VARIANTS.length);
    return WELCOME_VARIANTS[index];
}
