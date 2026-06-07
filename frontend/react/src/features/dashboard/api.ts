import { apiClient } from '../../shared/apiClient';
import type { DashboardData } from './types';

// Aggregated balances, counts, spending chart, and recent activity.
export async function fetchDashboard(): Promise<DashboardData> {
    return apiClient.get<DashboardData>('/dashboard');
}
