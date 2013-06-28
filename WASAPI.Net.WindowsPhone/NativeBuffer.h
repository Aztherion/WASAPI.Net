#pragma once
#include "windows.h"
#include <robuffer.h>
#include <wrl.h>
#include <wrl/implements.h>
#include <wrl/client.h>
#include <windows.storage.streams.h>

using namespace Microsoft::WRL;

namespace WASAPI_Net_WindowsPhone
{
	class NativeBuffer : public Microsoft::WRL::RuntimeClass<
		Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::WinRtClassicComMix>,
		ABI::Windows::Storage::Streams::IBuffer,
		Windows::Storage::Streams::IBufferByteAccess,
		Microsoft::WRL::FtmBase>
	{
	public:
		virtual ~NativeBuffer(){}
		STDMETHODIMP RuntimeClassInitialize(BYTE* buffer, UINT length)
		{
			m_buffer = buffer;
			m_length = length;
			return S_OK;
		}

		STDMETHODIMP Buffer(BYTE** value)
		{
			*value = m_buffer;
			return S_OK;
		}

		STDMETHODIMP get_Capacity(UINT32* value)
		{
			*value = m_length;
			return S_OK;
		}

		STDMETHODIMP get_Length(UINT32* value)
		{
			*value = m_length;
			return S_OK;
		}

		STDMETHODIMP put_Length(UINT32 value)
		{
			m_length = value;
			return S_OK;
		}

		static Windows::Storage::Streams::IBuffer^ AsIBuffer(ComPtr<NativeBuffer> spNativeBuffer)
		{
			auto iinspectable = reinterpret_cast<IInspectable*>(spNativeBuffer.Get());
			return reinterpret_cast<Windows::Storage::Streams::IBuffer^>(iinspectable);
		}

	private:
		UINT32 m_length;
		BYTE* m_buffer;
	};
}