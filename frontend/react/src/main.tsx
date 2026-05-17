import React from 'react';
import ReactDOM from 'react-dom/client';
import { AppThemeProvider } from './features/theme/AppThemeProvider';
import App from './App';

ReactDOM.createRoot(document.getElementById('root')!).render(
    <React.StrictMode>
        <AppThemeProvider>
            <App />
        </AppThemeProvider>
    </React.StrictMode>,
);