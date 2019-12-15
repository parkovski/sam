#include "sam/tts.h"

NotifySink::NotifySink() noexcept
{
  _finishEvent = CreateEvent(nullptr, true, true, nullptr);
}

NotifySink::~NotifySink() {
  CloseHandle(_finishEvent);
}

HRESULT NotifySink::QueryInterface(REFIID riid, LPVOID *ppv) {
  *ppv = nullptr;
  if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITTSNotifySink)) {
    *ppv = (LPVOID)this;
    return S_OK;
  }
  return E_NOINTERFACE;
}

ULONG NotifySink::AddRef() {
  return ++_refcnt;
}

ULONG NotifySink::Release() {
  if (_refcnt > 1) {
    return --_refcnt;
  }
  _refcnt = 0;
  delete this;
  return 0;
}

HRESULT NotifySink::AttribChanged(DWORD) {
  return S_OK;
}

HRESULT NotifySink::AudioStart(QWORD) {
  ResetEvent(_finishEvent);
  return S_OK;
}

HRESULT NotifySink::AudioStop(QWORD) {
  SetEvent(_finishEvent);
  PostQuitMessage(0);
  return S_OK;
}

HRESULT NotifySink::Visual(QWORD, WCHAR, WCHAR, DWORD, PTTSMOUTH) {
  return S_OK;
}

HRESULT TTSContainer::init(std::wstring_view name,
                           std::wstring_view outputFilename)
{
  HRESULT hr;

  TTSMODEINFO ttsModeInfo;
  TTSMODEINFO ttsResult;

  memset(&ttsModeInfo, 0, sizeof(TTSMODEINFO));
  auto count = name.length() * sizeof(wchar_t);
  if (count > TTSI_NAMELEN - sizeof(wchar_t)) {
    count = TTSI_NAMELEN - sizeof(wchar_t);
  }
  memcpy(ttsModeInfo.szModeName, name.data(), count);
  ttsModeInfo.szModeName[count] = 0;

  hr = CoCreateInstance(CLSID_TTSEnumerator, nullptr, CLSCTX_ALL,
                        IID_ITTSFind, (void**)&_ttsFind);
  if (FAILED(hr)) {
    Log::error("Failed creating ITTSFind");
    return hr;
  }

  hr = _ttsFind->Find(&ttsModeInfo, nullptr, &ttsResult);
  if (FAILED(hr)) {
    Log::error(L"Failed finding voice {}.", name);
  }

  if (!outputFilename.empty()) {
    Log::debug("Creating audio file output.");

    _outputFilename = outputFilename;
    IAudioFile *audioFile;
    hr = CoCreateInstance(CLSID_AudioDestFile, nullptr, CLSCTX_ALL,
                          IID_IAudioFile, (void**)&audioFile);
    if (FAILED(hr)) {
      Log::error("Failed creating IAudioFile.");
      return hr;
    }

    audioFile->RealTimeSet(RealTime);

    _output = audioFile;
  } else {
    Log::debug("Creating multimedia device output.");

    IAudioMultiMediaDevice *mmdevice;
    hr = CoCreateInstance (CLSID_MMAudioDest, NULL, CLSCTX_ALL,
                           IID_IAudioMultiMediaDevice, (void**)&mmdevice);
    if (FAILED(hr)) {
      Log::error("Failed creating IAudioMultiMediaDevice.");
      return hr;
    }

    hr = mmdevice->DeviceNumSet(0XFFFFFFFF);
    if (FAILED(hr)) {
      Log::error("Failed setting mmdevice number");
      return hr;
    }

    // some engines will leak an audio destination object
    // but if release they crash
    // mmdevice->AddRef();

    _output = mmdevice;
  }

  hr = _ttsFind->Select(ttsResult.gModeID, &_ttsCentral, _output);
  if (FAILED(hr)) {
    Log::error("Failed selecting audio file/device.");
    return hr;
  }

  hr = _ttsCentral->QueryInterface(IID_ITTSAttributes, (void**)&_ttsAttributes);
  if (FAILED(hr)) {
    Log::error("Failed to get ITTSAttributes.");
    return hr;
  }

  _ttsAttributes->PitchGet(&_defaultPitch);
  _ttsAttributes->SpeedGet(&_defaultSpeed);
  _ttsAttributes->PitchSet(TTSATTR_MINPITCH);
  _ttsAttributes->SpeedSet(TTSATTR_MINSPEED);
  _ttsAttributes->PitchGet(&_minPitch);
  _ttsAttributes->SpeedGet(&_minSpeed);
  _ttsAttributes->PitchSet(TTSATTR_MAXPITCH);
  _ttsAttributes->SpeedSet(TTSATTR_MAXSPEED);
  _ttsAttributes->PitchGet(&_maxPitch);
  _ttsAttributes->SpeedGet(&_maxSpeed);

  return true;
}

HRESULT TTSContainer::listVoices() const {
  PITTSENUM ttsEnum;
  HRESULT hr;

  hr = CoCreateInstance(CLSID_TTSEnumerator, nullptr, CLSCTX_ALL,
                        IID_ITTSEnum, (void**)&ttsEnum);
  if (FAILED(hr)) {
    Log::error("Failed to create ITTSEnum.");
    return hr;
  }

  Log::trace("Enumerating voices:");
  TTSMODEINFO ttsInfo;
  while (!ttsEnum->Next(1, &ttsInfo, nullptr)) {
    Log::info(L"Speaker: {}; Mode: {}", ttsInfo.szSpeaker, ttsInfo.szModeName);
  }
  ttsEnum->Release();

  return S_OK;
}

void TTSContainer::say(std::wstring_view text, NotifySink &sink,
                       WORD pitch, DWORD speed)
{
  if (pitch == 0) {
    pitch = _defaultPitch;
  } else if (pitch < _minPitch) {
    pitch = _minPitch;
  } else if (pitch > _maxPitch) {
    pitch = _maxPitch;
  }

  if (speed == 0) {
    speed = _defaultSpeed;
  } else if (speed < _minSpeed) {
    speed = _minSpeed;
  } else if (speed > _maxSpeed) {
    speed = _maxSpeed;
  }

  DWORD dwRegKey;
  _ttsCentral->Register((void*)&sink, IID_ITTSNotifySink, &dwRegKey);
  if (isFileOutput()) {
    Log::debug(L"Writing to {}.", _outputFilename);
    static_cast<IAudioFile *>(_output)->Set(_outputFilename.c_str(), 1);
  }

  ResetEvent(sink.finishEvent());
  _ttsAttributes->PitchSet(pitch);
  _ttsAttributes->SpeedSet(speed);
  _ttsCentral->AudioReset();
  SDATA data;
  data.dwSize = text.length() * sizeof(wchar_t);
  data.pData = (void*)text.data();
  _ttsCentral->TextData(CHARSET_TEXT, TTSDATAFLAG_TAGGED, data,
                        nullptr, IID_ITTSBufNotifySink);

  MSG msg;
  while (GetMessage(&msg, nullptr, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
  if (isFileOutput()) {
    static_cast<IAudioFile *>(_output)->Flush();
  }
}

TTSContainer::~TTSContainer() {
  if (_ttsFind) {
    _ttsFind->Release();
  }
  if (_output) {
    _output->Release();
  }
  if (_ttsAttributes) {
    _ttsAttributes->Release();
  }
  if (_ttsCentral) {
    _ttsCentral->Release();
  }
}
