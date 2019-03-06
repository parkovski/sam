#include "sam/tts.h"

#include <spdlog/sinks/stdout_color_sinks.h>

#include <iostream>
#include <string>
#include <sstream>
#include <cstdlib>

// RAII Wrapper around initializing and shutting down COM.
struct RAIIOLEInit {
  explicit RAIIOLEInit() noexcept {
    if (!FAILED(CoInitialize(nullptr))) {
      _ok = true;
    }
  }

  RAIIOLEInit(const RAIIOLEInit &) = delete;
  RAIIOLEInit &operator=(const RAIIOLEInit &) = delete;

  ~RAIIOLEInit() {
    if (_ok) {
      CoUninitialize();
    }
  }

  // Was COM initialization successful?
  explicit operator bool() const {
    return _ok;
  }

private:
  bool _ok = false;
};

namespace Log {
  std::shared_ptr<spdlog::logger> g_logger;
}

struct ScopedSpdlog {
  explicit ScopedSpdlog() noexcept {
    Log::g_logger = spdlog::stdout_color_mt("sam");
    spdlog::set_pattern("\033[90m[%T.%e]\033[m %^[%l]%$ %v");
  }

  ~ScopedSpdlog() {
    spdlog::drop_all();
  }
};

struct CommandLine {
  bool listVoices = false;
  std::wstring voiceName = L"Sam";
  int pitch = 0;
  int speed = 0;
  std::wstring text;
};

static void showHelp() {
  Log::info("Help message goes here.");
}

static bool parseArgs(int argc, wchar_t *argv[], CommandLine &commandLine) {
  for (int i = 1; i < argc; ++i) {
    std::wstring_view arg = argv[i];
    auto len = arg.length();
    if (!len) {
      continue;
    }

    if (arg[0] == '-' || arg[0] == '/') {
      switch (arg[1]) {
        case 'h':
          showHelp();
          return false;
        case 'l':
          commandLine.listVoices = true;
          break;
        case 'v':
          if (len > 2) {
            commandLine.voiceName = arg.substr(2);
          } else if (++i < argc) {
            commandLine.voiceName = argv[i];
            break;
          } else {
            Log::error("Command line: -v without voice name.");
            return false;
          }
          break;
        case 'p':
          if (len > 2) {
            commandLine.pitch = wcstol(argv[i] + 2, nullptr, 10);
          } else if (++i < argc) {
            commandLine.pitch = wcstol(argv[i], nullptr, 10);
          } else {
            Log::error("Command line: -p without pitch value.");
            return false;
          }
          break;
        case 's':
          if (len > 2) {
            commandLine.speed = wcstol(argv[i] + 2, nullptr, 10);
          } else if (++i < argc) {
            commandLine.speed = wcstol(argv[i], nullptr, 10);
          } else {
            Log::error("Command line: -s without speed value.");
            return false;
          }
          break;
        default:
          Log::error(L"Unrecognized option {}", arg);
          return false;
      }
    } else {
      commandLine.text += arg;
      commandLine.text += L" ";
    }
  }

  return true;
}

int wmain(int argc, wchar_t *argv[]) {
  CommandLine commandLine;
  std::wstringstream wss;
  if (!parseArgs(argc, argv, commandLine)) {
    return 1;
  }

  ScopedSpdlog _scopedSpdlog;
  Log::g_logger->set_level(spdlog::level::trace);

  RAIIOLEInit oleInit;
  if (!oleInit) {
    Log::critical("Couldn't initialize OLE!");
    return GetLastError();
  }

  TTSContainer tts;
  HRESULT hr;
  if (commandLine.listVoices) {
    if (FAILED(hr = tts.listVoices())) {
      Log::critical("TTS enumeration failed.");
      return hr;
    }
    return 0;
  }

  if (FAILED(hr = tts.init(commandLine.voiceName))) {
    Log::critical("Couldn't initialize TTS!");
    return hr;
  }

  NotifySink sink;
  tts.say(commandLine.text, sink, (WORD)commandLine.pitch,
          (DWORD)commandLine.speed);

  return 0;
}

