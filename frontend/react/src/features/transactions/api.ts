import { apiClient } from '../../shared/apiClient';
import type {
    RecentTransactionsResponse,
    StatementResponse,
    TransactionSummary,
} from './types';

// Paginated transaction history for one account.
export function fetchRecentTransactions(
    accountId: number,
    limit = 20,
    offset = 0,
): Promise<RecentTransactionsResponse> {
    const q = new URLSearchParams({
        account_id: String(accountId),
        limit: String(limit),
        offset: String(offset),
    });
    return apiClient.get<RecentTransactionsResponse>(`/transactions/recent?${q}`);
}

// Aggregated metrics and chart series for statistics page.
export function fetchTransactionSummary(params: {
    from?: string;
    to?: string;
    account_id?: number;
    transaction_type?: string;
    per_account_limit?: number;
}): Promise<TransactionSummary> {
    const q = new URLSearchParams();
    if (params.from) q.set('from', params.from);
    if (params.to) q.set('to', params.to);
    if (params.account_id) q.set('account_id', String(params.account_id));
    if (params.transaction_type && params.transaction_type !== 'All') {
        q.set('transaction_type', params.transaction_type);
    }
    q.set('per_account_limit', String(params.per_account_limit ?? 500));
    return apiClient.get<TransactionSummary>(`/transactions/summary?${q}`);
}

// Statement rows with running balance for a date range.
export function fetchStatement(params: {
    account_id: number;
    from?: string;
    to?: string;
    limit?: number;
    offset?: number;
}): Promise<StatementResponse> {
    const q = new URLSearchParams({ account_id: String(params.account_id) });
    if (params.from) q.set('from', params.from);
    if (params.to) q.set('to', params.to);
    if (params.limit) q.set('limit', String(params.limit));
    if (params.offset) q.set('offset', String(params.offset));
    return apiClient.get<StatementResponse>(`/transactions/statement?${q}`);
}

// ATM deposit in whole currency units.
export function recordDeposit(accountId: number, amount: number): Promise<number> {
    return apiClient.post<number, { account_id: number; amount: number }>('/transactions/deposit', {
        account_id: accountId,
        amount,
    });
}

// ATM withdrawal in whole currency units.
export function recordWithdrawal(accountId: number, amount: number): Promise<number> {
    return apiClient.post<number, { account_id: number; amount: number }>('/transactions/withdrawal', {
        account_id: accountId,
        amount,
    });
}

// Internal transfer between own accounts (value in cents).
export function recordTransfer(fromAccountId: number, toAccountId: number, valueCents: number): Promise<number> {
    return apiClient.post<number, { from_account_id: number; to_account_id: number; value_cents: number }>(
        '/transactions/transfer',
        { from_account_id: fromAccountId, to_account_id: toAccountId, value_cents: valueCents },
    );
}

// Outbound transfer to a saved affiliate (value in cents).
export function recordSend(
    fromAccountId: number,
    recipientAccountId: number,
    valueCents: number,
    message: string,
): Promise<number> {
    return apiClient.post<number, {
        from_account_id: number;
        recipient_account_id: number;
        value_cents: number;
        message: string;
    }>('/transactions/send', {
        from_account_id: fromAccountId,
        recipient_account_id: recipientAccountId,
        value_cents: valueCents,
        message,
    });
}

// Card payment with category and merchant metadata.
export function recordPayment(
    fromAccountId: number,
    amount: number,
    category: string,
    merchantName: string,
    note?: string,
): Promise<number> {
    return apiClient.post<number, {
        from_account_id: number;
        amount: number;
        category: string;
        merchant_name: string;
        note?: string | null;
    }>('/transactions/payment', {
        from_account_id: fromAccountId,
        amount,
        category,
        merchant_name: merchantName,
        note: note ?? null,
    });
}
