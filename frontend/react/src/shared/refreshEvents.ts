export const DASHBOARD_REFRESH_EVENT = 'gentlix:dashboard-refresh';

// Dispatch after a transaction so dashboard hooks refetch without a full navigation.
export function requestDashboardRefresh(): void {
    window.dispatchEvent(new Event(DASHBOARD_REFRESH_EVENT));
}
