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
    id: number;
}

export function login(req: LoginRequest) {
    return apiClient.post<LoginResponse, LoginRequest>('/users/login', req);
}

export function registerUser(req: RegisterRequest) {
    return apiClient.post<RegisterResponse, RegisterRequest>('/users', req);
}