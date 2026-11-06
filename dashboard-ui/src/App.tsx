import { BrowserRouter, Navigate, Route, Routes } from "react-router-dom";
import { AppProviders } from "./state/AppProviders";
import { ToastProvider } from "./components/ui/Toast";
import { Layout } from "./components/layout/Layout";
import { OverviewPage } from "./pages/OverviewPage";
import { CameraPage } from "./pages/CameraPage";
import { DrivePage } from "./pages/DrivePage";
import { DiagnosticsPage } from "./pages/DiagnosticsPage";
import { SettingsPage } from "./pages/SettingsPage";

export default function App() {
  return (
    <AppProviders>
      <ToastProvider>
        <BrowserRouter>
          <Routes>
            <Route element={<Layout />}>
              <Route index element={<OverviewPage />} />
              <Route path="/camera" element={<CameraPage />} />
              <Route path="/drive" element={<DrivePage />} />
              <Route path="/diagnostics" element={<DiagnosticsPage />} />
              <Route path="/settings" element={<SettingsPage />} />
              {/* Legacy URLs from the pre-merge 9-page layout */}
              <Route path="/detection" element={<Navigate to="/camera" replace />} />
              <Route path="/navigation" element={<Navigate to="/drive" replace />} />
              <Route path="/automation" element={<Navigate to="/drive" replace />} />
              <Route path="/collection" element={<Navigate to="/" replace />} />
              <Route path="/help" element={<Navigate to="/settings" replace />} />
              <Route path="*" element={<OverviewPage />} />
            </Route>
          </Routes>
        </BrowserRouter>
      </ToastProvider>
    </AppProviders>
  );
}
