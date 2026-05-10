const API_BASE_URL = 'http://localhost:6767/api';

export let authToken: string | null = null;

export function setAuthToken(token: string | null) {
    authToken = token;
}

async function request<T>(path: string, options: RequestInit = {}): Promise<T> {
    const headers = new Headers(options.headers || {});

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

    if (!response.ok) {
        const text = await response.text().catch(() => '');
        throw new Error(text || `Request failed with status ${response.status}`);
    }

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

    post: <T, B = unknown>(path: string, body: B) =>
        request<T>(path, {
            method: 'POST',
            body: JSON.stringify(body),
        }),

    put: <T, B = unknown>(path: string, body: B) =>
        request<T>(path, {
            method: 'PUT',
            body: JSON.stringify(body),
        }),

    patch: <T, B = unknown>(path: string, body: B) =>
        request<T>(path, {
            method: 'PATCH',
            body: JSON.stringify(body),
        }),

    delete: <T>(path: string) =>
        request<T>(path, {
            method: 'DELETE',
        }),
};