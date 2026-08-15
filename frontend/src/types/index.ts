export interface User {
  id: number;
  username: string;
  role: 'admin' | 'faculty' | 'student';
  name: string;
  email: string;
  active: boolean;
  created_at: string;
}

export interface Course {
  id: number;
  course_code: string;
  course_name: string;
  credits: number;
  faculty_id: number;
  faculty_name: string;
  capacity: number;
  enrolled_count: number;
  available_seats: number;
  schedule: string;
  created_at: string;
}

export interface Registration {
  id: number;
  student_id: number;
  course_id: number;
  status: 'active' | 'dropped';
  registered_at: string;
  student_name: string;
  course_code: string;
  course_name: string;
}

export interface ApiResponse<T = any> {
  success: boolean;
  message?: string;
  data?: T;
  error?: string;
}

export interface LoginRequest {
  username: string;
  password: string;
}

export interface LoginResponse {
  token: string;
  user: User;
}

export interface Statistics {
  total_users: number;
  total_students: number;
  total_faculty: number;
  total_admins: number;
  total_courses: number;
  total_registrations: number;
  overall_capacity: number;
  overall_enrolled: number;
  overall_utilization: number;
  course_utilization: CourseUtilization[];
}

export interface CourseUtilization {
  course_code: string;
  course_name: string;
  capacity: number;
  enrolled: number;
  utilization: number;
}
