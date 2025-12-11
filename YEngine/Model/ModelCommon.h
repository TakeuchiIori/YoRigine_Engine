#pragma once

// モデル共通クラス
class DirectXCommon;

class ModelCommon
{
public:
	///************************* 基本関数 *************************///

	// インスタンス取得
	static ModelCommon* GetInstance();

	// 初期化
	void Initialize(DirectXCommon* dxCommon);

public:
	///************************* アクセッサ *************************///

	// DirectX共通取得
	DirectXCommon* GetDxCommon() const { return dxCommon_; }

private:
	///************************* コンストラクタ禁止 *************************///

	ModelCommon() = default;
	~ModelCommon() = default;
	ModelCommon(const ModelCommon&) = delete;
	ModelCommon& operator=(const ModelCommon&) = delete;

private:
	///************************* メンバ変数 *************************///

	// DirectX共通クラス
	DirectXCommon* dxCommon_ = nullptr;
};
