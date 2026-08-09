#ifndef TINTIN_REPORTER_HPP
#define TINTIN_REPORTER_HPP

#include <string>
#include <fstream>
#include <iomanip>
#include <mutex>

class Tintin_reporter {
public:
    enum class Level { INFO, WARN, ERROR };

    static Tintin_reporter& getInstance();

    bool init();
    void log(Level level, const std::string& message);

    Tintin_reporter(const Tintin_reporter&) = delete;
    Tintin_reporter& operator=(const Tintin_reporter&) = delete;

private:
    Tintin_reporter() = default;
    ~Tintin_reporter();

    std::ofstream logFile;
    std::mutex logMutex;

    std::string getFormattedTimestamp();
    const char* levelToString(Level level);
};

#endif
