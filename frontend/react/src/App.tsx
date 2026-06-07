import { BrowserRouter, Routes, Route, Navigate } from 'react-router-dom';
import { AuthProvider, useAuth } from './features/auth/AuthContext';
import { ProtectedRoute } from './features/auth/ProtectedRoute';
import { LoginPage } from './features/auth/LoginPage';
import { RegisterPage } from './features/auth/RegisterPage';
import { AppLayout } from './features/app/AppLayout';
import { DashboardPage } from './features/dashboard/DashboardPage';
import { AccountsPage } from './features/accounts/AccountsPage';
import { AccountDetailPage } from './features/accounts/AccountDetailPage';
import { AffiliatesPage } from './features/affiliates/AffiliatesPage';
import { TransactionsPage } from './features/transactions/TransactionsPage';
import { StatementsPage } from './features/statements/StatementsPage';
import { StatisticsPage } from './features/statistics/StatisticsPage';
import type { ReactNode } from 'react';

// Redirect authenticated users away from login/register.
function PublicOnlyRoute({ children }: { children: ReactNode }) {
    const { token, isReady } = useAuth();
    // 1. Wait until persisted auth is hydrated.
    if (!isReady) return null;
    // 2. Signed-in users go straight to the dashboard.
    if (token) return <Navigate to="/app/dashboard" replace />;
    return children;
}

export default function App() {
    // Public routes (login/register) and protected /app/* layout nested under AuthProvider.
    return (
        <AuthProvider>
            <BrowserRouter>
                <Routes>
                    <Route path="/login" element={<PublicOnlyRoute><LoginPage /></PublicOnlyRoute>} />
                    <Route path="/register" element={<PublicOnlyRoute><RegisterPage /></PublicOnlyRoute>} />
                    <Route
                        path="/app"
                        element={
                            <ProtectedRoute>
                                <AppLayout />
                            </ProtectedRoute>
                        }
                    >
                        <Route index element={<Navigate to="dashboard" replace />} />
                        <Route path="dashboard" element={<DashboardPage />} />
                        <Route path="accounts" element={<AccountsPage />} />
                        <Route path="accounts/:id" element={<AccountDetailPage />} />
                        <Route path="affiliates" element={<AffiliatesPage />} />
                        <Route path="transactions" element={<TransactionsPage />} />
                        <Route path="statements" element={<StatementsPage />} />
                        <Route path="statistics" element={<StatisticsPage />} />
                    </Route>
                    <Route path="*" element={<Navigate to="/login" replace />} />
                </Routes>
            </BrowserRouter>
        </AuthProvider>
    );
}
