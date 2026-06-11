import { apiClient } from '../../shared/apiClient';
import type { UserWithAccounts } from './types';

// Profile + owned accounts (AccountsContext after login).
export function fetchUserWithAccounts(userId: number): Promise<UserWithAccounts> {
    return apiClient.get<UserWithAccounts>(`/users/${userId}`);
}
