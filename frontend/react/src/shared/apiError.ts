export type ApiErrorBody = {
    status?: number;
    code?: string;
    message?: string;
};

export class ApiError extends Error {
    readonly status: number;
    readonly code: string;

    constructor(status: number, code: string, message: string) {
        super(message);
        this.name = 'ApiError';
        this.status = status;
        this.code = code;
    }

    get title(): string {
        switch (this.code) {
            case 'validation_error':
                return 'Invalid input';
            case 'forbidden':
                return 'Access denied';
            case 'not_found':
                return 'Not found';
            case 'conflict':
                return 'Conflict';
            case 'unauthorized':
                return 'Session expired';
            default:
                return 'Something went wrong';
        }
    }
}

export async function parseApiError(response: Response): Promise<ApiError> {
    const text = await response.text().catch(() => '');

    // 1. Prefer structured { code, message } from the backend.
    if (text) {
        try {
            const body = JSON.parse(text) as ApiErrorBody;
            if (body.message) {
                return new ApiError(
                    body.status ?? response.status,
                    body.code ?? 'unknown_error',
                    body.message,
                );
            }
        } catch {
            // fall through to generic messages
        }
    }

    // 2. Fall back to status-specific or raw text messages.
    if (response.status === 401) {
        return new ApiError(401, 'unauthorized', 'Your session has expired. Please sign in again.');
    }
    return new ApiError(
        response.status,
        'unknown_error',
        text || `Request failed with status ${response.status}`,
    );
}
