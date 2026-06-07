export type Affiliate = {
    recipient_sub_account_id: number;
    nickname: string;
    recipient_full_name: string;
    currency: string;
};

export type PaginatedAffiliates = {
    items: Affiliate[];
    page: number;
    page_size: number;
    total: number;
};

export type ResolveTargetRequest = {
    identifier_type: 'tag';
    identifier: string;
};

export type ResolveTargetCurrency = {
    currency: string;
    recipient_sub_account_id: number;
};

export type ResolvedTarget = {
    recipient_user_id: number;
    recipient_full_name: string;
    currencies: ResolveTargetCurrency[];
};

export type CreateAffiliateRequest = {
    recipient_sub_account_id: number;
    nickname: string;
};
