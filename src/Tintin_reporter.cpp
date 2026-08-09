#include "Tintin_reporter.hpp"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <system_error>

Tintin_reporter& Tintin_reporter::getInstance() {
	static Tintin_reporter instance;
	return instance;
}

bool Tintin_reporter::init() {
	std::lock_guard<std::mutex> lock(logMutex);

	const std::string dirPath = "/var/log/matt_daemon";
	const std::string filePath = dirPath + "/matt_daemon.log";

	std::error_code ec;
	std::filesystem::create_directories(dirPath, ec);
	if (ec) {
		std::cerr << "Failed to create directory " << dirPath << ": " << ec.message() << '\n';
		return false;
	}

	logFile.open(filePath, std::ios::out | std::ios::app);
	if (!logFile.is_open()) {
		std::cerr << "Failed to open log file " << filePath << '\n';
		return false;
	}

	return true;
}

void Tintin_reporter::log(Level level, const std::string& message) {
	std::lock_guard<std::mutex> lock(logMutex);
	if (!logFile.is_open()) return;
	logFile << getFormattedTimestamp()
		<< " [" << levelToString(level) << "] " << message << std::endl;
}

Tintin_reporter::~Tintin_reporter() {
	if (logFile.is_open()) {
		logFile.close();
	}
}

std::string Tintin_reporter::getFormattedTimestamp() {
	auto now = std::chrono::system_clock::now();
	std::time_t now_time = std::chrono::system_clock::to_time_t(now);
	std::tm local_tm{};
	localtime_r(&now_time, &local_tm); // Thread-safe local time

	std::ostringstream ss;
	// Output format: [ DD / MM / YYYY - HH : MM : SS ]
	ss << '[' << ' '
		<< std::setfill('0') << std::setw(2) << local_tm.tm_mday << " / "
		<< std::setfill('0') << std::setw(2) << (local_tm.tm_mon + 1) << " / "
		<< (local_tm.tm_year + 1900) << " - "
		<< std::setfill('0') << std::setw(2) << local_tm.tm_hour << " : "
		<< std::setfill('0') << std::setw(2) << local_tm.tm_min << " : "
		<< std::setfill('0') << std::setw(2) << local_tm.tm_sec
		<< ' ' << ']';

	return ss.str();
}

const char* Tintin_reporter::levelToString(Level level) {
	switch (level) {
		case Level::INFO:  return "INFO";
		case Level::WARN:  return "WARNING";
		case Level::ERROR: return "ERROR";
		default:           return "LOG";
	}
}
