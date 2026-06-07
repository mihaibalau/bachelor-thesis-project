const ronFormatter = new Intl.NumberFormat('en-GB', {
    minimumFractionDigits: 2,
    maximumFractionDigits: 2,
});

const ronWholeFormatter = new Intl.NumberFormat('en-GB', {
    minimumFractionDigits: 0,
    maximumFractionDigits: 0,
});

export function centsToUnits(cents: number): number {
    return cents / 100;
}

export function unitsToCents(units: number): number {
    return Math.round(units * 100);
}

/** Whole currency units for deposit/withdrawal/payment APIs (amount × 100 → cents). */
export function parseWholeUnits(value: string | number): number {
    const n = typeof value === 'string' ? Number(value.trim()) : value;
    if (!Number.isFinite(n) || n <= 0) {
        throw new Error('Enter a positive amount');
    }
    const whole = Math.round(n);
    if (whole < 1) {
        throw new Error('Amount must be at least 1 whole unit');
    }
    return whole;
}

export function formatRon(cents: number, showDecimals = true): string {
    const units = centsToUnits(cents);
    const formatted = showDecimals
        ? ronFormatter.format(units)
        : ronWholeFormatter.format(units);
    return `RON ${formatted}`;
}

export function formatSignedRon(cents: number): string {
    const sign = cents >= 0 ? '+' : '-';
    return `${sign}${formatRon(Math.abs(cents))}`;
}

export function formatPercent(value: number, digits = 1): string {
    const sign = value > 0 ? '+' : '';
    return `${sign}${value.toFixed(digits)}%`;
}

export function formatDateTime(iso: string): string {
    const dt = new Date(iso);
    return dt.toLocaleString('en-GB', {
        weekday: 'short',
        day: 'numeric',
        month: 'short',
        hour: '2-digit',
        minute: '2-digit',
        hour12: false,
    });
}

export function formatActivityTimestamp(iso: string): { date: string; time: string } {
    const dt = new Date(iso);
    return {
        date: dt.toLocaleDateString('en-GB', {
            weekday: 'short',
            day: 'numeric',
            month: 'short',
        }),
        time: dt.toLocaleTimeString('en-GB', {
            hour: '2-digit',
            minute: '2-digit',
            hour12: false,
        }),
    };
}

export function formatChartDayLabel(dateIso: string): string {
    const d = new Date(`${dateIso}T12:00:00`);
    const weekday = d.toLocaleDateString('en-GB', { weekday: 'short' });
    return `${weekday} ${d.getDate()}`;
}

export function todayIso(): string {
    return new Date().toISOString().slice(0, 10);
}

export type AccountLabelFormat = 'full' | 'compact' | 'type-only';

export function formatAccountLabel(
    account: { account_type: string; currency: string; balance_cents?: number; iban?: string },
    format: AccountLabelFormat = 'compact',
): string {
    if (format === 'type-only') return account.account_type;
    if (format === 'compact') return `${account.account_type} · ${account.currency}`;
    const parts = [account.account_type, account.currency];
    if (account.balance_cents !== undefined) parts.push(formatRon(account.balance_cents));
    if (account.iban) parts.push(`…${account.iban.slice(-4)}`);
    return parts.join(' · ');
}

export function monthStartIso(): string {
    const d = new Date();
    return `${d.getFullYear()}-${String(d.getMonth() + 1).padStart(2, '0')}-01`;
}
