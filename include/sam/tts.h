#include "sam.h"

#include <speech.h>
#include <string_view>

const WORD RealTime = (WORD)-1;

class NotifySink : public ITTSNotifySink {
public:
  explicit NotifySink() noexcept;
  virtual ~NotifySink();

  STDMETHOD( QueryInterface)(REFIID riid, LPVOID *ppv);
  STDMETHOD_(ULONG,  AddRef)();
  STDMETHOD_(ULONG, Release)();

  STDMETHOD(AttribChanged)(DWORD);
  STDMETHOD   (AudioStart)(QWORD);
  STDMETHOD    (AudioStop)(QWORD);
  STDMETHOD       (Visual)(QWORD, WCHAR, WCHAR, DWORD, PTTSMOUTH);

  const HANDLE &finishEvent() const {
    return _finishEvent;
  }

  bool finished() const {
    switch (auto r = WaitForSingleObject(_finishEvent, 0)) {
      case WAIT_OBJECT_0:
        return true;
      case WAIT_TIMEOUT:
        return false;
      default:
        Log::error("Event wait error: WaitForSingleObject returned {} (0x{:X}), GetLastError() = 0x{:X}.", r, r, GetLastError());
        return true;
    }
  }

  bool wait(DWORD milliseconds = INFINITE) const {
    switch (WaitForSingleObject(_finishEvent, milliseconds)) {
      case WAIT_OBJECT_0:
      case WAIT_TIMEOUT:
        return true;
      default:
        Log::error("Event wait error");
        return false;
    }
  }

private:
  ULONG _refcnt = 1;
  HANDLE _finishEvent;
};

struct TTSContainer {
  TTSContainer() = default;
  TTSContainer(const TTSContainer &) = delete;
  TTSContainer &operator=(const TTSContainer &) = delete;
  ~TTSContainer();

  /**
   * \param name Voice name.
   * \param outputFilename Output filename, or empty to play through the
   *        speakers.
   */
  HRESULT init(std::wstring_view name, std::wstring_view outputFilename);
  HRESULT listVoices() const;
  void say(std::wstring_view text, NotifySink &sink,
           WORD pitch = 0, DWORD speed = 0);

  PITTSCENTRAL ttsCentral() const {
    return _ttsCentral;
  }

  PITTSATTRIBUTES ttsAttributes() const {
    return _ttsAttributes;
  }

  IUnknown *output() const {
    return _output;
  }

  bool isFileOutput() const {
    return !_outputFilename.empty();
  }

  /// \return a tuple of <c>[min pitch, max pitch, default pitch]</c>.
  std::tuple<WORD, WORD, WORD> pitchInfo() const {
    return std::make_tuple(_minPitch, _maxPitch, _defaultPitch);
  }

  /// \return a tuple of <c>[min speed, max speed, default speed]</c>.
  std::tuple<DWORD, DWORD, DWORD> speedInfo() const {
    return std::make_tuple(_minSpeed, _maxSpeed, _defaultSpeed);
  }

  explicit operator bool() const {
    return _ttsCentral && _ttsAttributes && _output;
  }

private:
  PITTSFIND _ttsFind = nullptr;
  PITTSCENTRAL _ttsCentral = nullptr;
  PITTSATTRIBUTES _ttsAttributes = nullptr;
  IUnknown *_output = nullptr;
  std::wstring _outputFilename;

  WORD _defaultPitch;
  WORD _minPitch;
  WORD _maxPitch;
  DWORD _defaultSpeed;
  DWORD _minSpeed;
  DWORD _maxSpeed;
};

