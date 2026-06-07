import { apiClient } from '../../shared/apiClient';

export interface LoginRequest {
    email: string;
    password: string;
}

export interface LoginResponse {
    token: string;
    user_id: number;
}

export interface RegisterRequest {
    tag: string;
    email: string;
    first_name: string;
    last_name: string;
    phone: string;
    birth_date: string;
    password: string;
}

export interface RegisterResponse {
    user_id: number;
}

// Exchange credentials for JWT and user id.
export function login(req: LoginRequest) {
    return apiClient.post<LoginResponse, LoginRequest>('/users/login', req);
}

// Create a new user account (does not sign in).
export function registerUser(req: RegisterRequest) {
    return apiClient.post<RegisterResponse, RegisterRequest>('/users', req);
}