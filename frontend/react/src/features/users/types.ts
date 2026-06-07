import type { Account } from '../accounts/types';

export type { Account };

export type User = {
    id: number;
    tag: string;
    email: string;
    first_name: string;
    last_name: string;
    phone: string | null;
    birth_date: string | null;
};

export type UserWithAccounts = {
    user: User;
    accounts: Account[];
};
