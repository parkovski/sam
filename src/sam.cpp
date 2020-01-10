#include "sam/tts.h"
#include "spdlog/common.h"

#include <spdlog/sinks/stdout_color_sinks.h>

#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <streambuf>
#include <filesystem>
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
    spdlog::set_pattern(
#     ifndef NDEBUG
        "\033[90m[%T.%e %^%l%$\033[90m]\033[m %v"
#     else
        "%^%l:%$ %v"
#     endif
    );
  }

  ~ScopedSpdlog() {
    spdlog::drop_all();
  }
};

struct CommandLine {
  bool showHelp = false;
  bool listVoices = false;
  bool showVoiceInfo = false;
  bool inputFromStdin = false;
  std::wstring voiceName = L"Sam";
  int pitch = 0;
  int speed = 0;
  std::wstring text;
  std::wstring outputFilename;
  int logLevel = -1;
};

static void showHelp() {
  fmt::print("{}",
    "sam - MS TTS4 client.\n"
    "Usage: samd [options] {text}...\n"
    "Options:\n"
    "-h         Show help.\n"
    "-l         List voices.\n"
    "-i         Show information for the selected voice.\n"
    "-v <name>  Select voice.\n"
    "-p <num>   Set voice pitch.\n"
    "-s <num>   Set voice speed.\n"
    "-f <file>  Set an input file to read.\n"
    "-o <file>  Write output to a file instead of playing directly.\n"
    "-V <level> Set logger level to: trace, debug, info, warn, error, critical, off.\n"
  );
}

static bool parseArgs(int argc, wchar_t *argv[], CommandLine &commandLine) {
  bool parseOptions = true;

  int i;
  for (i = 1; i < argc; ++i) {
    std::wstring_view arg = argv[i];
    auto const len = arg.length();
    if (!len) {
      continue;
    }

    if (arg[0] != '-' && arg[0] != '/') {
      break;
    }

    switch (arg[1]) {
      case 'h':
        commandLine.showHelp = true;
        return true;
      case 'l':
        commandLine.listVoices = true;
        return true;
      case 'i':
        commandLine.showVoiceInfo = true;
        return true;
      case 0:
        commandLine.inputFromStdin = true;
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
      case 'f':
        {
          std::wifstream input;
          if (len > 2) {
            input.open(std::filesystem::path(arg.substr(2)));
          } else if (++i < argc) {
            input.open(std::filesystem::path(argv[i]));
          } else {
            Log::error("Command line: -f without filename.");
            return false;
          }
          if (!input.good()) {
            Log::error("Couldn't open input file.");
            return false;
          }
          input.seekg(0, std::ios::end);
          commandLine.text.reserve(size_t(input.tellg()));
          input.seekg(0, std::ios::beg);
          commandLine.text.assign((std::istreambuf_iterator<wchar_t>(input)),
                                  std::istreambuf_iterator<wchar_t>());
          std::wcout << commandLine.text << L"\n";
        }
        break;
      case 'o':
        if (len > 2) {
          commandLine.outputFilename = arg.substr(2);
        } else if (++i < argc) {
          commandLine.outputFilename = argv[i];
        } else {
          Log::error("Command line: -o without filename.");
          return false;
        }
        break;
      case 'V':
        {
          std::wstring_view level;
          if (len > 2) {
            level = arg.substr(2);
          } else if (++i < argc) {
            level = argv[i];
          } else {
            Log::error("Command line: -V without log level.");
            return false;
          }

          if (level == L"trace") {
            commandLine.logLevel = spdlog::level::trace;
          } else if (level == L"debug") {
            commandLine.logLevel = spdlog::level::debug;
          } else if (level == L"info") {
            commandLine.logLevel = spdlog::level::info;
          } else if (level == L"warn") {
            commandLine.logLevel = spdlog::level::warn;
          } else if (level == L"error") {
            commandLine.logLevel = spdlog::level::err;
          } else if (level == L"critical") {
            commandLine.logLevel = spdlog::level::critical;
          } else if (level == L"off") {
            commandLine.logLevel = spdlog::level::off;
          } else {
            Log::error("Command line: Invalid log level for -V. Expected "
                       "trace, debug, info, warn, error, critical, off.");
            return false;
          }
        }
        break;
      case '-':
        if (arg[0] == '-' && arg.length() == 2) {
          parseOptions = false;
          break;
        }
        [[fallthrough]];
      default:
        goto break_arg_loop;
    }
  }
break_arg_loop:

  for (; i < argc; ++i) {
    std::wstring_view arg = argv[i];
    if (arg.empty() || std::all_of(arg.begin(), arg.end(), iswspace)) {
      continue;
    }

    commandLine.text += arg;
    commandLine.text += L" ";
  }

  return true;
}

int wmain(int argc, wchar_t *argv[]) {
  ScopedSpdlog _scopedSpdlog;
  Log::g_logger->set_level(
#   ifndef NDEBUG
      spdlog::level::trace
#   else
      spdlog::level::info
#   endif
  );

  CommandLine commandLine;
  if (!parseArgs(argc, argv, commandLine)) {
    return 1;
  }
  if (commandLine.text.empty()) {
    commandLine.inputFromStdin = true;
  }

  if (commandLine.logLevel > -1 && commandLine.logLevel <= spdlog::level::off) {
    Log::g_logger->set_level((spdlog::level::level_enum)commandLine.logLevel);
  }

  if (commandLine.showHelp) {
    showHelp();
    return 0;
  }

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

  hr = tts.init(commandLine.voiceName, commandLine.outputFilename);
  if (FAILED(hr)) {
    Log::critical("Couldn't initialize TTS!");
    return hr;
  }

  if (commandLine.showVoiceInfo) {
    auto [minPitch, maxPitch, defPitch] = tts.pitchInfo();
    auto [minSpeed, maxSpeed, defSpeed] = tts.speedInfo();
    Log::info("Pitch: min={}; max={}; default={}.", minPitch, maxPitch,
              defPitch);
    Log::info("Speed: min={}; max={}; default={}.", minSpeed, maxSpeed,
              defSpeed);
    return 0;
  }

  NotifySink sink;
  if (commandLine.inputFromStdin) {
    Log::info("Interactive mode. Ctrl-Z to end.");
    SetConsoleCtrlHandler(nullptr, false);
    std::wstring line{};
    while (std::wcin.good()) {
      line.clear();
      std::getline(std::wcin, line);
      if (std::all_of(line.cbegin(), line.cend(), iswspace)) {
        continue;
      }
      tts.say(line, sink, (WORD)commandLine.pitch,
              (DWORD)commandLine.speed);
      sink.wait();
    }
  } else {
    tts.say(commandLine.text, sink, (WORD)commandLine.pitch,
            (DWORD)commandLine.speed);
    sink.wait();
  }

  return 0;
}
