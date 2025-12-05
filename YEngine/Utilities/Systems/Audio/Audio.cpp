#include "Audio.h"
#include <Windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <cassert>

namespace YoRigine {
	// シングルトンの初期化
	Audio* Audio::instance = nullptr;

	Audio* Audio::GetInstance()
	{
		if (instance == nullptr) {
			instance = new Audio;
		}
		return instance;
	}

	void Audio::Finalize()
	{
		if (instance) {
			instance->FinalizeAudio();
			delete instance;
			instance = nullptr;
		}
	}

	Audio::Audio()
		: xAudio2_(nullptr),
		masterVoice_(nullptr),
		hr_(S_OK),
		mediaFoundationInitialized_(false)
	{
	}

	Audio::~Audio()
	{
		FinalizeAudio();
	}

	void Audio::Initialize()
	{
		// COM の初期化
		HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
		if (FAILED(hr)) {
			assert(SUCCEEDED(hr) && "Failed to initialize COM");
			return;
		}

		// Media Foundation の初期化
		if (!mediaFoundationInitialized_) {
			hr_ = MFStartup(MF_VERSION);
			assert(SUCCEEDED(hr_) && "Failed to initialize Media Foundation");
			mediaFoundationInitialized_ = true;
		}

		// XAudio2 の初期化
		hr_ = XAudio2Create(&xAudio2_, 0, XAUDIO2_DEFAULT_PROCESSOR);
		assert(SUCCEEDED(hr_) && "Failed to initialize XAudio2");

		// マスターボイスの作成
		hr_ = xAudio2_->CreateMasteringVoice(&masterVoice_);
		assert(SUCCEEDED(hr_) && "Failed to create mastering voice");
	}

	void Audio::FinalizeAudio()
	{
		// マスターボイスの破棄
		if (masterVoice_) {
			masterVoice_->DestroyVoice();
			masterVoice_ = nullptr;
		}

		// XAudio2 のシャットダウン
		if (xAudio2_) {
			xAudio2_->StopEngine();
			xAudio2_->Release();
			xAudio2_ = nullptr;
		}

		// Media Foundation のシャットダウン
		if (mediaFoundationInitialized_) {
			MFShutdown();
			mediaFoundationInitialized_ = false;
		}

		// COM のクリーンアップ
		CoUninitialize();
	}

