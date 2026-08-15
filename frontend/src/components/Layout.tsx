import { Outlet, Link, useLocation } from 'react-router-dom';
import { useAuth } from '../context/AuthContext';

const navItems: Record<string, { path: string; label: string; icon: string }[]> = {
  student: [
    { path: '/student', label: 'Dashboard', icon: '🏠' },
    { path: '/courses', label: 'Course Explorer', icon: '📚' },
  ],
  faculty: [
    { path: '/faculty', label: 'Dashboard', icon: '🏠' },
    { path: '/courses', label: 'Courses', icon: '📚' },
  ],
  admin: [
    { path: '/admin', label: 'Dashboard', icon: '🏠' },
    { path: '/courses', label: 'Courses', icon: '📚' },
  ],
};

export default function Layout() {
  const { user, logout } = useAuth();
  const location = useLocation();
  if (!user) return null;

  const items = navItems[user.role] || [];
  const roleColors: Record<string, string> = {
    admin: 'from-red-500 to-orange-500',
    faculty: 'from-emerald-500 to-teal-500',
    student: 'from-primary-500 to-accent-500',
  };

  return (
    <div className="min-h-screen flex">
      {/* Sidebar */}
      <aside className="w-72 bg-surface-900/80 backdrop-blur-xl border-r border-white/5 flex flex-col">
        <div className="p-6 border-b border-white/5">
          <div className="flex items-center gap-3">
            <div className={`w-10 h-10 rounded-xl bg-gradient-to-br ${roleColors[user.role]} flex items-center justify-center text-lg font-bold shadow-lg`}>
              {user.name.charAt(0)}
            </div>
            <div>
              <h1 className="text-lg font-bold bg-gradient-to-r from-white to-white/70 bg-clip-text text-transparent">Academia</h1>
              <p className="text-xs text-white/40">Course Registration</p>
            </div>
          </div>
        </div>

        <nav className="flex-1 p-4 space-y-1">
          {items.map((item) => {
            const isActive = location.pathname === item.path;
            return (
              <Link key={item.path} to={item.path}
                className={`flex items-center gap-3 px-4 py-3 rounded-xl text-sm font-medium transition-all duration-200 ${
                  isActive
                    ? 'bg-primary-500/20 text-primary-400 border border-primary-500/20'
                    : 'text-white/60 hover:text-white hover:bg-white/5'
                }`}>
                <span>{item.icon}</span>
                <span>{item.label}</span>
              </Link>
            );
          })}
        </nav>

        <div className="p-4 border-t border-white/5">
          <div className="glass-card p-4 mb-3">
            <p className="text-sm font-medium text-white/90">{user.name}</p>
            <p className="text-xs text-white/40">{user.email}</p>
            <span className={`badge mt-2 ${
              user.role === 'admin' ? 'bg-red-500/20 text-red-400' :
              user.role === 'faculty' ? 'bg-emerald-500/20 text-emerald-400' :
              'bg-primary-500/20 text-primary-400'
            }`}>
              {user.role.charAt(0).toUpperCase() + user.role.slice(1)}
            </span>
          </div>
          <button onClick={logout}
            className="w-full px-4 py-2.5 text-sm font-medium text-white/60 hover:text-white hover:bg-white/5 rounded-xl transition-all duration-200 flex items-center gap-2">
            <span>🚪</span> Sign Out
          </button>
        </div>
      </aside>

      {/* Main content */}
      <main className="flex-1 overflow-auto">
        <div className="p-8 max-w-7xl mx-auto">
          <Outlet />
        </div>
      </main>
    </div>
  );
}
