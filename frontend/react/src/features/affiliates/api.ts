import { apiClient } from '../../shared/apiClient';
import type {
    Affiliate,
    CreateAffiliateRequest,
    PaginatedAffiliates,
    ResolvedTarget,
    ResolveTargetRequest,
} from './types';

export type ListAffiliatesParams = {
    page?: number;
    page_size?: number;
    search?: string;
    currency?: string;
    /** Resolve each affiliate to the recipient's Regular account in this currency (for send). */
    for_send_currency?: string;
    sort?: 'asc' | 'desc';
};

// Paginated affiliate list with optional search and send-currency filter.
export function listAffiliates(params: ListAffiliatesParams = {}): Promise<PaginatedAffiliates> {
    const q = new URLSearchParams();
    if (params.page) q.set('page', String(params.page));
    if (params.page_size) q.set('page_size', String(params.page_size));
    if (params.search) q.set('search', params.search);
    if (params.currency) q.set('currency', params.currency);
    if (params.for_send_currency) q.set('for_send_currency', params.for_send_currency);
    if (params.sort) q.set('sort', params.sort);
    const qs = q.toString();
    return apiClient.get<PaginatedAffiliates>(`/affiliates${qs ? `?${qs}` : ''}`);
}

// One saved affiliate by recipient sub-account id.
export function getAffiliate(subAccountId: number): Promise<Affiliate> {
    return apiClient.get<Affiliate>(`/affiliates/${subAccountId}`);
}

// Resolve a user tag to recipient name and currency accounts.
export function resolveTarget(req: ResolveTargetRequest): Promise<ResolvedTarget> {
    return apiClient.post<ResolvedTarget, ResolveTargetRequest>('/affiliates/resolve-target', req);
}

// Save nickname for an existing recipient sub-account.
export function createAffiliate(req: CreateAffiliateRequest): Promise<void> {
    return apiClient.post<void, CreateAffiliateRequest>('/affiliates', req);
}

// Rename saved affiliate nickname.
export function updateAffiliateNickname(subAccountId: number, nickname: string): Promise<void> {
    return apiClient.patch<void, { nickname: string }>(`/affiliates/${subAccountId}`, { nickname });
}

// Remove affiliate link.
export function deleteAffiliate(subAccountId: number): Promise<void> {
    return apiClient.delete<void>(`/affiliates/${subAccountId}`);
}
