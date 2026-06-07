export type TransactionCategory = 'shopping' | 'food' | 'atm' | 'transfer' | 'salary';

export type TransactionActivityItem = {
    id: number;
    label: string;
    description: string;
    recorded_on: string;
    amount_cents: number;
    category: TransactionCategory;
    is_income: boolean;
};

type TxLike = {
    id: number;
    from_account_id: number;
    to_account_id: number;
    transaction_type: string;
    value_cents: number;
    recorded_on: string;
    description: string;
};

function parsePaymentFields(description: string): { merchant: string; category: string } {
    let category = 'shopping';
    let merchant = 'Payment';

    for (const part of description.split('|').map((p) => p.trim())) {
        if (part.startsWith('category:')) {
            category = part.slice('category:'.length).trim();
        } else if (part.startsWith('merchant:')) {
            merchant = part.slice('merchant:'.length).trim();
        }
    }

    return { merchant, category };
}

function uiCategory(
    txType: string,
    paymentCategory: string,
    description: string,
): TransactionCategory {
    switch (txType) {
        case 'Withdrawal':
            return 'atm';
        case 'Send':
        case 'Transfer':
            return 'transfer';
        case 'Deposit':
            return 'salary';
        case 'Payment': {
            const lower = paymentCategory.toLowerCase();
            if (
                lower.includes('food') ||
                lower.includes('dinner') ||
                lower.includes('restaurant') ||
                description.toLowerCase().includes('glovo')
            ) {
                return 'food';
            }
            return 'shopping';
        }
        default:
            return 'shopping';
    }
}

function extractNote(description: string): string | undefined {
    const note = description
        .split('|')
        .map((p) => p.trim())
        .find((p) => p.startsWith('note:'))
        ?.slice('note:'.length)
        .trim();
    return note && note.length > 0 ? note : undefined;
}

function mapLabelAndDetail(
    txType: string,
    description: string,
): { label: string; detail: string; category: TransactionCategory } {
    switch (txType) {
        case 'Payment': {
            const { merchant, category: paymentCategory } = parsePaymentFields(description);
            const note = extractNote(description);
            return {
                label: merchant,
                detail: note ?? paymentCategory,
                category: uiCategory(txType, paymentCategory, description),
            };
        }
        case 'Withdrawal':
            return {
                label: 'ATM Withdrawal',
                detail: 'Cash out',
                category: uiCategory(txType, '', description),
            };
        case 'Send': {
            const raw = description.startsWith('Send:')
                ? description.slice('Send:'.length).trim()
                : description;
            const recipient = raw.split('|')[0]?.trim() || 'Transfer';
            const note = extractNote(raw);
            return {
                label: `Transfer – ${recipient}`,
                detail: note ?? 'Rent share',
                category: uiCategory(txType, '', description),
            };
        }
        case 'Transfer':
            return {
                label: 'Transfer',
                detail: description,
                category: uiCategory(txType, '', description),
            };
        case 'Deposit': {
            const lower = description.toLowerCase();
            if (lower.includes('salary')) {
                return { label: 'Salary', detail: 'Gentlix Bank', category: 'salary' };
            }
            if (description.includes('Opening balance')) {
                return {
                    label: 'Balance adjustment',
                    detail: description,
                    category: 'salary',
                };
            }
            return { label: 'Deposit', detail: description, category: 'salary' };
        }
        default:
            return { label: txType, detail: description, category: 'shopping' };
    }
}

export type StatementRowLike = {
    transaction_id: number;
    recorded_on: string;
    transaction_type: string;
    description: string;
    value_cents: number;
    balance_after_cents: number;
};

const OUTGOING_TYPES = new Set(['Payment', 'Withdrawal', 'Send']);

function inferFirstEntrySigned(row: StatementRowLike): number {
    if (row.value_cents < 0) return row.value_cents;
    if (OUTGOING_TYPES.has(row.transaction_type)) return -Math.abs(row.value_cents);
    if (row.transaction_type === 'Deposit') return Math.abs(row.value_cents);
    return row.value_cents;
}

/** Derive signed amounts from running balances — correct even if API returns unsigned values. */
export function computeStatementSignedAmounts(entries: StatementRowLike[]): Map<number, number> {
    const asc = [...entries].sort(
        (a, b) =>
            a.recorded_on.localeCompare(b.recorded_on)
            || a.transaction_id - b.transaction_id,
    );
    const map = new Map<number, number>();
    for (let i = 0; i < asc.length; i++) {
        const row = asc[i];
        const signed =
            i > 0
                ? row.balance_after_cents - asc[i - 1].balance_after_cents
                : inferFirstEntrySigned(row);
        map.set(row.transaction_id, signed);
    }
    return map;
}

export function statementRangeBalances(
    entries: StatementRowLike[],
    signed: Map<number, number>,
): { opening: number | null; closing: number | null } {
    if (!entries.length) return { opening: null, closing: null };
    const asc = [...entries].sort(
        (a, b) =>
            a.recorded_on.localeCompare(b.recorded_on)
            || a.transaction_id - b.transaction_id,
    );
    const first = asc[0];
    const last = asc[asc.length - 1];
    const signedFirst = signed.get(first.transaction_id) ?? inferFirstEntrySigned(first);
    return {
        opening: first.balance_after_cents - signedFirst,
        closing: last.balance_after_cents,
    };
}

/** Statement API rows already carry signed value_cents relative to the account. */
export function mapStatementToActivity(row: {
    transaction_id: number;
    transaction_type: string;
    description: string;
    recorded_on: string;
    value_cents: number;
}): TransactionActivityItem {
    const { label, detail, category } = mapLabelAndDetail(row.transaction_type, row.description);

    return {
        id: row.transaction_id,
        label,
        description: detail,
        recorded_on: row.recorded_on,
        amount_cents: row.value_cents,
        category,
        is_income: row.value_cents > 0,
    };
}

export function mapTransactionToActivity(tx: TxLike, accountId: number): TransactionActivityItem {
    const fromOwned = tx.from_account_id === accountId;
    const toOwned = tx.to_account_id === accountId;

    let amountCents = 0;
    if (toOwned && !fromOwned) {
        amountCents = tx.value_cents;
    } else if (fromOwned) {
        amountCents = -tx.value_cents;
    }

    const { label, detail, category } = mapLabelAndDetail(tx.transaction_type, tx.description);

    return {
        id: tx.id,
        label,
        description: detail,
        recorded_on: tx.recorded_on,
        amount_cents: amountCents,
        category,
        is_income: amountCents > 0,
    };
}
