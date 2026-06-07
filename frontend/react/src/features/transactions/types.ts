export type Transaction = {
    id: number;
    from_account_id: number;
    to_account_id: number;
    transaction_type: string;
    value_cents: number;
    recorded_on: string;
    description: string;
};

export type RecentTransactionsResponse = {
    items: Transaction[];
};

export type StatementEntry = {
    transaction_id: number;
    recorded_on: string;
    description: string;
    transaction_type: string;
    value_cents: number;
    balance_after_cents: number;
};

export type StatementResponse = {
    items: StatementEntry[];
};

export type PerTypeTotal = {
    transaction_type: string;
    total_cents: number;
};

export type DailySpendingPoint = {
    date: string;
    spending_cents: number;
    cumulative_spending_cents: number;
};

export type DailyNetPoint = {
    date: string;
    net_cents: number;
};

export type PaymentCategoryTotal = {
    category: string;
    total_cents: number;
};

export type AccountBalanceSummary = {
    account_id: number;
    account_type: string;
    currency: string;
    balance_cents: number;
};

export type TransactionSummary = {
    total_incoming_cents: number;
    total_outgoing_cents: number;
    net_flow_cents: number;
    total_volume_cents: number;
    transaction_count: number;
    per_type_totals: PerTypeTotal[];
    daily_net: DailyNetPoint[];
    daily_cumulative_spending: DailySpendingPoint[];
    payment_category_totals: PaymentCategoryTotal[];
    account_balances: AccountBalanceSummary[];
};
