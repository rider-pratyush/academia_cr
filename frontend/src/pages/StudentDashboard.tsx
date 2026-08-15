import { useEffect, useState } from 'react';
import { useAuth } from '../context/AuthContext';
import { registrationApi, courseApi } from '../services/api';
import { Course, Registration } from '../types';

export default function StudentDashboard() {
  const { user } = useAuth();
  const [enrolledCourses, setEnrolledCourses] = useState<Registration[]>([]);
  const [availableCourses, setAvailableCourses] = useState<Course[]>([]);
  const [totalCredits, setTotalCredits] = useState(0);
  const [loading, setLoading] = useState(true);
  const [actionLoading, setActionLoading] = useState<number | null>(null);
  const [message, setMessage] = useState<{ type: 'success' | 'error'; text: string } | null>(null);

  const fetchData = async () => {
    if (!user) return;
    try {
      const [enrolledRes, coursesRes] = await Promise.all([
        registrationApi.getStudentCourses(user.id),
        courseApi.getAll(),
      ]);
      const enrolled = enrolledRes.data.data.registrations || [];
      setEnrolledCourses(enrolled);
      setTotalCredits(enrolledRes.data.data.total_credits || 0);
      setAvailableCourses(coursesRes.data.data || []);
    } catch (err) {
      console.error(err);
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => { fetchData(); }, [user]);

  const handleRegister = async (courseId: number) => {
    setActionLoading(courseId);
    setMessage(null);
    try {
      await registrationApi.register(courseId);
      setMessage({ type: 'success', text: 'Successfully registered!' });
      fetchData();
    } catch (err: any) {
      setMessage({ type: 'error', text: err.response?.data?.error || 'Registration failed' });
    } finally {
      setActionLoading(null);
    }
  };

  const handleDrop = async (courseId: number) => {
    setActionLoading(courseId);
    setMessage(null);
    try {
      await registrationApi.drop(courseId);
      setMessage({ type: 'success', text: 'Course dropped successfully' });
      fetchData();
    } catch (err: any) {
      setMessage({ type: 'error', text: err.response?.data?.error || 'Drop failed' });
    } finally {
      setActionLoading(null);
    }
  };

  const enrolledCourseIds = new Set(enrolledCourses.map((r) => r.course_id));

  if (loading) return <div className="flex items-center justify-center h-64"><div className="animate-spin w-8 h-8 border-2 border-primary-400 border-t-transparent rounded-full" /></div>;

  return (
    <div className="space-y-8 animate-fade-in">
      <div>
        <h1 className="text-3xl font-bold">Welcome back, {user?.name?.split(' ')[0]} 👋</h1>
        <p className="text-white/40 mt-1">Here's your academic overview</p>
      </div>

      {message && (
        <div className={`p-4 rounded-xl border animate-fade-in ${
          message.type === 'success' ? 'bg-emerald-500/10 border-emerald-500/20 text-emerald-400' : 'bg-red-500/10 border-red-500/20 text-red-400'
        }`}>{message.text}</div>
      )}

      {/* Stats */}
      <div className="grid grid-cols-1 md:grid-cols-4 gap-4">
        <div className="stat-card">
          <span className="text-white/40 text-sm">Enrolled Courses</span>
          <span className="text-3xl font-bold text-primary-400">{enrolledCourses.length}</span>
        </div>
        <div className="stat-card">
          <span className="text-white/40 text-sm">Total Credits</span>
          <span className="text-3xl font-bold text-accent-400">{totalCredits}</span>
        </div>
        <div className="stat-card">
          <span className="text-white/40 text-sm">Available Courses</span>
          <span className="text-3xl font-bold text-emerald-400">{availableCourses.length}</span>
        </div>
        <div className="stat-card">
          <span className="text-white/40 text-sm">Status</span>
          <span className="text-xl font-bold text-emerald-400">Active ✓</span>
        </div>
      </div>

      {/* Enrolled Courses */}
      <div>
        <h2 className="text-xl font-semibold mb-4">My Courses</h2>
        {enrolledCourses.length === 0 ? (
          <div className="glass-card p-8 text-center text-white/40">
            <p className="text-4xl mb-2">📚</p>
            <p>No courses enrolled yet. Browse the available courses below!</p>
          </div>
        ) : (
          <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-4">
            {enrolledCourses.map((reg) => (
              <div key={reg.id} className="glass-card-hover p-5">
                <div className="flex items-start justify-between mb-3">
                  <span className="badge-info">{reg.course_code}</span>
                  <span className="badge-success">Enrolled</span>
                </div>
                <h3 className="font-semibold text-lg mb-1">{reg.course_name}</h3>
                <p className="text-white/40 text-sm mb-4">Registered: {new Date(reg.registered_at).toLocaleDateString()}</p>
                <button onClick={() => handleDrop(reg.course_id)}
                  disabled={actionLoading === reg.course_id}
                  className="btn-danger text-sm px-4 py-2 w-full">
                  {actionLoading === reg.course_id ? 'Dropping...' : 'Drop Course'}
                </button>
              </div>
            ))}
          </div>
        )}
      </div>

      {/* Available Courses */}
      <div>
        <h2 className="text-xl font-semibold mb-4">Available Courses</h2>
        <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-4">
          {availableCourses.map((course) => {
            const isEnrolled = enrolledCourseIds.has(course.id);
            const isFull = course.available_seats <= 0;
            const fillPercent = course.capacity > 0 ? (course.enrolled_count / course.capacity) * 100 : 0;
            return (
              <div key={course.id} className="glass-card-hover p-5">
                <div className="flex items-start justify-between mb-3">
                  <span className="badge-info">{course.course_code}</span>
                  <span className="text-xs text-white/40">{course.credits} credits</span>
                </div>
                <h3 className="font-semibold text-lg mb-1">{course.course_name}</h3>
                <p className="text-white/40 text-sm mb-1">👨‍🏫 {course.faculty_name || 'TBA'}</p>
                <p className="text-white/40 text-sm mb-3">📅 {course.schedule || 'TBA'}</p>
                
                <div className="mb-3">
                  <div className="flex justify-between text-xs mb-1">
                    <span className="text-white/40">Seats</span>
                    <span className={isFull ? 'text-red-400' : 'text-emerald-400'}>
                      {course.available_seats}/{course.capacity}
                    </span>
                  </div>
                  <div className="capacity-bar">
                    <div className={`capacity-fill ${
                      fillPercent >= 90 ? 'bg-gradient-to-r from-red-500 to-red-400' :
                      fillPercent >= 70 ? 'bg-gradient-to-r from-amber-500 to-amber-400' :
                      'bg-gradient-to-r from-emerald-500 to-emerald-400'
                    }`} style={{ width: `${Math.min(fillPercent, 100)}%` }} />
                  </div>
                </div>

                <button
                  onClick={() => handleRegister(course.id)}
                  disabled={isEnrolled || isFull || actionLoading === course.id}
                  className={isEnrolled ? 'btn-secondary w-full text-sm px-4 py-2 opacity-50' :
                    isFull ? 'btn-danger w-full text-sm px-4 py-2 opacity-50' :
                    'btn-primary w-full text-sm px-4 py-2'}>
                  {actionLoading === course.id ? 'Processing...' :
                    isEnrolled ? 'Already Enrolled' : isFull ? 'Course Full' : 'Register'}
                </button>
              </div>
            );
          })}
        </div>
      </div>
    </div>
  );
}
