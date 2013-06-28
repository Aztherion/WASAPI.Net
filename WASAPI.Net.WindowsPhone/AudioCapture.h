#pragma once
#include "windows.h"
#include <synchapi.h>
#include <audioclient.h>
#include <phoneaudioclient.h>

#define DEFAULT_BUFFER_SIZE 1024*128
#define MAX_BUFFER_SIZE 1000*1000*10

namespace WASAPI_Net_WindowsPhone
{
	public delegate void BufferReadyEventHandler(Windows::Storage::Streams::IBuffer^ pBuffer);

	public ref class AudioCapture sealed
	{
	public:
											AudioCapture();
		virtual								~AudioCapture();

		void								Start();
		void								Stop();
		void								SetBufferSize(UINT32 size);

		event BufferReadyEventHandler^		BufferReady;

	private:
		HRESULT								InitCapture(UINT32 latency);
		HRESULT								StartAudioThread();
		void								CaptureThread(Windows::Foundation::IAsyncAction^ operation);
		void								Write(BYTE* bytes, int length);

		WAVEFORMATEX*						m_pwfx;
		int									m_frameSizeInBytes;
		IAudioClient2*						m_pAudioClient;
		IAudioCaptureClient*				m_pCaptureClient;

		HANDLE								m_hCaptureEvent;
		HANDLE								m_hShutdownEvent;
		Windows::Foundation::IAsyncAction^	m_captureThread;
		bool								m_started;
		UINT32								m_bufferSize;
	};
}