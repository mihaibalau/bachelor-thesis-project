export const DASHBOARD_REFRESH_EVENT = 'gentlix:dashboard-refresh';

export function requestDashboardRefresh(): void {
    window.dispatchEvent(new Event(DASHBOARD_REFRESH_EVENT));
}
