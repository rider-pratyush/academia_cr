import { useEffect, useState } from 'react';
import { useAuth } from '../context/AuthContext';
import { courseApi } from '../services/api';
import { Course, Registration } from '../types';

export default function FacultyDashboard() {
  const { user } = useAuth();
  const [courses, setCourses] = useState<Course[]>([]);
  const [loading, setLoading] = useState(true);
  const [showForm, setShowForm] = useState(false);
  const [form, setForm] = useState({ course_code: '', course_name: '', credits: 3, capacity: 30, schedule: '' });
  const [message, setMessage] = useState<{ type: 'success' | 'error'; text: string } | null>(null);
  const [selectedCourse, setSelectedCourse] = useState<Course | null>(null);
  const [students, setStudents] = useState<Registration[]>([]);

  const fetchCourses = async () => {
    if (!user) return;
    try {
      const res = await courseApi.getAll();
      const allCourses = (res.data.data || []) as Course[];
      setCourses(allCourses.filter(c => c.faculty_id === user.id));
    } catch (err) { console.error(err); }
    finally { setLoading(false); }
  };

  useEffect(() => { fetchCourses(); }, [user]);

  const handleCreate = async (e: React.FormEvent) => {
    e.preventDefault();
    try {
      await courseApi.create({ ...form, faculty_id: user?.id });
      setMessage({ type: 'success', text: 'Course created successfully!' });
      setShowForm(false);
      setForm({ course_code: '', course_name: '', credits: 3, capacity: 30, schedule: '' });
      fetchCourses();
    } catch (err: any) {
      setMessage({ type: 'error', text: err.response?.data?.error || 'Failed to create course' });
    }
  };

  const handleViewStudents = async (course: Course) => {
    setSelectedCourse(course);
    try {
      const res = await courseApi.getStudents(course.id);
      setStudents(res.data.data || []);
    } catch { setStudents([]); }
  };

  const totalEnrolled = courses.reduce((sum, c) => sum + c.enrolled_count, 0);
  const totalCapacity = courses.reduce((sum, c) => sum + c.capacity, 0);

  if (loading) return <div className="flex items-center justify-center h-64"><div className="animate-spin w-8 h-8 border-2 border-primary-400 border-t-transparent rounded-full" /></div>;

  return (
    <div className="space-y-8 animate-fade-in">
      <div className="flex items-center justify-between">
        <div>
          <h1 className="text-3xl font-bold">Faculty Dashboard 👨‍🏫</h1>
          <p className="text-white/40 mt-1">Manage your courses and view enrollments</p>
        </div>
        <button onClick={() => setShowForm(!showForm)} className="btn-primary">
          {showForm ? 'Cancel' : '+ New Course'}
        </button>
      </div>

      {message && (
        <div className={`p-4 rounded-xl border animate-fade-in ${
          message.type === 'success' ? 'bg-emerald-500/10 border-emerald-500/20 text-emerald-400' : 'bg-red-500/10 border-red-500/20 text-red-400'
        }`}>{message.text}</div>
      )}

      {/* Stats */}
      <div className="grid grid-cols-1 md:grid-cols-4 gap-4">
        <div className="stat-card">
          <span className="text-white/40 text-sm">My Courses</span>
          <span className="text-3xl font-bold text-emerald-400">{courses.length}</span>
        </div>
        <div className="stat-card">
          <span className="text-white/40 text-sm">Total Enrolled</span>
          <span className="text-3xl font-bold text-primary-400">{totalEnrolled}</span>
        </div>
        <div className="stat-card">
          <span className="text-white/40 text-sm">Total Capacity</span>
          <span className="text-3xl font-bold text-accent-400">{totalCapacity}</span>
        </div>
        <div className="stat-card">
          <span className="text-white/40 text-sm">Avg Utilization</span>
          <span className="text-3xl font-bold text-amber-400">
            {totalCapacity > 0 ? Math.round(totalEnrolled / totalCapacity * 100) : 0}%
          </span>
        </div>
      </div>

      {/* New Course Form */}
      {showForm && (
        <div className="glass-card p-6 animate-fade-in">
          <h3 className="text-lg font-semibold mb-4">Create New Course</h3>
          <form onSubmit={handleCreate} className="grid grid-cols-1 md:grid-cols-2 gap-4">
            <input value={form.course_code} onChange={e => setForm({...form, course_code: e.target.value})}
              className="input-field" placeholder="Course Code (e.g. CS301)" required />
            <input value={form.course_name} onChange={e => setForm({...form, course_name: e.target.value})}
              className="input-field" placeholder="Course Name" required />
            <input type="number" value={form.credits} onChange={e => setForm({...form, credits: +e.target.value})}
              className="input-field" placeholder="Credits" min={1} max={6} />
            <input type="number" value={form.capacity} onChange={e => setForm({...form, capacity: +e.target.value})}
              className="input-field" placeholder="Capacity" min={1} />
            <input value={form.schedule} onChange={e => setForm({...form, schedule: e.target.value})}
              className="input-field" placeholder="Schedule (e.g. Mon/Wed 10:00-11:30)" />
            <button type="submit" className="btn-primary">Create Course</button>
          </form>
        </div>
      )}

      {/* Courses */}
      <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
        {courses.map(course => {
          const fillPercent = course.capacity > 0 ? (course.enrolled_count / course.capacity) * 100 : 0;
          return (
            <div key={course.id} className="glass-card-hover p-5">
              <div className="flex justify-between items-start mb-3">
                <span className="badge-info text-sm">{course.course_code}</span>
                <span className="text-xs text-white/40">{course.credits} credits</span>
              </div>
              <h3 className="text-lg font-semibold mb-1">{course.course_name}</h3>
              <p className="text-white/40 text-sm mb-2">📅 {course.schedule || 'TBA'}</p>
              <div className="mb-3">
                <div className="flex justify-between text-xs mb-1">
                  <span className="text-white/40">Enrollment</span>
                  <span className="text-white/60">{course.enrolled_count}/{course.capacity} ({Math.round(fillPercent)}%)</span>
                </div>
                <div className="capacity-bar">
                  <div className={`capacity-fill ${fillPercent >= 90 ? 'bg-red-500' : fillPercent >= 70 ? 'bg-amber-500' : 'bg-emerald-500'}`}
                    style={{ width: `${Math.min(fillPercent, 100)}%` }} />
                </div>
              </div>
              <button onClick={() => handleViewStudents(course)}
                className="btn-secondary w-full text-sm px-4 py-2">
                View Enrolled Students
              </button>
            </div>
          );
        })}
      </div>

      {/* Student List Modal */}
      {selectedCourse && (
        <div className="fixed inset-0 bg-black/60 backdrop-blur-sm flex items-center justify-center z-50 p-4" onClick={() => setSelectedCourse(null)}>
          <div className="glass-card p-6 max-w-lg w-full max-h-[80vh] overflow-auto animate-fade-in" onClick={e => e.stopPropagation()}>
            <div className="flex justify-between items-center mb-4">
              <h3 className="text-lg font-semibold">Students in {selectedCourse.course_code}</h3>
              <button onClick={() => setSelectedCourse(null)} className="text-white/40 hover:text-white text-xl">✕</button>
            </div>
            {students.length === 0 ? (
              <p className="text-white/40 text-center py-4">No students enrolled yet</p>
            ) : (
              <div className="space-y-2">
                {students.map((s, idx) => (
                  <div key={s.id} className="flex items-center gap-3 p-3 bg-white/5 rounded-lg">
                    <span className="w-8 h-8 rounded-full bg-primary-500/20 flex items-center justify-center text-sm font-semibold text-primary-400">
                      {idx + 1}
                    </span>
                    <div>
                      <p className="font-medium text-sm">{s.student_name}</p>
                      <p className="text-xs text-white/40">Registered: {new Date(s.registered_at).toLocaleDateString()}</p>
                    </div>
                  </div>
                ))}
              </div>
            )}
          </div>
        </div>
      )}
    </div>
  );
}
