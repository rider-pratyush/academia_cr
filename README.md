# Academia Course Registration System

A production-quality, full-stack course registration system built with **C++20** and **React/TypeScript**, demonstrating operating systems concepts, concurrent programming, and modern software architecture.

> **Originally an OS course project** — rebuilt from scratch as a professional application while preserving and expanding the systems-programming concepts.

## 🎯 Features

- **Role-based access control** — Admin, Faculty, and Student roles with backend-enforced authorization
- **Concurrent-safe course registration** — Triple-layer race condition prevention (mutex + transaction + constraint)
- **Thread pool** — Bounded worker threads with producer-consumer task queue
- **REST API** — Clean JSON API with proper HTTP status codes and error handling
- **Modern React dashboard** — Glassmorphic UI with charts, search, and real-time capacity indicators
- **SQLite persistence** — ACID transactions, prepared statements, relational schema
- **Structured logging** — Thread-safe, timestamped, severity-leveled
- **Docker support** — Single-command deployment

## 🏗 Architecture

```
React Frontend (Vite + Tailwind)
        │
        │ HTTP/JSON (REST API)
        ▼
┌─────────────────────────────────────┐
│     C++20 REST API Server (Crow)     │
│                                     │
│  ┌──────────┐  ┌──────────────────┐ │
│  │   Auth   │  │    Course        │ │
│  │Controller│  │   Controller     │ │
│  └────┬─────┘  └───────┬─────────┘ │
│       │                │           │
│  ┌────▼─────┐  ┌───────▼─────────┐ │
│  │   Auth   │  │  Registration   │ │
│  │ Service  │  │    Service      │ │
│  └────┬─────┘  └───────┬─────────┘ │
│       │        ┌───────┤           │
│       │        │  Per-Course Mutex │
│       │        │  (std::mutex)    │
│  ┌────▼────────▼──────────────────┐│
│  │     SQLite Database            ││
│  │  (BEGIN IMMEDIATE transactions)││
│  └────────────────────────────────┘│
│                                     │
│  ┌────────────────────────────────┐ │
│  │       Thread Pool              │ │
│  │  (std::thread + condition_var) │ │
│  └────────────────────────────────┘ │
└─────────────────────────────────────┘
```

## 🛠 Tech Stack

| Layer | Technology |
|-------|-----------|
| Backend Language | C++20 |
| HTTP Framework | Crow |
| Database | SQLite3 |
| JSON | nlohmann/json |
| Threading | std::thread, std::mutex, std::shared_mutex, std::condition_variable |
| Testing | GoogleTest |
| Frontend | React 18 + TypeScript |
| Build Tool | Vite |
| CSS | Tailwind CSS |
| Charts | Recharts |
| HTTP Client | Axios |
| Containerization | Docker + Docker Compose |

## 📁 Project Structure

```
├── backend/
│   ├── CMakeLists.txt
│   ├── include/
│   │   ├── concurrency/ThreadPool.hpp
│   │   ├── controllers/{Auth,Course,Registration,Admin}Controller.hpp
│   │   ├── database/Database.hpp
│   │   ├── middleware/AuthMiddleware.hpp
│   │   ├── models/{User,Course,Registration}.hpp
│   │   ├── repositories/{User,Course,Registration}Repository.hpp
│   │   ├── services/{Auth,Course,Registration}Service.hpp
│   │   └── utils/Logger.hpp
│   ├── src/
│   │   ├── database/Database.cpp
│   │   └── main.cpp
│   └── tests/
│       ├── test_auth.cpp
│       ├── test_courses.cpp
│       ├── test_registration.cpp
│       └── test_concurrency.cpp
├── frontend/
│   └── src/
│       ├── components/Layout.tsx
│       ├── context/AuthContext.tsx
│       ├── pages/{Login,Student,Faculty,Admin,CourseExplorer}.tsx
│       ├── services/api.ts
│       └── types/index.ts
├── systems/
│   ├── tcp/{TcpServer,TcpClient}.cpp
│   ├── synchronization/MutexDemo.cpp
│   ├── threadpool/ThreadPoolDemo.cpp
│   └── producer_consumer/ProducerConsumer.cpp
├── docs/
│   ├── interview_notes.md
│   └── architecture_overview.html
├── docker/
│   ├── backend.Dockerfile
│   ├── frontend.Dockerfile
│   └── nginx.conf
├── docker-compose.yml
└── README.md
```

## 🔒 Database Schema

```sql
users (id, username, password_hash, role, name, email, active, created_at)
courses (id, course_code, course_name, credits, faculty_id, capacity, schedule)
registrations (id, student_id, course_id, status, registered_at) UNIQUE(student_id, course_id)
sessions (token, user_id, created_at, expires_at)
```

## 🔐 Demo Accounts

| Role | Username | Password |
|------|----------|----------|
| Admin | admin | admin123 |
| Faculty | dr.smith | faculty123 |
| Faculty | dr.jones | faculty123 |
| Student | john.doe | student123 |
| Student | jane.doe | student123 |

## 🚀 Running Locally

### With Docker
```bash
docker compose up --build
# Frontend: http://localhost:3000
# API: http://localhost:8080/api/health
```

### Without Docker

**Backend:**
```bash
cd backend
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
./academia_server
```

**Frontend:**
```bash
cd frontend
npm install
npm run dev
```

### Running Tests
```bash
cd backend/build
ctest --output-on-failure
```

## 🧪 Concurrency Test

The critical test: **100 threads race to register for a course with capacity 20.**

Expected: exactly 20 succeed, exactly 80 fail, zero duplicates, zero corruption.

```cpp
// test_concurrency.cpp
TEST_F(ConcurrencyTest, HundredThreadsCapacityTwenty) {
    // 100 threads start simultaneously via std::barrier
    // Each attempts to register for the same course
    EXPECT_EQ(success_count.load(), 20);
    EXPECT_EQ(failure_count.load(), 80);
}
```

## 📚 OS Concepts Demonstrated

| Concept | Implementation |
|---------|---------------|
| Thread Pool | `ThreadPool.hpp` — fixed workers, task queue, condition variable |
| Producer-Consumer | Thread pool task queue + `systems/producer_consumer/` demo |
| Mutex | Per-course `std::mutex` in RegistrationService |
| Reader-Writer Lock | `std::shared_mutex` on session store and course mutex map |
| Condition Variable | Thread pool worker wait/notify mechanism |
| RAII | Database, PreparedStatement, lock_guard, unique_lock |
| TCP Networking | `systems/tcp/TcpServer.cpp` — socket lifecycle demo |
| Message Framing | 4-byte length-prefix protocol in TCP demo |
| Signal Handling | Graceful shutdown on SIGINT/SIGTERM |
| Atomic Operations | `std::atomic<int>` counters in concurrency tests |
| Race Condition Prevention | Triple-layer: mutex + transaction + constraint |
| Transaction Isolation | `BEGIN IMMEDIATE` for serializable writes |


## 📄 License

MIT
