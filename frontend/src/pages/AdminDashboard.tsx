import { useEffect, useState } from 'react';
import { useAuth } from '../context/AuthContext';
import { adminApi } from '../services/api';
import { User, Statistics } from '../types';
import { BarChart, Bar, XAxis, YAxis, Tooltip, ResponsiveContainer, PieChart, Pie, Cell } from 'recharts';

const COLORS = ['#3b82f6', '#d946ef', '#10b981', '#f59e0b', '#ef4444', '#8b5cf6', '#06b6d4', '#f97316'];

export default function AdminDashboard() {
  const { user } = useAuth();
  const [users, setUsers] = useState<User[]>([]);
  const [stats, setStats] = useState<Statistics | null>(null);
  const [loading, setLoading] = useState(true);
  const [showUserForm, setShowUserForm] = useState(false);
  const [userForm, setUserForm] = useState({ username: '', name: '', email: '', password: '', role: 'student' as string });
  const [message, setMessage] = useState<{ type: 'success' | 'error'; text: string } | null>(null);
  const [activeTab, setActiveTab] = useState<'overview' | 'users' | 'analytics'>('overview');

  const fetchData = async () => {
    try {
      const [usersRes, statsRes] = await Promise.all([adminApi.getUsers(), adminApi.getStatistics()]);
      setUsers(usersRes.data.data || []);
      setStats(statsRes.data.data || null);
    } catch (err) { console.error(err); }
    finally { setLoading(false); }
  };

  useEffect(() => { fetchData(); }, []);

  const handleCreateUser = async (e: React.FormEvent) => {
    e.preventDefault();
    try {
      await adminApi.createUser(userForm);
      setMessage({ type: 'success', text: 'User created successfully!' });
      setShowUserForm(false);
      setUserForm({ username: '', name: '', email: '', password: '', role: 'student' });
      fetchData();
    } catch (err: any) {
      setMessage({ type: 'error', text: err.response?.data?.error || 'Failed to create user' });
    }
  };

  const handleDeleteUser = async (id: number) => {
    if (!confirm('Delete this user?')) return;
    try {
      await adminApi.deleteUser(id);
      setMessage({ type: 'success', text: 'User deleted' });
      fetchData();
    } catch (err: any) {
      setMessage({ type: 'error', text: err.response?.data?.error || 'Failed to delete user' });
    }
  };

  const handleToggleActive = async (u: User) => {
    try {
      await adminApi.updateUser(u.id, { active: !u.active });
      fetchData();
    } catch (err: any) {
      setMessage({ type: 'error', text: err.response?.data?.error || 'Failed to update user' });
    }
  };

  if (loading) return <div className="flex items-center justify-center h-64"><div className="animate-spin w-8 h-8 border-2 border-primary-400 border-t-transparent rounded-full" /></div>;

  const rolePieData = stats ? [
    { name: 'Students', value: stats.total_students },
    { name: 'Faculty', value: stats.total_faculty },
    { name: 'Admins', value: stats.total_admins },
  ] : [];

  return (
    <div className="space-y-8 animate-fade-in">
      <div className="flex items-center justify-between">
        <div>
          <h1 className="text-3xl font-bold">Admin Dashboard ⚙️</h1>
          <p className="text-white/40 mt-1">System overview and management</p>
        </div>
        <button onClick={() => setShowUserForm(!showUserForm)} className="btn-primary">
          {showUserForm ? 'Cancel' : '+ New User'}
        </button>
      </div>

      {message && (
        <div className={`p-4 rounded-xl border animate-fade-in ${message.type === 'success' ? 'bg-emerald-500/10 border-emerald-500/20 text-emerald-400' : 'bg-red-500/10 border-red-500/20 text-red-400'}`}>
          {message.text}
        </div>
      )}

      {/* Tabs */}
      <div className="flex gap-2">
        {(['overview', 'users', 'analytics'] as const).map(tab => (
          <button key={tab} onClick={() => setActiveTab(tab)}
            className={`px-5 py-2.5 rounded-xl text-sm font-medium transition-all ${
              activeTab === tab ? 'bg-primary-500/20 text-primary-400 border border-primary-500/30' : 'bg-white/5 text-white/40 hover:text-white hover:bg-white/10'
            }`}>
            {tab.charAt(0).toUpperCase() + tab.slice(1)}
          </button>
        ))}
      </div>

      {/* New User Form */}
      {showUserForm && (
        <div className="glass-card p-6 animate-fade-in">
          <h3 className="text-lg font-semibold mb-4">Create New User</h3>
          <form onSubmit={handleCreateUser} className="grid grid-cols-1 md:grid-cols-2 gap-4">
            <input value={userForm.username} onChange={e => setUserForm({...userForm, username: e.target.value})}
              className="input-field" placeholder="Username" required />
            <input value={userForm.name} onChange={e => setUserForm({...userForm, name: e.target.value})}
              className="input-field" placeholder="Full Name" required />
            <input value={userForm.email} onChange={e => setUserForm({...userForm, email: e.target.value})}
              className="input-field" placeholder="Email" type="email" />
            <input value={userForm.password} onChange={e => setUserForm({...userForm, password: e.target.value})}
              className="input-field" placeholder="Password" type="password" required />
            <select value={userForm.role} onChange={e => setUserForm({...userForm, role: e.target.value})}
              className="input-field">
              <option value="student">Student</option>
              <option value="faculty">Faculty</option>
              <option value="admin">Admin</option>
            </select>
            <button type="submit" className="btn-primary">Create User</button>
          </form>
        </div>
      )}

      {/* Overview Tab */}
      {activeTab === 'overview' && stats && (
        <div className="space-y-6">
          <div className="grid grid-cols-2 md:grid-cols-3 lg:grid-cols-6 gap-4">
            <div className="stat-card"><span className="text-white/40 text-xs">Total Users</span><span className="text-2xl font-bold text-primary-400">{stats.total_users}</span></div>
            <div className="stat-card"><span className="text-white/40 text-xs">Students</span><span className="text-2xl font-bold text-accent-400">{stats.total_students}</span></div>
            <div className="stat-card"><span className="text-white/40 text-xs">Faculty</span><span className="text-2xl font-bold text-emerald-400">{stats.total_faculty}</span></div>
            <div className="stat-card"><span className="text-white/40 text-xs">Courses</span><span className="text-2xl font-bold text-amber-400">{stats.total_courses}</span></div>
            <div className="stat-card"><span className="text-white/40 text-xs">Registrations</span><span className="text-2xl font-bold text-cyan-400">{stats.total_registrations}</span></div>
            <div className="stat-card"><span className="text-white/40 text-xs">Utilization</span><span className="text-2xl font-bold text-rose-400">{Math.round(stats.overall_utilization)}%</span></div>
          </div>

          <div className="grid grid-cols-1 lg:grid-cols-2 gap-6">
            {/* Course Utilization Bar Chart */}
            <div className="glass-card p-6">
              <h3 className="text-lg font-semibold mb-4">Course Utilization</h3>
              <ResponsiveContainer width="100%" height={300}>
                <BarChart data={stats.course_utilization}>
                  <XAxis dataKey="course_code" stroke="#ffffff40" fontSize={12} />
                  <YAxis stroke="#ffffff40" fontSize={12} />
                  <Tooltip contentStyle={{ background: '#1e293b', border: '1px solid #ffffff20', borderRadius: '12px', color: 'white' }} />
                  <Bar dataKey="enrolled" fill="#3b82f6" radius={[4, 4, 0, 0]} name="Enrolled" />
                  <Bar dataKey="capacity" fill="#ffffff15" radius={[4, 4, 0, 0]} name="Capacity" />
                </BarChart>
              </ResponsiveContainer>
            </div>

            {/* Role Distribution Pie */}
            <div className="glass-card p-6">
              <h3 className="text-lg font-semibold mb-4">User Distribution</h3>
              <ResponsiveContainer width="100%" height={300}>
                <PieChart>
                  <Pie data={rolePieData} cx="50%" cy="50%" innerRadius={60} outerRadius={100}
                    dataKey="value" label={({ name, value }) => `${name}: ${value}`}>
                    {rolePieData.map((_, idx) => (
                      <Cell key={idx} fill={COLORS[idx % COLORS.length]} />
                    ))}
                  </Pie>
                  <Tooltip contentStyle={{ background: '#1e293b', border: '1px solid #ffffff20', borderRadius: '12px', color: 'white' }} />
                </PieChart>
              </ResponsiveContainer>
            </div>
          </div>
        </div>
      )}

      {/* Users Tab */}
      {activeTab === 'users' && (
        <div className="glass-card overflow-hidden">
          <table className="w-full">
            <thead>
              <tr className="border-b border-white/10">
                <th className="text-left p-4 text-sm font-medium text-white/40">ID</th>
                <th className="text-left p-4 text-sm font-medium text-white/40">Username</th>
                <th className="text-left p-4 text-sm font-medium text-white/40">Name</th>
                <th className="text-left p-4 text-sm font-medium text-white/40">Role</th>
                <th className="text-center p-4 text-sm font-medium text-white/40">Status</th>
                <th className="text-right p-4 text-sm font-medium text-white/40">Actions</th>
              </tr>
            </thead>
            <tbody>
              {users.map(u => (
                <tr key={u.id} className="border-b border-white/5 hover:bg-white/5 transition-colors">
                  <td className="p-4 text-white/40">{u.id}</td>
                  <td className="p-4 font-mono text-sm">{u.username}</td>
                  <td className="p-4">{u.name}</td>
                  <td className="p-4">
                    <span className={`badge ${u.role === 'admin' ? 'bg-red-500/20 text-red-400' : u.role === 'faculty' ? 'bg-emerald-500/20 text-emerald-400' : 'bg-primary-500/20 text-primary-400'}`}>
                      {u.role}
                    </span>
                  </td>
                  <td className="p-4 text-center">
                    <button onClick={() => handleToggleActive(u)}
                      className={u.active ? 'badge-success cursor-pointer' : 'badge-danger cursor-pointer'}>
                      {u.active ? 'Active' : 'Blocked'}
                    </button>
                  </td>
                  <td className="p-4 text-right">
                    {u.id !== user?.id && (
                      <button onClick={() => handleDeleteUser(u.id)}
                        className="text-red-400/60 hover:text-red-400 text-sm transition-colors">
                        Delete
                      </button>
                    )}
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      )}

      {/* Analytics Tab */}
      {activeTab === 'analytics' && stats && (
        <div className="space-y-6">
          <div className="glass-card p-6">
            <h3 className="text-lg font-semibold mb-4">Course Capacity Analysis</h3>
            <div className="space-y-4">
              {stats.course_utilization.map(course => {
                const percent = Math.round(course.utilization);
                return (
                  <div key={course.course_code} className="flex items-center gap-4">
                    <span className="w-20 text-sm font-mono text-white/60">{course.course_code}</span>
                    <div className="flex-1">
                      <div className="capacity-bar h-3">
                        <div className={`capacity-fill ${percent >= 90 ? 'bg-gradient-to-r from-red-500 to-red-400' : percent >= 70 ? 'bg-gradient-to-r from-amber-500 to-amber-400' : 'bg-gradient-to-r from-emerald-500 to-emerald-400'}`}
                          style={{ width: `${percent}%` }} />
                      </div>
                    </div>
                    <span className="w-24 text-right text-sm text-white/60">{course.enrolled}/{course.capacity} ({percent}%)</span>
                  </div>
                );
              })}
            </div>
          </div>
        </div>
      )}
    </div>
  );
}
