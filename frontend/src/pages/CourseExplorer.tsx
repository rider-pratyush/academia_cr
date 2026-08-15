import { useEffect, useState } from 'react';
import { useAuth } from '../context/AuthContext';
import { courseApi } from '../services/api';
import { Course } from '../types';

export default function CourseExplorer() {
  const { user } = useAuth();
  const [courses, setCourses] = useState<Course[]>([]);
  const [search, setSearch] = useState('');
  const [sortBy, setSortBy] = useState<'code' | 'name' | 'seats' | 'credits'>('code');
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    const fetchCourses = async () => {
      try {
        const res = await courseApi.getAll(search || undefined);
        setCourses(res.data.data || []);
      } catch (err) { console.error(err); }
      finally { setLoading(false); }
    };
    const timer = setTimeout(fetchCourses, 300);
    return () => clearTimeout(timer);
  }, [search]);

  const sorted = [...courses].sort((a, b) => {
    switch (sortBy) {
      case 'code': return a.course_code.localeCompare(b.course_code);
      case 'name': return a.course_name.localeCompare(b.course_name);
      case 'seats': return b.available_seats - a.available_seats;
      case 'credits': return b.credits - a.credits;
    }
  });

  return (
    <div className="space-y-6 animate-fade-in">
      <div>
        <h1 className="text-3xl font-bold">Course Explorer 📚</h1>
        <p className="text-white/40 mt-1">Browse and search all available courses</p>
      </div>

      {/* Search & Filter */}
      <div className="flex flex-col md:flex-row gap-4">
        <div className="flex-1">
          <input type="text" value={search} onChange={(e) => setSearch(e.target.value)}
            className="input-field" placeholder="🔍 Search by course code, name, or faculty..." />
        </div>
        <div className="flex gap-2">
          {(['code', 'name', 'seats', 'credits'] as const).map((s) => (
            <button key={s} onClick={() => setSortBy(s)}
              className={`px-4 py-2 rounded-lg text-sm font-medium transition-all ${
                sortBy === s ? 'bg-primary-500/20 text-primary-400 border border-primary-500/30' : 'bg-white/5 text-white/40 hover:text-white hover:bg-white/10'
              }`}>
              {s.charAt(0).toUpperCase() + s.slice(1)}
            </button>
          ))}
        </div>
      </div>

      {/* Results count */}
      <p className="text-white/40 text-sm">{sorted.length} courses found</p>

      {loading ? (
        <div className="flex justify-center py-12"><div className="animate-spin w-8 h-8 border-2 border-primary-400 border-t-transparent rounded-full" /></div>
      ) : sorted.length === 0 ? (
        <div className="glass-card p-12 text-center text-white/40">
          <p className="text-4xl mb-2">🔍</p>
          <p>No courses found matching your search</p>
        </div>
      ) : (
        <div className="glass-card overflow-hidden">
          <table className="w-full">
            <thead>
              <tr className="border-b border-white/10">
                <th className="text-left p-4 text-sm font-medium text-white/40">Code</th>
                <th className="text-left p-4 text-sm font-medium text-white/40">Course Name</th>
                <th className="text-left p-4 text-sm font-medium text-white/40">Faculty</th>
                <th className="text-center p-4 text-sm font-medium text-white/40">Credits</th>
                <th className="text-center p-4 text-sm font-medium text-white/40">Schedule</th>
                <th className="text-center p-4 text-sm font-medium text-white/40">Capacity</th>
              </tr>
            </thead>
            <tbody>
              {sorted.map((course, idx) => {
                const fillPercent = course.capacity > 0 ? (course.enrolled_count / course.capacity) * 100 : 0;
                const isFull = course.available_seats <= 0;
                return (
                  <tr key={course.id} className={`border-b border-white/5 hover:bg-white/5 transition-colors ${idx % 2 === 0 ? '' : 'bg-white/[0.02]'}`}>
                    <td className="p-4"><span className="badge-info">{course.course_code}</span></td>
                    <td className="p-4 font-medium">{course.course_name}</td>
                    <td className="p-4 text-white/60">{course.faculty_name || 'TBA'}</td>
                    <td className="p-4 text-center">{course.credits}</td>
                    <td className="p-4 text-center text-white/60 text-sm">{course.schedule || 'TBA'}</td>
                    <td className="p-4">
                      <div className="flex items-center gap-2">
                        <div className="capacity-bar flex-1">
                          <div className={`capacity-fill ${isFull ? 'bg-red-500' : fillPercent > 70 ? 'bg-amber-500' : 'bg-emerald-500'}`}
                            style={{ width: `${Math.min(fillPercent, 100)}%` }} />
                        </div>
                        <span className={`text-xs font-medium min-w-[40px] text-right ${isFull ? 'text-red-400' : 'text-white/60'}`}>
                          {course.enrolled_count}/{course.capacity}
                        </span>
                      </div>
                    </td>
                  </tr>
                );
              })}
            </tbody>
          </table>
        </div>
      )}
    </div>
  );
}
