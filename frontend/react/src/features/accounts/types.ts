export type Account = {
    id: number;
    account_type: string;
    currency: string;
    balance_cents: number;
    iban: string;
};

export type CurrencyAvailability = {
    currency: string;
    available: boolean;
};

export type AccountTypeAvailability = {
    account_type: string;
    has_any_available: boolean;
    currencies: CurrencyAvailability[];
};

export type AccountAvailabilityResponse = {
    types: AccountTypeAvailability[];
};

export type OpenAccountRequest = {
    account_type: string;
    currency: string;
    initial_balance_cents: number;
};
