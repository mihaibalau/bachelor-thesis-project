import type { TransactionCategory } from '../../shared/transactionDisplay';

export type { TransactionCategory };

export type DashboardActivityItem = {
    id: number;
    label: string;
    description: string;
    recorded_on: string;
    amount_cents: number;
    category: TransactionCategory;
    is_income: boolean;
};

export type DashboardDailySpendingPoint = {
    day_label: string;
    date: string;
    cumulative_spending_cents: number;
};

export type DashboardSpending = {
    total_spent_cents: number;
    change_percent_vs_last_month: number;
    daily_cumulative: DashboardDailySpendingPoint[];
};

export type DashboardData = {
    total_balance_cents: number;
    balance_change_percent: number;
    active_accounts_count: number;
    affiliates_count: number;
    transfers_total_cents: number;
    recent_activity: DashboardActivityItem[];
    spending: DashboardSpending;
};
