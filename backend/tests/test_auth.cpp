/**
 * test_auth.cpp — Authentication unit tests
 */

#include <gtest/gtest.h>
#include "database/Database.hpp"
#include "repositories/UserRepository.hpp"
#include "services/AuthService.hpp"

class AuthTest : public ::testing::Test {
protected:
    void SetUp() override {
        db = std::make_unique<academia::Database>(":memory:");
        db->initialize_schema();
        user_repo = std::make_unique<academia::UserRepository>(*db);
        auth_service = std::make_unique<academia::AuthService>(*user_repo);

        // Create test user
        academia::User user;
        user.username = "testuser";
        user.password_hash = academia::AuthService::hash_password("testpass");
        user.role = academia::Role::Student;
        user.name = "Test User";
        user.email = "test@test.com";
        user_repo->create(user);
    }

    std::unique_ptr<academia::Database> db;
    std::unique_ptr<academia::UserRepository> user_repo;
    std::unique_ptr<academia::AuthService> auth_service;
};

TEST_F(AuthTest, LoginSuccess) {
    auto result = auth_service->login("testuser", "testpass");
    EXPECT_TRUE(result.success);
    EXPECT_FALSE(result.token.empty());
    EXPECT_EQ(result.user.username, "testuser");
    EXPECT_EQ(result.user.role, academia::Role::Student);
}

TEST_F(AuthTest, LoginWrongPassword) {
    auto result = auth_service->login("testuser", "wrongpass");
    EXPECT_FALSE(result.success);
    EXPECT_TRUE(result.token.empty());
    EXPECT_FALSE(result.error.empty());
}

TEST_F(AuthTest, LoginNonexistentUser) {
    auto result = auth_service->login("nouser", "testpass");
    EXPECT_FALSE(result.success);
}

TEST_F(AuthTest, TokenValidation) {
    auto login_result = auth_service->login("testuser", "testpass");
    ASSERT_TRUE(login_result.success);

    auto user = auth_service->validate_token(login_result.token);
    ASSERT_TRUE(user.has_value());
    EXPECT_EQ(user->username, "testuser");
}

TEST_F(AuthTest, InvalidToken) {
    auto user = auth_service->validate_token("invalid_token_here");
    EXPECT_FALSE(user.has_value());
}

TEST_F(AuthTest, Logout) {
    auto login_result = auth_service->login("testuser", "testpass");
    ASSERT_TRUE(login_result.success);

    auth_service->logout(login_result.token);

    auto user = auth_service->validate_token(login_result.token);
    EXPECT_FALSE(user.has_value());
}

TEST_F(AuthTest, PasswordHashing) {
    std::string hash1 = academia::AuthService::hash_password("password");
    std::string hash2 = academia::AuthService::hash_password("password");
    EXPECT_EQ(hash1, hash2);  // Same input → same hash

    std::string hash3 = academia::AuthService::hash_password("different");
    EXPECT_NE(hash1, hash3);  // Different input → different hash
}

TEST_F(AuthTest, DeactivatedUser) {
    // Create deactivated user
    academia::User user;
    user.username = "blocked";
    user.password_hash = academia::AuthService::hash_password("pass");
    user.role = academia::Role::Student;
    user.name = "Blocked User";
    user.active = false;
    user_repo->create(user);

    auto result = auth_service->login("blocked", "pass");
    EXPECT_FALSE(result.success);
    EXPECT_TRUE(result.error.find("deactivated") != std::string::npos);
}

TEST_F(AuthTest, RoleCheck) {
    academia::User admin;
    admin.role = academia::Role::Admin;
    EXPECT_TRUE(academia::AuthService::has_role(admin, academia::Role::Admin));
    EXPECT_TRUE(academia::AuthService::has_role(admin, academia::Role::Faculty)); // Admin can do all

    academia::User student;
    student.role = academia::Role::Student;
    EXPECT_TRUE(academia::AuthService::has_role(student, academia::Role::Student));
    EXPECT_FALSE(academia::AuthService::has_role(student, academia::Role::Faculty));
    EXPECT_FALSE(academia::AuthService::has_role(student, academia::Role::Admin));
}
