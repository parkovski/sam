#include "sam/tts.h"

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
    spdlog::set_pattern("\033[90m[%T.%e]\033[m %^[%l]%$ %v");
  }

  ~ScopedSpdlog() {
    spdlog::drop_all();
  }
};

struct CommandLine {
  bool showHelp = false;
  bool listVoices = false;
  bool showVoiceInfo = false;
  std::wstring voiceName = L"Sam";
  int pitch = 0;
  int speed = 0;
  std::wstring text;
  std::wstring outputFilename;
};

static void showHelp() {
  Log::info("sam - MS TTS4 client.");
  Log::info("Usage: samd [options] {text}...");
  Log::info("Options:");
  Log::info("-h        Show help.");
  Log::info("-l        List voices.");
  Log::info("-i        Show information for the selected voice.");
  Log::info("-v <name> Select voice.");
  Log::info("-p <num>  Set voice pitch.");
  Log::info("-s <num>  Set voice speed.");
  Log::info("-f <file> Set an input file to read.");
  Log::info("-o <file> Write output to a file instead of playing directly.");
}

static bool parseArgs(int argc, wchar_t *argv[], CommandLine &commandLine) {
  bool parseOptions = true;

  for (int i = 1; i < argc; ++i) {
    std::wstring_view arg = argv[i];
    auto len = arg.length();
    if (!len) {
      continue;
    }

    if ((arg[0] == '-' || arg[0] == '/') && parseOptions) {
      switch (arg[1]) {
        case 'h':
          commandLine.showHelp = true;
          return true;
        case 'l':
          commandLine.listVoices = true;
          return true;
        case 'i':
          commandLine.showVoiceInfo = true;
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
            commandLine.text.reserve(input.tellg());
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
        case '-':
          if (arg[0] == '-') {
            parseOptions = false;
            break;
          }
          [[fallthrough]];
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
  ScopedSpdlog _scopedSpdlog;
  Log::g_logger->set_level(spdlog::level::trace);

  CommandLine commandLine;
  if (!parseArgs(argc, argv, commandLine)) {
    return 1;
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
  }

  NotifySink sink;
  tts.say(commandLine.text, sink, (WORD)commandLine.pitch,
          (DWORD)commandLine.speed);

  return 0;
}

