#include "pch.h"
#include "AudioCapture.h"
#include "ApiLock.h"
#include "NativeBuffer.h"

using namespace WASAPI_Net_WindowsPhone;
using namespace Windows::System::Threading;

void FillPcmFormat(WAVEFORMATEX& format, WORD wChannels, int nSampleRate, WORD wBits)
{
    format.wFormatTag        = WAVE_FORMAT_PCM;
    format.nChannels         = wChannels;
    format.nSamplesPerSec    = nSampleRate;
    format.wBitsPerSample    = wBits;
    format.nBlockAlign       = format.nChannels * (format.wBitsPerSample / 8);
    format.nAvgBytesPerSec   = format.nSamplesPerSec * format.nBlockAlign;
    format.cbSize            = 0;
}

AudioCapture::AudioCapture() :
	m_pwfx(NULL),
	m_pAudioClient(NULL),
	m_pCaptureClient(NULL),
	m_hCaptureEvent(NULL),
	m_captureThread(nullptr),
	m_hShutdownEvent(NULL),
	m_frameSizeInBytes(0),
	m_started(false),
	m_bufferSize(DEFAULT_BUFFER_SIZE){}

AudioCapture::~AudioCapture(){}

void AudioCapture::Start()
{
	// there can be only one
	LONG desiredLatency = 30;
	std::lock_guard<std::recursive_mutex> lock(g_apiLock);
	
	if (m_started) return;

	HRESULT hr = InitCapture(desiredLatency);
	if (SUCCEEDED(hr))
	{
		hr = StartAudioThread();
	}

	if (SUCCEEDED(hr))
	{
		HRESULT hr = m_pAudioClient->Start();
	}

	if (FAILED(hr))
	{
		Stop();
		throw ref new Platform::COMException(hr, L"Unable to start audio capture");
	}

	m_started = true;
}

void AudioCapture::Stop()
{
	std::lock_guard<std::recursive_mutex> lock(g_apiLock);
	if (m_hShutdownEvent)
	{
		SetEvent(m_hShutdownEvent);
	}

	if (m_captureThread != nullptr)
	{
		m_captureThread->Cancel();
		m_captureThread->Close();
		m_captureThread = nullptr;
	}

	if (m_pAudioClient)
	{
		m_pAudioClient->Stop();
	}

	if (m_pCaptureClient)
	{
		m_pCaptureClient->Release();
		m_pCaptureClient = NULL;
	}

	if (m_pAudioClient)
	{
		m_pAudioClient->Release();
		m_pAudioClient = NULL;
	}

	if (m_pwfx)
	{
		CoTaskMemFree((LPVOID)m_pwfx);
		m_pwfx = NULL;
	}

	if (m_hCaptureEvent)
	{
		CloseHandle(m_hCaptureEvent);
		m_hCaptureEvent = NULL;
	}

	if (m_hShutdownEvent)
	{
		CloseHandle(m_hShutdownEvent);
		m_hShutdownEvent = NULL;
	}

	m_started = false;
}

void AudioCapture::SetBufferSize(UINT32 size)
{
	if (m_started) throw ref new Platform::Exception(E_FAIL, L"Buffer size must be set before Start is called.");
	if (size > MAX_BUFFER_SIZE) throw ref new Platform::Exception(E_INVALIDARG, L"Buffer size must be smaller than 10MB");
	m_bufferSize = size;
}

HRESULT AudioCapture::InitCapture(UINT32 latency)
{
	HRESULT hr = E_FAIL;

	LPCWSTR pwstrDefaultCaptureDeviceId = GetDefaultAudioCaptureId(AudioDeviceRole::Communications);
	if (pwstrDefaultCaptureDeviceId == NULL)
	{
		hr = E_FAIL;
	}

	hr = ActivateAudioInterface(pwstrDefaultCaptureDeviceId, __uuidof(IAudioClient2), (void**)&m_pAudioClient);
	if (SUCCEEDED(hr))
	{
		hr = m_pAudioClient->GetMixFormat(&m_pwfx);
	}

	if (SUCCEEDED(hr))
	{
		m_frameSizeInBytes = (m_pwfx->wBitsPerSample / 8) * m_pwfx->nChannels;
	}

	hr = m_pAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_NOPERSIST | AUDCLNT_STREAMFLAGS_EVENTCALLBACK, latency * 10000, 0, m_pwfx, NULL);
	
	if (SUCCEEDED(hr))
	{
		m_hCaptureEvent = CreateEventEx(NULL, NULL, 0, EVENT_ALL_ACCESS);
		if (m_hCaptureEvent == NULL)
		{
			hr = HRESULT_FROM_WIN32(GetLastError());
		}
	}

	if (SUCCEEDED(hr))
	{
		hr = m_pAudioClient->SetEventHandle(m_hCaptureEvent);
	}
	
	if (SUCCEEDED(hr))
	{
		hr = m_pAudioClient->GetService(__uuidof(IAudioCaptureClient), (void**)&m_pCaptureClient);
	}

	if (pwstrDefaultCaptureDeviceId)
	{
		CoTaskMemFree((LPVOID)pwstrDefaultCaptureDeviceId);
	}
	return hr;
}

HRESULT AudioCapture::StartAudioThread()
{
	m_hShutdownEvent = CreateEventEx(NULL, NULL, CREATE_EVENT_MANUAL_RESET, EVENT_ALL_ACCESS);
	if (!m_hShutdownEvent)
	{
		return HRESULT_FROM_WIN32(GetLastError());
	}
	m_captureThread = ThreadPool::RunAsync(ref new WorkItemHandler(this, &AudioCapture::CaptureThread), WorkItemPriority::High, WorkItemOptions::TimeSliced);
	return S_OK;
}

void AudioCapture::CaptureThread(Windows::Foundation::IAsyncAction^ operation)
{
	bool capturing = true;
	BYTE* pBuffer = new BYTE[m_bufferSize];
	HANDLE eventHandles[] = {
							m_hShutdownEvent,
							m_hCaptureEvent
							};
	unsigned int uAccumulatedBytes = 0;
	while(capturing)
	{
		DWORD waitResult = WaitForMultipleObjectsEx(SIZEOF_ARRAY(eventHandles), eventHandles, FALSE, INFINITE, FALSE);
		switch(waitResult)
		{
		case WAIT_OBJECT_0: // shutdownevent
			capturing = false;
			break;
		case WAIT_OBJECT_0+1:
			BYTE* pData = nullptr;
			UINT32 framesAvailable = 0;
			DWORD flags = 0;
			UINT64 captureStartTime;
			HRESULT hr = m_pCaptureClient->GetBuffer(&pData, &framesAvailable, &flags, nullptr, &captureStartTime);
			if (SUCCEEDED(hr))
			{
				unsigned int size = framesAvailable * m_frameSizeInBytes;
				if (flags & AUDCLNT_BUFFERFLAGS_SILENT)
				{
					break;
				} 
				if (m_bufferSize - uAccumulatedBytes < size)
				{
					Write(pBuffer, uAccumulatedBytes);
					uAccumulatedBytes = 0;
				}
				memcpy(pBuffer + uAccumulatedBytes, pData, size);
				uAccumulatedBytes += size;
				hr = m_pCaptureClient->ReleaseBuffer(framesAvailable);
			}
			break;
		}
	}
}

void AudioCapture::Write(BYTE* bytes, int length)
{
	ComPtr<NativeBuffer> spNativeBuffer = NULL;
	MakeAndInitialize<NativeBuffer>(&spNativeBuffer, bytes, length);
	BufferReady(NativeBuffer::AsIBuffer(spNativeBuffer));
}