	Audio::SoundData Audio::LoadAudio(const wchar_t* filename)
	{
		SoundData soundData = {};
		IMFSourceReader* pReader = nullptr;
		IMFMediaType* pAudioType = nullptr;
		IMFMediaType* pActualType = nullptr;

		HRESULT hr = MFCreateSourceReaderFromURL(filename, nullptr, &pReader);
		if (FAILED(hr)) {
			assert(false && "Failed to create source reader");
			return soundData;
		}

		// PCM フォーマットを設定
		hr = MFCreateMediaType(&pAudioType);
		if (FAILED(hr)) {
			pReader->Release();
			assert(false && "Failed to create media type");
			return soundData;
		}

		hr = pAudioType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
		if (FAILED(hr)) {
			pAudioType->Release();
			pReader->Release();
			assert(false);
			return soundData;
		}

		hr = pAudioType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
		if (FAILED(hr)) {
			pAudioType->Release();
			pReader->Release();
			assert(false);
			return soundData;
		}

		hr = pReader->SetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), nullptr, pAudioType);
		pAudioType->Release();
		pAudioType = nullptr;

		if (FAILED(hr)) {
			pReader->Release();
			assert(false && "Failed to set media type to PCM");
			return soundData;
		}

		// メディアタイプから WAVEFORMATEX を取得
		hr = pReader->GetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), &pActualType);
		if (FAILED(hr)) {
			pReader->Release();
			assert(false && "Failed to get current media type");
			return soundData;
		}

		// GetWaveFormatEx を MFCreateWaveFormatExFromMFMediaType に置き換え
		WAVEFORMATEX* pWfx = nullptr;
		hr = MFCreateWaveFormatExFromMFMediaType(pActualType, &pWfx, nullptr);
		pActualType->Release();
		pActualType = nullptr;

		if (FAILED(hr)) {
			pReader->Release();
			assert(false && "Failed to create WaveFormatEx from IMFMediaType");
			return soundData;
		}

		// コピーして SoundData に設定
		memcpy(&soundData.wfex, pWfx, sizeof(WAVEFORMATEX));
		CoTaskMemFree(pWfx); // MFCreateWaveFormatExFromMFMediaType で確保されたメモリを解放
		pWfx = nullptr;

		// バッファ用のベクターを準備
		std::vector<BYTE> bufferData;

		// データの読み取り
		while (true)
		{
			DWORD dwFlags = 0;
			IMFSample* pSample = nullptr;
			hr = pReader->ReadSample(
				static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM),
				0,
				nullptr,
				&dwFlags,
				nullptr,
				&pSample
			);

			if (FAILED(hr)) {
				pReader->Release();
				// バッファデータがある場合は解放
				if (!bufferData.empty() && soundData.pBuffer) {
					delete[] soundData.pBuffer;
					soundData.pBuffer = nullptr;
					soundData.bufferSize = 0;
				}
				assert(false && "Failed to read sample");
				return soundData;
			}

			if (dwFlags & MF_SOURCE_READERF_ENDOFSTREAM) {
				break;
			}

			if (pSample) {
				IMFMediaBuffer* pBuffer = nullptr;
				hr = pSample->ConvertToContiguousBuffer(&pBuffer);
				if (FAILED(hr)) {
					pSample->Release();
					pReader->Release();
					assert(false && "Failed to convert sample to contiguous buffer");
					return soundData;
				}

				BYTE* pData = nullptr;
				DWORD maxLength = 0, currentLength = 0;
				hr = pBuffer->Lock(&pData, &maxLength, &currentLength);
				if (FAILED(hr)) {
					pBuffer->Release();
					pSample->Release();
					pReader->Release();
					assert(false && "Failed to lock buffer");
					return soundData;
				}

				// バッファにデータを追加
				bufferData.insert(bufferData.end(), pData, pData + currentLength);

				pBuffer->Unlock();
				pBuffer->Release();
				pSample->Release();
			}
		}

		pReader->Release();
		pReader = nullptr;

		// バッファのサイズとデータを SoundData に設定
		if (!bufferData.empty()) {
			soundData.bufferSize = static_cast<DWORD>(bufferData.size());
			soundData.pBuffer = new BYTE[soundData.bufferSize];
			memcpy(soundData.pBuffer, bufferData.data(), soundData.bufferSize);
		}

		return soundData;
	}

	void Audio::StopAndDestroyVoice(IXAudio2SourceVoice* pSourceVoice)
	{
		if (pSourceVoice) {
			pSourceVoice->Stop();
			pSourceVoice->DestroyVoice();
		}
	}

	void Audio::SoundUnload(SoundData* soundData)
	{
		if (soundData && soundData->pBuffer) {
			delete[] soundData->pBuffer;
			soundData->pBuffer = nullptr;
			soundData->bufferSize = 0;
			ZeroMemory(&soundData->wfex, sizeof(WAVEFORMATEX));
		}
	}

	IXAudio2SourceVoice* Audio::SoundPlayAudio(const SoundData& soundData)
	{
		if (!soundData.pBuffer || soundData.bufferSize == 0) {
			return nullptr;
		}

		HRESULT hr;

		// SourceVoice の作成
		IXAudio2SourceVoice* pSourceVoice = nullptr;
		hr = xAudio2_->CreateSourceVoice(&pSourceVoice, &soundData.wfex);
		if (FAILED(hr)) {
			assert(false && "Failed to create source voice");
			return nullptr;
		}

		// XAUDIO2_BUFFER の設定
		XAUDIO2_BUFFER buf = {};
		buf.pAudioData = soundData.pBuffer;
		buf.AudioBytes = soundData.bufferSize;
		buf.Flags = XAUDIO2_END_OF_STREAM;

		// バッファの送信
		hr = pSourceVoice->SubmitSourceBuffer(&buf);
		if (FAILED(hr)) {
			pSourceVoice->DestroyVoice();
			assert(false && "Failed to submit source buffer");
			return nullptr;
		}

		// 再生開始
		hr = pSourceVoice->Start();
		if (FAILED(hr)) {
			pSourceVoice->DestroyVoice();
			assert(false && "Failed to start source voice");
			return nullptr;
		}

		return pSourceVoice;
	}

	IXAudio2SourceVoice* Audio::SoundPlayWave(const SoundData& soundData)
	{
		if (!soundData.pBuffer || soundData.bufferSize == 0) {
			return nullptr;
		}

		HRESULT hr;

		// SourceVoice の作成
		IXAudio2SourceVoice* pSourceVoice = nullptr;
		hr = xAudio2_->CreateSourceVoice(&pSourceVoice, &soundData.wfex);
		if (FAILED(hr)) {
			assert(false && "Failed to create source voice");
			return nullptr;
		}

		// XAUDIO2_BUFFER の設定
		XAUDIO2_BUFFER buf = {};
		buf.pAudioData = soundData.pBuffer;
		buf.AudioBytes = soundData.bufferSize;
		buf.Flags = XAUDIO2_END_OF_STREAM;

		// バッファの送信
		hr = pSourceVoice->SubmitSourceBuffer(&buf);
		if (FAILED(hr)) {
			pSourceVoice->DestroyVoice();
			assert(false && "Failed to submit source buffer");
			return nullptr;
		}

		// 再生開始
		hr = pSourceVoice->Start();
		if (FAILED(hr)) {
			pSourceVoice->DestroyVoice();
			assert(false && "Failed to start source voice");
			return nullptr;
		}

		return pSourceVoice;
	}

	void Audio::SetVolume(IXAudio2SourceVoice* pSourceVoice, float volume)
	{
		if (pSourceVoice) {
			// 0.0f ～ 1.0f の範囲で音量を設定
			HRESULT hr = pSourceVoice->SetVolume(volume);
			if (FAILED(hr)) {
				assert(SUCCEEDED(hr) && "Failed to set volume");
			}
		}
	}

	Audio::SoundData Audio::LoadWave(const char* filename)
	{
		SoundData soundData = {};

		// ファイル入力ストリームのインスタンス
		std::ifstream file;
		// .wavファイルをバイナリモードで開く
		file.open(filename, std::ios_base::binary);
		// ファイルオープン失敗を検出
		if (!file.is_open()) {
			assert(false && "Failed to open wave file");
			return soundData;
		}

		// RIFFヘッダーの読み込み
		RiffHeader riff;
		file.read((char*)&riff, sizeof(riff));
		if (!file || file.gcount() != sizeof(riff)) {
			file.close();
			assert(false && "Failed to read RIFF header");
			return soundData;
		}

		// ファイルがRIFFかチェック
		if (strncmp(riff.chunk.id, "RIFF", 4) != 0) {
			file.close();
			assert(false && "Not a RIFF file");
			return soundData;
		}
		// タイプがWAVEかチェック
		if (strncmp(riff.type, "WAVE", 4) != 0) {
			file.close();
			assert(false && "Not a WAVE file");
			return soundData;
		}

		// チャンクのループを開始
		ChunkHeader chunkHeader;
		FormatChunk format = {};
		bool formatFound = false;

		while (file.read((char*)&chunkHeader, sizeof(chunkHeader))) {
			if (!file || file.gcount() != sizeof(chunkHeader)) {
				break;
			}

			// チャンクIDが "fmt " かを確認
			if (strncmp(chunkHeader.id, "fmt ", 4) == 0) {
				// Formatチャンクのサイズを確認し、データを読み込む
				if (chunkHeader.size > sizeof(format.fmt)) {
					file.close();
					assert(false && "Format chunk size too large");
					return soundData;
				}
				format.chunk = chunkHeader; // チャンクヘッダーをコピー
				file.read((char*)&format.fmt, chunkHeader.size); // fmtのデータを読み込み
				if (!file || file.gcount() != chunkHeader.size) {
					file.close();
					assert(false && "Failed to read format chunk");
					return soundData;
				}
				formatFound = true;
				break;
			} else {
				// 次のチャンクに移動
				file.seekg(chunkHeader.size, std::ios_base::cur);
				if (!file) {
					break;
				}
			}
		}

		// "fmt "チャンクが見つからなかった場合のエラー処理
		if (!formatFound) {
			file.close();
			assert(false && "'fmt ' chunk not found");
			return soundData;
		}

		// Dataチャンクの読み込み
		ChunkHeader data;
		while (file.read((char*)&data, sizeof(data))) {
			if (!file || file.gcount() != sizeof(data)) {
				file.close();
				assert(false && "Failed to read chunk header");
				return soundData;
			}

			// JUNKチャンクを検出した場合
			if (strncmp(data.id, "JUNK", 4) == 0) {
				// 読み取り位置をJUNKチャンクの終わりまで進める
				file.seekg(data.size, std::ios_base::cur);
				if (!file) {
					file.close();
					assert(false && "Failed to skip JUNK chunk");
					return soundData;
				}
				continue; // 次のチャンクを読む
			}

			if (strncmp(data.id, "data", 4) == 0) {
				break; // dataチャンクが見つかった
			}

			// その他のチャンクをスキップ
			file.seekg(data.size, std::ios_base::cur);
			if (!file) {
				file.close();
				assert(false && "Failed to skip chunk");
				return soundData;
			}
		}

		if (strncmp(data.id, "data", 4) != 0) {
			file.close();
			assert(false && "Data chunk not found");
			return soundData;
		}

		// サイズチェック
		if (data.size == 0 || data.size > 100 * 1024 * 1024) { // 100MB制限
			file.close();
			assert(false && "Invalid data size");
			return soundData;
		}

		// Dataチャンクのデータ部（波形データの読み込み）
		char* pBuffer = new(std::nothrow) char[data.size];
		if (!pBuffer) {
			file.close();
			assert(false && "Memory allocation failed");
			return soundData;
		}

		file.read(pBuffer, data.size);
		if (!file || file.gcount() != data.size) {
			delete[] pBuffer;
			file.close();
			assert(false && "Failed to read wave data");
			return soundData;
		}

		// Waveファイルを閉じる
		file.close();

		// returnする為のデータ
		soundData.wfex = format.fmt;
		soundData.pBuffer = reinterpret_cast<BYTE*>(pBuffer);
		soundData.bufferSize = data.size;

		return soundData;
	}
}