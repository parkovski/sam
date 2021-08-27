#ifndef SAM_SAM_H_
#define SAM_SAM_H_

#define WIN32_LEAN_AND_MEAN
#define _UNICODE
#define UNICODE

#include <Windows.h>
#include <initguid.h>
#include <objbase.h>
#include <objerror.h>
#include <ole2ver.h>

#include <spdlog/spdlog.h>

namespace Log {

extern std::shared_ptr<spdlog::logger> g_logger;

template<typename... Args>
static inline void trace(const char *fmt, Args &&...args)
{ g_logger->trace(fmt, std::forward<Args>(args)...); }

template<typename... Args>
static inline void trace(const wchar_t *fmt, Args &&...args)
{ g_logger->trace(fmt, std::forward<Args>(args)...); }

template<typename... Args>
static inline void debug(const char *fmt, Args &&...args)
{ g_logger->debug(fmt, std::forward<Args>(args)...); }

template<typename... Args>
static inline void debug(const wchar_t *fmt, Args &&...args)
{ g_logger->debug(fmt, std::forward<Args>(args)...); }

template<typename... Args>
static inline void info(const char *fmt, Args &&...args)
{ g_logger->info(fmt, std::forward<Args>(args)...); }

template<typename... Args>
static inline void info(const wchar_t *fmt, Args &&...args)
{ g_logger->info(fmt, std::forward<Args>(args)...); }

template<typename... Args>
static inline void warn(const char *fmt, Args &&...args)
{ g_logger->warn(fmt, std::forward<Args>(args)...); }

template<typename... Args>
static inline void warn(const wchar_t *fmt, Args &&...args)
{ g_logger->warn(fmt, std::forward<Args>(args)...); }

template<typename... Args>
static inline void error(const char *fmt, Args &&...args)
{ g_logger->error(fmt, std::forward<Args>(args)...); }

template<typename... Args>
static inline void error(const wchar_t *fmt, Args &&...args)
{ g_logger->error(fmt, std::forward<Args>(args)...); }

template<typename... Args>
static inline void critical(const char *fmt, Args &&...args)
{ g_logger->critical(fmt, std::forward<Args>(args)...); }

} // namespace log

#endif /* SAM_SAM_H_ */

