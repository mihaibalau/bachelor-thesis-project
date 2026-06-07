import { ApiError, parseApiError } from './apiError';

const API_BASE_URL = import.meta.env.VITE_API_BASE_URL ?? 'http://localhost:6767/api';

export let authToken: string | null = null;

let unauthorizedHandler: (() => void) | null = null;

// Called by AuthContext to attach Bearer token to every request.
export function setAuthToken(token: string | null) {
    authToken = token;
}

// Called by AuthContext to handle session expiry (401).
export function setUnauthorizedHandler(handler: (() => void) | null) {
    unauthorizedHandler = handler;
}

async function request<T>(path: string, options: RequestInit = {}): Promise<T> {
    const headers = new Headers(options.headers || {});

    // 1. Attach JSON content-type and Bearer token when present.
    if (options.body && !headers.has('Content-Type')) {
        headers.set('Content-Type', 'application/json');
    }
    if (authToken) {
        headers.set('Authorization', `Bearer ${authToken}`);
    }

    const response = await fetch(`${API_BASE_URL}${path}`, {
        ...options,
        headers,
    });

    // 2. On failure, trigger session logout for 401 and throw parsed ApiError.
    if (!response.ok) {
        if (response.status === 401 && authToken && unauthorizedHandler) {
            unauthorizedHandler();
        }
        throw await parseApiError(response);
    }

    // 3. Parse JSON body; treat 204 and empty bodies as undefined.
    if (response.status === 204) {
        return undefined as T;
    }
    const text = await response.text();
    if (!text) {
        return undefined as T;
    }
    return JSON.parse(text) as T;
}

export const apiClient = {
    get: <T>(path: string) => request<T>(path),

    post: <T, B = unknown>(path: string, body?: B) =>
        request<T>(path, {
            method: 'POST',
            body: body !== undefined ? JSON.stringify(body) : undefined,
        }),

    put: <T, B = unknown>(path: string, body: B) =>
        request<T>(path, { method: 'PUT', body: JSON.stringify(body) }),

    patch: <T, B = unknown>(path: string, body: B) =>
        request<T>(path, { method: 'PATCH', body: JSON.stringify(body) }),

    delete: <T>(path: string) => request<T>(path, { method: 'DELETE' }),
};

export { ApiError };
