import { apiClient } from '../../shared/apiClient';
import type { Account, AccountAvailabilityResponse, OpenAccountRequest } from './types';

// Types and currencies the user has not opened yet.
export function fetchAccountAvailability(): Promise<AccountAvailabilityResponse> {
    return apiClient.get<AccountAvailabilityResponse>('/accounts/availability');
}

// Single account by id (detail page).
export function fetchAccount(id: number): Promise<Account> {
    return apiClient.get<Account>(`/accounts/${id}`);
}

// Create a new account for the authenticated user.
export function openAccount(req: OpenAccountRequest): Promise<Account> {
    return apiClient.post<Account, OpenAccountRequest>('/accounts', req);
}
