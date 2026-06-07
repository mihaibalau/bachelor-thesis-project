import { Outlet } from 'react-router-dom';
import { AccountsProvider } from '../../shared/context/AccountsContext';
import { ToastProvider } from '../../shared/context/ToastContext';
import { AppShell } from '../../shared/layout/AppShell';

export function AppLayout() {
    return (
        <ToastProvider>
            <AccountsProvider>
                <AppShell>
                    <Outlet />
                </AppShell>
            </AccountsProvider>
        </ToastProvider>
    );
}
