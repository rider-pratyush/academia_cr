import axios from 'axios';

const API_BASE = '/api';

const api = axios.create({
  baseURL: API_BASE,
  headers: { 'Content-Type': 'application/json' },
});

// Request interceptor: attach auth token
api.interceptors.request.use((config) => {
  const token = localStorage.getItem('auth_token');
  if (token) {
    config.headers.Authorization = `Bearer ${token}`;
  }
  return config;
});

// Response interceptor: handle 401
api.interceptors.response.use(
  (response) => response,
  (error) => {
    if (error.response?.status === 401) {
      localStorage.removeItem('auth_token');
      localStorage.removeItem('auth_user');
      window.location.href = '/login';
    }
    return Promise.reject(error);
  }
);

export const authApi = {
  login: (username: string, password: string) =>
    api.post('/auth/login', { username, password }),
  logout: () => api.post('/auth/logout'),
  me: () => api.get('/auth/me'),
};

export const courseApi = {
  getAll: (search?: string) =>
    api.get('/courses', { params: search ? { search } : {} }),
  getById: (id: number) => api.get(`/courses/${id}`),
  create: (data: any) => api.post('/courses', data),
  update: (id: number, data: any) => api.put(`/courses/${id}`, data),
  delete: (id: number) => api.delete(`/courses/${id}`),
  getStudents: (courseId: number) => api.get(`/courses/${courseId}/students`),
};

export const registrationApi = {
  register: (courseId: number) => api.post(`/courses/${courseId}/register`),
  drop: (courseId: number) => api.delete(`/courses/${courseId}/register`),
  getStudentCourses: (studentId: number) => api.get(`/students/${studentId}/courses`),
  getFacultyCourses: (facultyId: number) => api.get(`/faculty/${facultyId}/courses`),
};

export const adminApi = {
  getUsers: () => api.get('/admin/users'),
  createUser: (data: any) => api.post('/admin/users', data),
  updateUser: (id: number, data: any) => api.put(`/admin/users/${id}`, data),
  deleteUser: (id: number) => api.delete(`/admin/users/${id}`),
  getStatistics: () => api.get('/admin/statistics'),
};

export default api;
