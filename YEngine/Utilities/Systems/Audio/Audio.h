#pragma once

// C++
#include <xaudio2.h>
#include <wrl/client.h>
#include <fstream>
#include <vector>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <cassert>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib,"xaudio2.lib")

///************************* サウンド管理クラス *************************///
///
/// XAudio2 と Media Foundation を使用して  
/// wav / mp3 / mp4 形式の音声を再生・制御する
///
namespace YoRigine {
	class Audio {
	public:
		///************************* シングルトン *************************///

		static Audio* GetInstance();
		static void Finalize();

	public:
		///************************* 初期化と終了 *************************///

		void Initialize();
		void FinalizeAudio();

	public:
		///************************* 構造体定義 *************************///

		struct ChunkHeader {
			char id[4];
			int32_t size;
		};

		struct RiffHeader {
			ChunkHeader chunk;
			char type[4];
		};

		struct FormatChunk {
			ChunkHeader chunk;
			WAVEFORMATEX fmt;
		};

		struct SoundData {
			WAVEFORMATEX wfex;
			BYTE* pBuffer;
			unsigned int bufferSize;

			SoundData() : wfex{}, pBuffer(nullptr), bufferSize(0) {}
			SoundData(const SoundData&) = delete;
			SoundData& operator=(const SoundData&) = delete;

			SoundData(SoundData&& other) noexcept
				: wfex(other.wfex), pBuffer(other.pBuffer), bufferSize(other.bufferSize) {
				other.pBuffer = nullptr;
				other.bufferSize = 0;
				ZeroMemory(&other.wfex, sizeof(WAVEFORMATEX));
			}

			SoundData& operator=(SoundData&& other) noexcept {
				if (this != &other) {
					if (pBuffer) {
						delete[] pBuffer;
					}
					wfex = other.wfex;
					pBuffer = other.pBuffer;
					bufferSize = other.bufferSize;
					other.pBuffer = nullptr;
					other.bufferSize = 0;
					ZeroMemory(&other.wfex, sizeof(WAVEFORMATEX));
				}
				return *this;
			}

			~SoundData() {
				if (pBuffer) {
					delete[] pBuffer;
					pBuffer = nullptr;
				}
			}
		};

	public:
		///************************* 読み込み処理 *************************///

		SoundData LoadWave(const char* filename);
		SoundData LoadAudio(const wchar_t* filename);
		void SoundUnload(SoundData* soundData);

	public:
		///************************* 再生処理 *************************///

		IXAudio2SourceVoice* SoundPlayWave(const SoundData& soundData);
		IXAudio2SourceVoice* SoundPlayAudio(const SoundData& soundData);
		void SetVolume(IXAudio2SourceVoice* pSourceVoice, float volume);
		void StopAndDestroyVoice(IXAudio2SourceVoice* pSourceVoice);

	private:
		///************************* シングルトン内部 *************************///

		static Audio* instance;
		Audio();
		~Audio();

	private:
		///************************* メンバ変数 *************************///

		IXAudio2* xAudio2_;
		IXAudio2MasteringVoice* masterVoice_;
		HRESULT hr_;
		bool mediaFoundationInitialized_;
	};
}