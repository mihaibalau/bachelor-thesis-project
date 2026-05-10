import { BrowserRouter, Routes, Route, Navigate } from 'react-router-dom';
import { AuthProvider, useAuth } from '../src/features/auth/AuthContext';
import { LoginPage } from '../src/features/auth/LoginPage';
import { DashboardPage } from '../src/features/dashboard/DashboardPage';
import type { ReactNode } from 'react';
import {RegisterPage} from "./features/auth/RegisterPage.tsx";

function ProtectedRoute({ children }: { children: ReactNode }) {
    const { token } = useAuth();

    if (!token) {
        return <Navigate to="/login" replace />;
    }

    return children;
}

export default function App() {
    return (
        <AuthProvider>
            <BrowserRouter>
                <Routes>
                    <Route path="/login" element={<LoginPage />} />
                    <Route path="/register" element={<RegisterPage />} />
                    <Route
                        path="/app"
                        element={
                            <ProtectedRoute>
                                <DashboardPage />
                            </ProtectedRoute>
                        }
                    />
                    <Route path="*" element={<Navigate to="/login" replace />} />
                </Routes>
            </BrowserRouter>
        </AuthProvider>
    );
}