#ifndef TEST_HELPERS_HPP
#define TEST_HELPERS_HPP

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <unistd.h>
#include <vector>

namespace test {

class TestRunner {
public:
    static TestRunner& instance() {
        static TestRunner r;
        return r;
    }

    void check(bool condition, const std::string& msg, const char* file, int line) {
        if (condition) {
            passed_++;
        } else {
            failed_++;
            std::cerr << "[FAIL] " << file << ":" << line << " - " << msg << "\n";
        }
    }

    int report() const {
        std::cout << "\nTest Summary: " << passed_ << " passed, " << failed_ << " failed.\n";
        if (failed_ == 0) {
            std::cout << "ALL TESTS PASSED\n";
            return 0;
        } else {
            std::cout << "TESTS FAILED\n";
            return 1;
        }
    }

    size_t passed() const { return passed_; }
    size_t failed() const { return failed_; }
    void reset() { passed_ = 0; failed_ = 0; }

private:
    TestRunner() = default;
    size_t passed_{0};
    size_t failed_{0};
};

inline int report() {
    return TestRunner::instance().report();
}

// RAII temporary directory helper for isolated test I/O
class TempDir {
public:
    TempDir() {
        const char* env_tmp = std::getenv("TMPDIR");
        std::string base = (env_tmp != nullptr && env_tmp[0] != '\0') ? env_tmp : "/tmp";
        if (base.back() != '/') {
            base.push_back('/');
        }
        std::string tmpl = base + "test_tmp_XXXXXX";
        std::vector<char> buf(tmpl.begin(), tmpl.end());
        buf.push_back('\0');

        char* res = ::mkdtemp(buf.data());
        if (res == nullptr) {
            throw std::runtime_error("TempDir: gagal membuat direktori sementara");
        }
        path_ = res;
    }

    ~TempDir() {
        if (!path_.empty()) {
            std::error_code ec;
            std::filesystem::remove_all(path_, ec);
        }
    }

    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;

    const std::string& path() const { return path_; }

private:
    std::string path_;
};

} // namespace test

#define CHECK(cond, msg) \
    ::test::TestRunner::instance().check((cond), (msg), __FILE__, __LINE__)

#define CHECK_THROWS(expr, ExceptionType, msg) \
    do { \
        bool caught_expected = false; \
        try { \
            expr; \
        } catch (const ExceptionType&) { \
            caught_expected = true; \
        } catch (...) { \
            caught_expected = false; \
        } \
        ::test::TestRunner::instance().check(caught_expected, (msg), __FILE__, __LINE__); \
    } while (0)

#define CHECK_NO_THROW(expr, msg) \
    do { \
        bool threw_exception = false; \
        try { \
            expr; \
        } catch (...) { \
            threw_exception = true; \
        } \
        ::test::TestRunner::instance().check(!threw_exception, (msg), __FILE__, __LINE__); \
    } while (0)

#endif // TEST_HELPERS_HPP
