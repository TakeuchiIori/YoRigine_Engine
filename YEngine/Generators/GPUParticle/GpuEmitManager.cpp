#include "GpuEmitManager.h"
#include <fstream>
#include <filesystem>
#include <iostream>
#include "Loaders/Json/JsonConverters.h"
#include <Loaders/Texture/TextureManager.h>
#include <ModelManager.h>
#include "Systems/GameTime/GameTime.h"

namespace YoRigine {
	// ImGui用の形状名一覧
	const char* GpuEmitManager::shapeNames_[] = {
		"円形",
		"箱形",
		"三角形",
		"コーン",
		"メッシュ"
	};

	/// <summary>
	/// GpuEmitManager シングルトン取得
	/// </summary>
	GpuEmitManager* GpuEmitManager::GetInstance()
	{
		static GpuEmitManager instance;
		return &instance;
	}

	/// <summary>
	/// 初期化（カメラ登録のみ）
	/// </summary>
	void GpuEmitManager::Initialize(Camera* camera)
	{
		camera_ = camera;
	}

	/// <summary>
	/// 全エミッターを更新 (グループ内をループ)
	/// </summary>
	void GpuEmitManager::Update()
	{
		float deltaTime = GameTime::GetDeltaTime();

		// グループ全体をループ
		for (auto& [groupName, groupData] : groups_) {
			// グループが有効かつ再生中の場合のみ更新
			if (groupData->isActive && groupData->isPlaying) {

				// システム時間の更新
				groupData->currentTime += deltaTime;

				// グループ内のエミッターをループ
				for (auto& [emitterName, emitterData] : groupData->emitters) {
					if (emitterData->isActive && emitterData->emitter) {
						emitterData->emitter->SetTrailParams(emitterData->trailParams);
						emitterData->emitter->Update();
					}
				}

				// システムの自動終了判定 (Durationが0より大きく、再生時間がDurationを超えたら停止)
				if (groupData->systemDuration > 0.0f && groupData->currentTime >= groupData->systemDuration) {
					StopEmitterGroup(groupName);
				}
			}
		}
	}

	/// <summary>
	/// 全エミッター描画 (グループ内をループ)
	/// </summary>
	void GpuEmitManager::Draw()
	{
		for (auto& [groupName, groupData] : groups_) {
			if (groupData->isActive) {
				for (auto& [emitterName, emitterData] : groupData->emitters) {
					if (emitterData->isActive && emitterData->emitter) {
						emitterData->emitter->Draw();
					}
				}
			}
		}
	}

	void GpuEmitManager::UpdateParticleParams(EmitterData* emitterData)
	{
		if (!emitterData || !emitterData->emitter) return;

		auto* emitter = emitterData->emitter.get();
		auto& params = emitterData->particleParams;

		emitter->SetLifeTime(params.lifeTime, params.lifeTimeVariance);
		emitter->SetScale(params.scale, params.scaleVariance);
		emitter->SetRotation(params.rotation, params.rotationVariance, params.rotationSpeed, params.rotationSpeedVariance);
		emitter->SetVelocity(params.velocity, params.velocityVariance);
		emitter->SetColor(params.color, params.colorVariance);
		emitter->SetBillboard(params.isBillboard);
	}
	/// <summary>
	/// エミッター管理用の ImGui 描画
	/// </summary>
	/// <summary>
	/// エミッター管理用の ImGui 描画
	/// </summary>
	void GpuEmitManager::DrawImGui()
	{
#ifdef USE_IMGUI
		// メニューバー

		if (ImGui::CollapsingHeader("ファイル操作・ロード", ImGuiTreeNodeFlags_DefaultOpen))
		{
			float halfWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
			if (ImGui::Button("\uF0C7 保存", ImVec2(halfWidth, 0))) {
				if (SaveToFile(saveFilePath_))
					std::cout << "保存成功: " << saveFilePath_ << std::endl;
				else
					std::cout << "保存失敗: " << saveFilePath_ << std::endl;
			}
			ImGui::SameLine();
			if (ImGui::Button("\uF07C 読み込み", ImVec2(halfWidth, 0))) {
				if (LoadFromFile(saveFilePath_))
					std::cout << "読み込み成功: " << saveFilePath_ << std::endl;
				else
					std::cout << "読み込み失敗: " << saveFilePath_ << std::endl;
			}

			ImGui::Separator();

			// 2. パス入力とディレクトリのスキャンロジック
			ImGui::InputText("ファイルパス", saveFilePath_, sizeof(saveFilePath_));

			// 編集中のパスからディレクトリ部分を抽出 (既存ロジック)
			std::filesystem::path currentPath(saveFilePath_);
			std::string dirPath;
			if (currentPath.has_filename()) {
				dirPath = currentPath.parent_path().string();
			} else {
				dirPath = currentPath.string();
			}
			if (dirPath.empty() || (dirPath.back() != '/' && dirPath.back() != '\\')) {
				dirPath += '/';
			}

			// スキャン実行
			if (dirPath != currentJsonDir_ || shouldRescanJson_)
			{
				ScanJsonDirectory(dirPath);
				shouldRescanJson_ = false;
			}

			// 3. ファイル一覧と再スキャンボタン
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.4f, 0.6f, 1.0f)); // ボタンの色を変更
			if (ImGui::Button("\uf2f9 再スキャン")) {
				ScanJsonDirectory(dirPath);
			}
			ImGui::PopStyleColor();

			ImGui::SameLine();
			ImGui::Text("現在のディレクトリ: %s", currentJsonDir_.c_str());

			ImGui::Separator();

			// JSONファイル一覧の表示
			ImGui::Text("\uf0d7 JSONファイル一覧 (.json)");
			// リストボックスとして機能させるための子ウィンドウ
			ImGui::BeginChild("JsonList", ImVec2(0, 150), true, ImGuiWindowFlags_HorizontalScrollbar);
			for (const auto& filename : availableJsonFiles_)
			{
				if (ImGui::Selectable(filename.c_str()))
				{
					// ディレクトリパスとファイル名を結合して saveFilePath_ を更新
					std::string newPath = currentJsonDir_ + filename;
					strncpy_s(saveFilePath_, newPath.c_str(), sizeof(saveFilePath_) - 1);
					saveFilePath_[sizeof(saveFilePath_) - 1] = '\0';
				}
			}
			ImGui::EndChild();

			ImGui::Separator();

			// 全削除ボタン（目立つように配置）
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f)); // 危険な操作なので赤くする
			if (ImGui::Button("全削除", ImVec2(ImGui::GetContentRegionAvail().x, 0)) && !groups_.empty()) {
				showDeleteDialog_ = true;
				selectedGroupName_.clear();
			}
			ImGui::PopStyleColor();
		}
		// タブバーでセクション分け
		if (ImGui::BeginTabBar("MainTabs", ImGuiTabBarFlags_None))
		{
			// ===== グループ管理タブ =====
			if (ImGui::BeginTabItem("グループ管理"))
			{
				DrawGroupManagementTab();
				ImGui::EndTabItem();
			}

			// ===== エミッター管理タブ =====
			if (ImGui::BeginTabItem("エミッター管理"))
			{
				DrawEmitterManagementTab();
				ImGui::EndTabItem();
			}

			// ===== エディタータブ =====
			if (ImGui::BeginTabItem("エディター"))
			{
				DrawEditorTab();
				ImGui::EndTabItem();
			}

			ImGui::EndTabBar();
		}

		// 削除確認ダイアログ
		DrawDeleteDialog();

#endif // USE_IMGUI
	}

	bool GpuEmitManager::DrawParticleParametersEditor(EmitterData* emitterData)
	{
#ifdef USE_IMGUI
		bool changed = false;

		if (ImGui::CollapsingHeader("パーティクルパラメータ設定", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 8));

			// ===== ビルボード設定 =====
			ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.2f, 0.5f, 0.7f, 0.8f));
			if (ImGui::CollapsingHeader("ビルボード", ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::PopStyleColor();
				ImGui::Indent(16.0f);
				changed |= ImGui::Checkbox("ビルボードを有効", &emitterData->particleParams.isBillboard);
				ImGui::TextDisabled("パーティクルが常にカメラの方向を向きます");
				ImGui::Unindent(16.0f);
				ImGui::Spacing();
			} else
			{
				ImGui::PopStyleColor();
			}

			// ===== 生存時間 =====
			ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.7f, 0.3f, 0.3f, 0.8f));
			if (ImGui::CollapsingHeader("生存時間", ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::PopStyleColor();
				ImGui::Indent(16.0f);

				changed |= ImGui::DragFloat("基本時間 (秒)", &emitterData->particleParams.lifeTime,
					0.1f, 0.1f, 30.0f, "%.2f 秒");

				changed |= ImGui::DragFloat("ランダム生存幅 (±)", &emitterData->particleParams.lifeTimeVariance,
					0.01f, 0.0f, 10.0f, "± %.2f 秒");

				float minLife = emitterData->particleParams.lifeTime - emitterData->particleParams.lifeTimeVariance;
				float maxLife = emitterData->particleParams.lifeTime + emitterData->particleParams.lifeTimeVariance;

				ImGui::BeginDisabled();
				ImGui::Text("範囲: %.2f ~ %.2f 秒", minLife, maxLife);
				ImGui::EndDisabled();

				ImGui::Unindent(16.0f);
				ImGui::Spacing();
			} else
			{
				ImGui::PopStyleColor();
			}

			// ===== スケール =====
			ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.3f, 0.7f, 0.3f, 0.8f));
			if (ImGui::CollapsingHeader("スケール", ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::PopStyleColor();
				ImGui::Indent(16.0f);

				// 基本スケール
				changed |= ImGui::DragFloat3("基本スケール", &emitterData->particleParams.scale.x,
					0.01f, 0.01f, 100.0f, "%.2f");

				// ランダム幅
				changed |= ImGui::DragFloat3("ランダムスケール幅", &emitterData->particleParams.scaleVariance.x,
					0.01f, 0.0f, 50.0f, "± %.2f");

				// 統一スケール調整用のヘルパー
				ImGui::Spacing();
				ImGui::TextDisabled("スケールプリセット:");
				if (ImGui::Button("小 (0.5)")) {
					emitterData->particleParams.scale = Vector3(0.5f, 0.5f, 0.5f);
					changed = true;
				}
				ImGui::SameLine();
				if (ImGui::Button("中 (1.0)")) {
					emitterData->particleParams.scale = Vector3(1.0f, 1.0f, 1.0f);
					changed = true;
				}
				ImGui::SameLine();
				if (ImGui::Button("大 (2.0)")) {
					emitterData->particleParams.scale = Vector3(2.0f, 2.0f, 2.0f);
					changed = true;
				}

				// プレビュー表示
				ImGui::Spacing();
				ImGui::BeginDisabled();
				Vector3 minScale = {
					std::max(0.01f, emitterData->particleParams.scale.x - emitterData->particleParams.scaleVariance.x),
					std::max(0.01f, emitterData->particleParams.scale.y - emitterData->particleParams.scaleVariance.y),
					std::max(0.01f, emitterData->particleParams.scale.z - emitterData->particleParams.scaleVariance.z)
				};
				Vector3 maxScale = {
					emitterData->particleParams.scale.x + emitterData->particleParams.scaleVariance.x,
					emitterData->particleParams.scale.y + emitterData->particleParams.scaleVariance.y,
					emitterData->particleParams.scale.z + emitterData->particleParams.scaleVariance.z
				};
				ImGui::Text("範囲 X: %.2f ~ %.2f", minScale.x, maxScale.x);
				ImGui::Text("範囲 Y: %.2f ~ %.2f", minScale.y, maxScale.y);
				ImGui::Text("範囲 Z: %.2f ~ %.2f", minScale.z, maxScale.z);
				ImGui::EndDisabled();

				ImGui::Unindent(16.0f);
				ImGui::Spacing();
			} else
			{
				ImGui::PopStyleColor();
			}

			// ===== 回転 =====
			ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.7f, 0.5f, 0.2f, 0.8f));
			if (ImGui::CollapsingHeader("回転", ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::PopStyleColor();
				ImGui::Indent(16.0f);

				// 初期回転角度（ラジアン）
				float rotationDeg = emitterData->particleParams.rotation * (180.0f / 3.14159265f);
				if (ImGui::DragFloat("初期回転角度", &rotationDeg, 1.0f, -360.0f, 360.0f, "%.1f°"))
				{
					emitterData->particleParams.rotation = rotationDeg * (3.14159265f / 180.0f);
					changed = true;
				}

				// 初期回転のランダム幅（ラジアン）
				float rotationVarianceDeg = emitterData->particleParams.rotationVariance * (180.0f / 3.14159265f);
				if (ImGui::DragFloat("ランダム回転幅", &rotationVarianceDeg, 1.0f, 0.0f, 180.0f, "± %.1f°"))
				{
					emitterData->particleParams.rotationVariance = rotationVarianceDeg * (3.14159265f / 180.0f);
					changed = true;
				}

				ImGui::Spacing();
				ImGui::Separator();
				ImGui::Spacing();

				// 回転速度（ラジアン/秒）
				float rotationSpeedDeg = emitterData->particleParams.rotationSpeed * (180.0f / 3.14159265f);
				if (ImGui::DragFloat("回転速度", &rotationSpeedDeg, 1.0f, -360.0f, 360.0f, "%.1f°/s"))
				{
					emitterData->particleParams.rotationSpeed = rotationSpeedDeg * (3.14159265f / 180.0f);
					changed = true;
				}

				// 回転速度のランダム幅（ラジアン/秒）
				float rotationSpeedVarianceDeg = emitterData->particleParams.rotationSpeedVariance * (180.0f / 3.14159265f);
				if (ImGui::DragFloat("ランダム回転速度幅", &rotationSpeedVarianceDeg, 1.0f, 0.0f, 180.0f, "± %.1f°/s"))
				{
					emitterData->particleParams.rotationSpeedVariance = rotationSpeedVarianceDeg * (3.14159265f / 180.0f);
					changed = true;
				}

				// プリセットボタン
				ImGui::Spacing();
				ImGui::TextDisabled("回転プリセット:");
				if (ImGui::Button("回転しない")) {
					emitterData->particleParams.rotationSpeed = 0.0f;
					emitterData->particleParams.rotationSpeedVariance = 0.0f;
					changed = true;
				}
				ImGui::SameLine();
				if (ImGui::Button("ゆっくり右回転")) {
					emitterData->particleParams.rotationSpeed = 0.5f;
					emitterData->particleParams.rotationSpeedVariance = 0.1f;
					changed = true;
				}
				ImGui::SameLine();
				if (ImGui::Button("速く右回転")) {
					emitterData->particleParams.rotationSpeed = 2.0f;
					emitterData->particleParams.rotationSpeedVariance = 0.5f;
					changed = true;
				}
				if (ImGui::Button("ゆっくり左回転")) {
					emitterData->particleParams.rotationSpeed = -0.5f;
					emitterData->particleParams.rotationSpeedVariance = 0.1f;
					changed = true;
				}
				ImGui::SameLine();
				if (ImGui::Button("速く左回転")) {
					emitterData->particleParams.rotationSpeed = -2.0f;
					emitterData->particleParams.rotationSpeedVariance = 0.5f;
					changed = true;
				}
				ImGui::SameLine();
				if (ImGui::Button("ランダム回転")) {
					emitterData->particleParams.rotationSpeed = 0.0f;
					emitterData->particleParams.rotationSpeedVariance = 2.0f;
					changed = true;
				}

				// プレビュー表示
				ImGui::Spacing();
				ImGui::BeginDisabled();
				float minRotSpeed = (emitterData->particleParams.rotationSpeed - emitterData->particleParams.rotationSpeedVariance) * (180.0f / 3.14159265f);
				float maxRotSpeed = (emitterData->particleParams.rotationSpeed + emitterData->particleParams.rotationSpeedVariance) * (180.0f / 3.14159265f);
				ImGui::Text("回転速度の範囲: %.1f° ~ %.1f° per second", minRotSpeed, maxRotSpeed);
				ImGui::EndDisabled();

				ImGui::Unindent(16.0f);
				ImGui::Spacing();
			} else
			{
				ImGui::PopStyleColor();
			}

			// ===== 速度 =====
			ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.5f, 0.3f, 0.7f, 0.8f));
			if (ImGui::CollapsingHeader("速度", ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::PopStyleColor();
				ImGui::Indent(16.0f);

				// 基本速度
				changed |= ImGui::DragFloat3("基本速度", &emitterData->particleParams.velocity.x,
					0.01f, -10.0f, 10.0f, "%.2f");

				// ランダム幅
				changed |= ImGui::DragFloat3("ランダム速度幅", &emitterData->particleParams.velocityVariance.x,
					0.01f, 0.0f, 5.0f, "± %.2f");

				// 方向プリセット
				ImGui::Spacing();
				ImGui::TextDisabled("方向プリセット:");
				if (ImGui::Button("上")) {
					emitterData->particleParams.velocity = Vector3(0.0f, 1.0f, 0.0f);
					changed = true;
				}
				ImGui::SameLine();
				if (ImGui::Button("下")) {
					emitterData->particleParams.velocity = Vector3(0.0f, -1.0f, 0.0f);
					changed = true;
				}
				ImGui::SameLine();
				if (ImGui::Button("前")) {
					emitterData->particleParams.velocity = Vector3(0.0f, 0.0f, 1.0f);
					changed = true;
				}
				ImGui::SameLine();
				if (ImGui::Button("後ろ")) {
					emitterData->particleParams.velocity = Vector3(0.0f, 0.0f, -1.0f);
					changed = true;
				}
				ImGui::SameLine();
				if (ImGui::Button("右")) {
					emitterData->particleParams.velocity = Vector3(1.0f, 0.0f, 0.0f);
					changed = true;
				}
				ImGui::SameLine();
				if (ImGui::Button("左")) {
					emitterData->particleParams.velocity = Vector3(-1.0f, 0.0f, 0.0f);
					changed = true;
				}
				ImGui::SameLine();
				if (ImGui::Button("停止")) {
					emitterData->particleParams.velocity = Vector3(0.0f, 0.0f, 0.0f);
					changed = true;
				}

				// 速度の大きさを表示
				ImGui::Spacing();
				ImGui::BeginDisabled();
				float speed = std::sqrt(
					emitterData->particleParams.velocity.x * emitterData->particleParams.velocity.x +
					emitterData->particleParams.velocity.y * emitterData->particleParams.velocity.y +
					emitterData->particleParams.velocity.z * emitterData->particleParams.velocity.z
				);
				ImGui::Text("速度の大きさ: %.2f units/sec", speed);
				ImGui::EndDisabled();

				ImGui::Unindent(16.0f);
				ImGui::Spacing();
			} else
			{
				ImGui::PopStyleColor();
			}

			// ===== 色 =====
			ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.7f, 0.7f, 0.2f, 0.8f));
			if (ImGui::CollapsingHeader("色", ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::PopStyleColor();
				ImGui::Indent(16.0f);

				// 基本色（カラーピッカー）
				changed |= ImGui::ColorEdit4("基本色", &emitterData->particleParams.color.x,
					ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_DisplayRGB);

				// ランダム幅（RGB）
				changed |= ImGui::DragFloat3("RGB ランダム幅(±)", &emitterData->particleParams.colorVariance.x,
					0.01f, 0.0f, 1.0f, "± %.2f");

				// アルファのランダム幅
				changed |= ImGui::DragFloat("Alpha ランダム幅 (±)", &emitterData->particleParams.colorVariance.w,
					0.01f, 0.0f, 1.0f, "± %.2f");

				// 色プリセット
				ImGui::Spacing();
				ImGui::TextDisabled("色プリセット:");
				if (ImGui::Button("白")) {
					emitterData->particleParams.color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
					changed = true;
				}
				ImGui::SameLine();
				if (ImGui::Button("赤")) {
					emitterData->particleParams.color = Vector4(1.0f, 0.2f, 0.2f, 1.0f);
					changed = true;
				}
				ImGui::SameLine();
				if (ImGui::Button("黄色")) {
					emitterData->particleParams.color = Vector4(1.0f, 1.0f, 0.2f, 1.0f);
					changed = true;
				}
				if (ImGui::Button("緑")) {
					emitterData->particleParams.color = Vector4(0.2f, 1.0f, 0.2f, 1.0f);
					changed = true;
				}
				ImGui::SameLine();
				if (ImGui::Button("青")) {
					emitterData->particleParams.color = Vector4(0.2f, 0.5f, 1.0f, 1.0f);
					changed = true;
				}
				// カラーバリエーション
				ImGui::Spacing();
				ImGui::TextDisabled("カラーばらつきプリセット:");
				if (ImGui::Button("ばらつきなし")) {
					emitterData->particleParams.colorVariance = Vector4(0.0f, 0.0f, 0.0f, 0.0f);
					changed = true;
				}
				ImGui::SameLine();
				if (ImGui::Button("小さなばらつき")) {
					emitterData->particleParams.colorVariance = Vector4(0.1f, 0.1f, 0.1f, 0.0f);
					changed = true;
				}
				ImGui::SameLine();
				if (ImGui::Button("大きなばらつき")) {
					emitterData->particleParams.colorVariance = Vector4(0.3f, 0.3f, 0.3f, 0.0f);
					changed = true;
				}
				ImGui::Unindent(16.0f);
				ImGui::Spacing();
			} else
			{
				ImGui::PopStyleColor();
			}
			ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
			if (ImGui::CollapsingHeader("トレイル", ImGuiTreeNodeFlags_None))
			{
				ImGui::PopStyleColor();
				changed |= ImGui::Checkbox("有効化", &emitterData->trailParams.isTrail);
				changed |= ImGui::Checkbox("エミッターのスケールを継承", &emitterData->trailParams.inheritScale);
				ImGui::DragFloat("トレイル生成距離", &emitterData->trailParams.minDistance, 0.01f, 0.0f, 1000.0f);
				ImGui::DragFloat("トレイル寿命", &emitterData->trailParams.lifeTime, 0.01f, 0.0f, 1000.0f);
				ImGui::DragFloat("生成パーティクル数", &emitterData->trailParams.emissionCount, 1.0f, 1.0f, 100000.0f);
			} else {
				ImGui::PopStyleColor();
			}

			// ===== プリセット全体 =====
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.6f, 0.3f, 0.8f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.7f, 0.4f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f, 0.5f, 0.2f, 1.0f));

			ImGui::Text("パーティクルプリセットs:");

			if (ImGui::Button("炎", ImVec2(120, 0))) {
				emitterData->particleParams.lifeTime = 2.0f;
				emitterData->particleParams.lifeTimeVariance = 0.5f;
				emitterData->particleParams.scale = Vector3(1.0f, 1.0f, 1.0f);
				emitterData->particleParams.scaleVariance = Vector3(0.3f, 0.3f, 0.3f);
				emitterData->particleParams.velocity = Vector3(0.0f, 0.5f, 0.0f);
				emitterData->particleParams.velocityVariance = Vector3(0.2f, 0.1f, 0.2f);
				emitterData->particleParams.color = Vector4(1.0f, 0.5f, 0.1f, 1.0f);
				emitterData->particleParams.colorVariance = Vector4(0.2f, 0.2f, 0.1f, 0.0f);
				emitterData->particleParams.rotation = 0.0f;
				emitterData->particleParams.rotationSpeed = 0.0f;
				emitterData->particleParams.rotationSpeedVariance = 1.0f;
				changed = true;
			}
			ImGui::SameLine();
			if (ImGui::Button("雨", ImVec2(120, 0))) {
				emitterData->particleParams.lifeTime = 2.0f;
				emitterData->particleParams.lifeTimeVariance = 0.3f;
				emitterData->particleParams.scale = Vector3(0.3f, 1.0f, 0.3f);
				emitterData->particleParams.scaleVariance = Vector3(0.1f, 0.2f, 0.1f);
				emitterData->particleParams.velocity = Vector3(0.0f, -2.0f, 0.0f);
				emitterData->particleParams.velocityVariance = Vector3(0.1f, 0.2f, 0.1f);
				emitterData->particleParams.color = Vector4(0.7f, 0.8f, 1.0f, 0.6f);
				emitterData->particleParams.colorVariance = Vector4(0.1f, 0.1f, 0.1f, 0.2f);
				emitterData->particleParams.rotation = 0.0f;
				emitterData->particleParams.rotationSpeed = 0.0f;
				emitterData->particleParams.rotationSpeedVariance = 0.0f;
				emitterData->particleParams.isBillboard = false;
				changed = true;
			}
			ImGui::SameLine();
			if (ImGui::Button("雪", ImVec2(120, 0))) {
				emitterData->particleParams.lifeTime = 5.0f;
				emitterData->particleParams.lifeTimeVariance = 1.0f;
				emitterData->particleParams.scale = Vector3(0.8f, 0.8f, 0.8f);
				emitterData->particleParams.scaleVariance = Vector3(0.3f, 0.3f, 0.3f);
				emitterData->particleParams.velocity = Vector3(0.0f, -0.3f, 0.0f);
				emitterData->particleParams.velocityVariance = Vector3(0.2f, 0.1f, 0.2f);
				emitterData->particleParams.color = Vector4(1.0f, 1.0f, 1.0f, 0.9f);
				emitterData->particleParams.colorVariance = Vector4(0.1f, 0.1f, 0.1f, 0.1f);
				emitterData->particleParams.rotation = 0.0f;
				emitterData->particleParams.rotationSpeed = 0.0f;
				emitterData->particleParams.rotationSpeedVariance = 0.5f;
				changed = true;
			}
			ImGui::PopStyleColor(3);
			ImGui::PopStyleVar(2);
		}

		return changed;
#else
		(void)emitterData;
		return false;
#endif
	}

	/// <summary>
	/// 現在の形状に応じたパラメータ編集を表示
	/// </summary>
	bool GpuEmitManager::DrawShapeEditor(EmitterData* emitterData)
	{
		switch (emitterData->shape)
		{
		case EmitterShape::Sphere:   return DrawSphereEditor(emitterData);
		case EmitterShape::Box:      return DrawBoxEditor(emitterData);
		case EmitterShape::Triangle: return DrawTriangleEditor(emitterData);
		case EmitterShape::Cone:     return DrawConeEditor(emitterData);
		case EmitterShape::Mesh:     return DrawMeshEditor(emitterData);
		}
		return false;
	}

	/// <summary>
	/// Sphere パラメータの ImGui 編集
	/// </summary>
	bool GpuEmitManager::DrawSphereEditor(EmitterData* emitterData)
	{
		(void)emitterData;
		bool changed = false;
#ifdef USE_IMGUI

		auto& p = emitterData->sphereParams;

		changed |= ImGui::DragFloat3("位置", &p.translate.x, 0.1f);
		changed |= ImGui::DragFloat("半径", &p.radius, 0.1f, 0.1f, 10000.0f);
		changed |= ImGui::DragFloat("射出パーティクル数", &p.count, 1.0f, 1.0f, GPUParticle::kMaxParticles);
		changed |= ImGui::DragFloat("射出間隔", &p.emitInterval, 0.01f, 0.01f, 10.0f);


#endif
		return changed;
	}

	/// <summary>
	/// Box パラメータの ImGui 編集
	/// </summary>
	bool GpuEmitManager::DrawBoxEditor(EmitterData* emitterData)
	{
		(void)emitterData;
		bool changed = false;
#ifdef USE_IMGUI

		auto& p = emitterData->boxParams;

		changed |= ImGui::DragFloat3("位置", &p.translate.x, 0.1f);
		changed |= ImGui::DragFloat3("サイズ", &p.size.x, 0.1f, 0.1f, 10000.0f);
		changed |= ImGui::DragFloat("射出パーティクル数", &p.count, 1.0f, 1.0f, GPUParticle::kMaxParticles);
		changed |= ImGui::DragFloat("射出間隔", &p.emitInterval, 0.01f, 0.01f, 10.0f);


#endif
		return changed;
	}

	/// <summary>
	/// Triangle パラメータの ImGui 編集
	/// </summary>
	bool GpuEmitManager::DrawTriangleEditor(EmitterData* emitterData)
	{
		(void)emitterData;
		bool changed = false;
#ifdef USE_IMGUI

		auto& p = emitterData->triangleParams;

		changed |= ImGui::DragFloat3("頂点 1", &p.v1.x, 0.1f);
		changed |= ImGui::DragFloat3("頂点 2", &p.v2.x, 0.1f);
		changed |= ImGui::DragFloat3("頂点 3", &p.v3.x, 0.1f);
		changed |= ImGui::DragFloat("射出パーティクル数", &p.count, 1.0f, 1.0f, GPUParticle::kMaxParticles);
		changed |= ImGui::DragFloat("射出間隔", &p.emitInterval, 0.01f, 0.01f, 10.0f);
#endif
		return changed;
	}

	/// <summary>
	/// Cone パラメータの ImGui 編集
	/// </summary>
	bool GpuEmitManager::DrawConeEditor(EmitterData* emitterData)
	{
		(void)emitterData;
		bool changed = false;
#ifdef USE_IMGUI

		auto& p = emitterData->coneParams;

		changed |= ImGui::DragFloat3("位置", &p.translate.x, 0.1f);
		changed |= ImGui::DragFloat3("とんがる方向", &p.direction.x, 0.01f, -1.0f, 1.0f);
		changed |= ImGui::DragFloat("半径", &p.radius, 0.1f, 0.1f, 10000.0f);
		changed |= ImGui::DragFloat("高さ", &p.height, 0.1f, 0.1f, 10000.0f);
		changed |= ImGui::DragFloat("射出パーティクル数", &p.count, 1.0f, 1.0f, GPUParticle::kMaxParticles);
		changed |= ImGui::DragFloat("射出間隔", &p.emitInterval, 0.01f, 0.01f, 10.0f);

#endif
		return changed;
	}

	bool GpuEmitManager::DrawMeshEditor(EmitterData* emitterData)
	{
		(void)emitterData;
#ifdef USE_IMGUI
		bool changed = false;
		auto& p = emitterData->meshParams;

		// -------------------------
		// モデル選択コンボボックス
		// -------------------------
		auto modelKeys = ModelManager::GetInstance()->GetModelKeys();
		static int selected = -1;

		// 現在の選択を反映
		if (p.model != nullptr) {
			std::string currentKey = p.model->GetName();
			for (int i = 0; i < modelKeys.size(); i++) {
				if (modelKeys[i] == currentKey) {
					selected = i;
					break;
				}
			}
		}

		if (ImGui::BeginCombo("使用モデル", selected >= 0 ? modelKeys[selected].c_str() : "未選択"))
		{
			for (int i = 0; i < modelKeys.size(); i++)
			{
				bool isSelected = (selected == i);
				if (ImGui::Selectable(modelKeys[i].c_str(), isSelected)) {
					selected = i;
					p.model = ModelManager::GetInstance()->FindModel(modelKeys[i]);
					changed = true;
				}
			}
			ImGui::EndCombo();
		}

		// -------------------------
		// 通常パラメータ
		// -------------------------

		changed |= ImGui::DragFloat3("位置", &p.translate.x, 0.1f);
		changed |= ImGui::DragFloat3("スケール", &p.scale.x, 0.1f);

		float r[4] = { p.rotation.x, p.rotation.y, p.rotation.z, p.rotation.w };
		if (ImGui::DragFloat4("回転(Quat)", r, 0.01f)) {
			p.rotation = Quaternion(r[0], r[1], r[2], r[3]);
			changed = true;
		}

		changed |= ImGui::DragFloat("射出数", &p.count, 1.0f);
		changed |= ImGui::DragFloat("射出間隔", &p.emitInterval, 0.01f);

		const char* modeList[] = { "Surface", "Volume", "Edge" };
		int modeIndex = static_cast<int>(p.emitMode);
		if (ImGui::Combo("Emit Mode", &modeIndex, modeList, 3)) {
			p.emitMode = static_cast<MeshEmitMode>(modeIndex);
			changed = true;
		}

		return changed;
#else
		return false;
#endif
	}

	/// <summary>
	/// グループ管理タブ
	/// </summary>
	void GpuEmitManager::DrawGroupManagementTab()
	{
#ifdef USE_IMGUI
		ImGui::BeginChild("GroupManagement", ImVec2(0, 0), false);

		// ===== 新規グループ作成 =====
		ImGui::SeparatorText("新規グループ作成");

		ImGui::PushItemWidth(-150);
		ImGui::InputTextWithHint("##NewGroupName", "グループ名を入力...", newGroupName_, sizeof(newGroupName_));
		ImGui::PopItemWidth();

		ImGui::SameLine();
		ImGui::BeginDisabled(strlen(newGroupName_) == 0);
		if (ImGui::Button("作成", ImVec2(140, 0))) {
			CreateEmitterGroup(newGroupName_);
			selectedGroupName_ = newGroupName_;
			newGroupName_[0] = '\0';
		}
		ImGui::EndDisabled();

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		// ===== グループリスト =====
		ImGui::SeparatorText("グループリスト");

		ImGui::Text("登録グループ数: %zu", groups_.size());

		// フィルター検索
		static char groupFilter[256] = "";
		ImGui::PushItemWidth(-1);
		ImGui::InputTextWithHint("##GroupFilter", "\uf002 検索...", groupFilter, sizeof(groupFilter));
		ImGui::PopItemWidth();

		ImGui::Spacing();

		// グループリストテーブル
		if (ImGui::BeginTable("GroupTable", 4,
			ImGuiTableFlags_Borders |
			ImGuiTableFlags_RowBg |
			ImGuiTableFlags_ScrollY |
			ImGuiTableFlags_Resizable,
			ImVec2(0, 300)))
		{
			ImGui::TableSetupColumn("状態", ImGuiTableColumnFlags_WidthFixed, 50);
			ImGui::TableSetupColumn("グループ名", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("エミッター数", ImGuiTableColumnFlags_WidthFixed, 100);
			ImGui::TableSetupColumn("再生", ImGuiTableColumnFlags_WidthFixed, 80);
			ImGui::TableHeadersRow();

			for (auto& [name, groupData] : groups_)
			{
				// フィルター適用
				if (strlen(groupFilter) > 0 && name.find(groupFilter) == std::string::npos)
					continue;

				ImGui::TableNextRow();

				// 状態列
				ImGui::TableSetColumnIndex(0);
				ImGui::PushID(name.c_str());
				ImGui::Checkbox("##Active", &groupData->isActive);
				ImGui::PopID();

				// グループ名列
				ImGui::TableSetColumnIndex(1);
				bool isSelected = (selectedGroupName_ == name);

				ImGuiSelectableFlags flags = ImGuiSelectableFlags_SpanAllColumns;
				if (ImGui::Selectable(name.c_str(), isSelected, flags)) {
					selectedGroupName_ = name;
					selectedEmitterName_.clear();
				}

				// 右クリックメニュー
				if (ImGui::BeginPopupContextItem())
				{
					if (ImGui::MenuItem("削除")) {
						DeleteEmitterGroup(name);
						ImGui::EndPopup();
						break;
					}
					ImGui::EndPopup();
				}

				// エミッター数列
				ImGui::TableSetColumnIndex(2);
				ImGui::TextDisabled("%zu", groupData->emitters.size());

				// 再生状態列
				ImGui::TableSetColumnIndex(3);
				ImGui::PushID((name + "_play").c_str());
				if (groupData->isPlaying) {
					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.3f, 0.3f, 1.0f));
					if (ImGui::SmallButton("■")) {
						StopEmitterGroup(name);
					}
					ImGui::PopStyleColor();
				} else {
					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));
					if (ImGui::SmallButton("▶")) {
						PlayEmitterGroup(name);
					}
					ImGui::PopStyleColor();
				}
				ImGui::PopID();
			}

			ImGui::EndTable();
		}

		ImGui::Spacing();

		// ===== 選択グループの詳細 =====
		EmitterGroup* currentGroup = GetGroup(selectedGroupName_);
		if (currentGroup)
		{
			ImGui::Separator();
			ImGui::SeparatorText(("選択中: " + currentGroup->name).c_str());

			// プロパティグリッド風のレイアウト
			if (ImGui::BeginTable("GroupProperties", 2, ImGuiTableFlags_BordersInnerV))
			{
				ImGui::TableSetupColumn("プロパティ", ImGuiTableColumnFlags_WidthFixed, 150);
				ImGui::TableSetupColumn("値", ImGuiTableColumnFlags_WidthStretch);

				// 有効/無効
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::AlignTextToFramePadding();
				ImGui::Text("有効");
				ImGui::TableSetColumnIndex(1);
				ImGui::Checkbox("##GroupActive", &currentGroup->isActive);

				// 再生状態
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::AlignTextToFramePadding();
				ImGui::Text("再生状態");
				ImGui::TableSetColumnIndex(1);

				if (currentGroup->isPlaying) {
					ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "● 再生中");
					ImGui::SameLine();
					if (ImGui::Button("\uf04d 停止")) {
						StopEmitterGroup(selectedGroupName_);
					}
				} else {
					ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "○ 停止中");
					ImGui::SameLine();
					if (ImGui::Button("▶ 再生")) {
						PlayEmitterGroup(selectedGroupName_);
					}
				}

				// 経過時間
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::AlignTextToFramePadding();
				ImGui::Text("経過時間");
				ImGui::TableSetColumnIndex(1);
				ImGui::Text("%.2f 秒", currentGroup->currentTime);

				// システム寿命
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::AlignTextToFramePadding();
				ImGui::Text("システム寿命");
				ImGui::TableSetColumnIndex(1);
				ImGui::SetNextItemWidth(-1);
				ImGui::DragFloat("##SystemDuration", &currentGroup->systemDuration,
					0.1f, 0.0f, 60.0f, "%.1f 秒 (0=無限)");

				// 位置
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::AlignTextToFramePadding();
				ImGui::Text("位置");
				ImGui::TableSetColumnIndex(1);
				ImGui::SetNextItemWidth(-1);
				ImGui::DragFloat3("##GroupTranslate", &currentGroup->translate.x, 0.1f);

				ImGui::EndTable();
			}

			ImGui::Spacing();

			// グループ操作ボタン
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.3f, 0.3f, 0.8f));
			if (ImGui::Button("このグループを削除", ImVec2(-1, 0))) {
				showDeleteDialog_ = true;
			}
			ImGui::PopStyleColor();
		}

		ImGui::EndChild();
#endif
	}

	/// <summary>
	/// エミッター管理タブ
	/// </summary>
	void GpuEmitManager::DrawEmitterManagementTab()
	{
#ifdef USE_IMGUI
		ImGui::BeginChild("EmitterManagement", ImVec2(0, 0), false);

		if (selectedGroupName_.empty()) {
			ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f),
				"⚠ グループを選択してください");
			ImGui::Text("「グループ管理」タブでグループを作成・選択してください。");
			ImGui::EndChild();
			return;
		}

		EmitterGroup* currentGroup = GetGroup(selectedGroupName_);
		if (!currentGroup) {
			ImGui::EndChild();
			return;
		}

		// ===== 新規エミッター作成 =====
		ImGui::SeparatorText("新規エミッター作成");
		ImGui::Text("作成先グループ: %s", currentGroup->name.c_str());

		ImGui::Spacing();

		// 名前入力
		ImGui::Text("名前:");
		ImGui::SameLine();
		ImGui::PushItemWidth(250);
		ImGui::InputTextWithHint("##EmitterName", "エミッター名...",
			newEmitterName_, sizeof(newEmitterName_));
		ImGui::PopItemWidth();

		// 形状選択
		ImGui::Text("形状:");
		ImGui::SameLine();
		ImGui::PushItemWidth(150);
		ImGui::Combo("##Shape", &selectedShapeIndex_, shapeNames_, IM_ARRAYSIZE(shapeNames_));
		ImGui::PopItemWidth();

		// テクスチャパス
		ImGui::Text("テクスチャ:");
		ImGui::PushItemWidth(-150);
		ImGui::InputTextWithHint("##TexturePath", "テクスチャパス...",
			newEmitterTexturePath_, sizeof(newEmitterTexturePath_));
		ImGui::PopItemWidth();

		ImGui::SameLine();
		static bool textureBrowserOpen = false;
		if (ImGui::Button("参照...", ImVec2(140, 0))) {
			ScanTextureDirectory("Resources/Textures/");
			textureBrowserOpen = !textureBrowserOpen;
		}

		// テクスチャブラウザ
		if (textureBrowserOpen)
		{
			ImGui::Spacing();
			DrawTextureBrowser(textureBrowserOpen);
		}

		ImGui::Spacing();

		// 作成ボタン
		ImGui::BeginDisabled(strlen(newEmitterName_) == 0);
		if (ImGui::Button("エミッター作成", ImVec2(-1, 35)))
		{
			std::string name = newEmitterName_;
			std::string texPath = newEmitterTexturePath_;
			EmitterShape shape = static_cast<EmitterShape>(selectedShapeIndex_);

			if (CreateEmitter(selectedGroupName_, name, texPath, shape))
			{
				selectedEmitterName_ = name;
				std::memset(newEmitterName_, 0, sizeof(newEmitterName_));
				std::memset(newEmitterTexturePath_, 0, sizeof(newEmitterTexturePath_));
			}
		}
		ImGui::EndDisabled();

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		// ===== エミッターリスト =====
		ImGui::SeparatorText("エミッターリスト");
		ImGui::Text("エミッター数: %zu", currentGroup->emitters.size());

		// フィルター検索
		static char emitterFilter[256] = "";
		ImGui::PushItemWidth(-1);
		ImGui::InputTextWithHint("##EmitterFilter", "\uf002 検索...",
			emitterFilter, sizeof(emitterFilter));
		ImGui::PopItemWidth();

		ImGui::Spacing();

		// エミッターリストテーブル
		if (ImGui::BeginTable("EmitterTable", 4,
			ImGuiTableFlags_Borders |
			ImGuiTableFlags_RowBg |
			ImGuiTableFlags_ScrollY |
			ImGuiTableFlags_Resizable,
			ImVec2(0, -1)))
		{
			ImGui::TableSetupColumn("有効", ImGuiTableColumnFlags_WidthFixed, 50);
			ImGui::TableSetupColumn("名前", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("形状", ImGuiTableColumnFlags_WidthFixed, 100);
			ImGui::TableSetupColumn("操作", ImGuiTableColumnFlags_WidthFixed, 60);
			ImGui::TableHeadersRow();

			for (auto it = currentGroup->emitters.begin();
				it != currentGroup->emitters.end(); )
			{
				const std::string& name = it->first;
				auto* data = it->second.get();

				// フィルター適用
				if (strlen(emitterFilter) > 0 && name.find(emitterFilter) == std::string::npos) {
					++it;
					continue;
				}

				ImGui::TableNextRow();
				ImGui::PushID(name.c_str());

				// 有効列
				ImGui::TableSetColumnIndex(0);
				ImGui::Checkbox("##Active", &data->isActive);

				// 名前列
				ImGui::TableSetColumnIndex(1);
				bool isSelected = (selectedEmitterName_ == name);

				if (ImGui::Selectable(name.c_str(), isSelected,
					ImGuiSelectableFlags_SpanAllColumns))
				{
					selectedEmitterName_ = name;
				}

				// 形状列
				ImGui::TableSetColumnIndex(2);
				ImGui::TextDisabled("%s", shapeNames_[static_cast<int>(data->shape)]);

				// 操作列
				ImGui::TableSetColumnIndex(3);
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.3f, 0.3f, 0.8f));
				if (ImGui::SmallButton("削除")) {
					if (selectedEmitterName_ == name) {
						selectedEmitterName_.clear();
					}
					it = currentGroup->emitters.erase(it);
					ImGui::PopStyleColor();
					ImGui::PopID();
					continue;
				}
				ImGui::PopStyleColor();

				ImGui::PopID();
				++it;
			}

			ImGui::EndTable();
		}

		ImGui::EndChild();
#endif
	}


	/// <summary>
	/// テクスチャブラウザ
	/// </summary>
	void GpuEmitManager::DrawTextureBrowser(bool& isOpen)
	{
		(void)isOpen;
#ifdef USE_IMGUI
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 10));

		if (ImGui::BeginChild("TextureBrowser", ImVec2(0, 350), true))
		{
			ImGui::Text("\uf07b フォルダ: %s", currentTextureDir_.c_str());
			ImGui::Separator();

			// 親フォルダへ戻る
			if (ImGui::Selectable("\uf07b [親フォルダへ]"))
			{
				std::filesystem::path parent =
					std::filesystem::path(currentTextureDir_).parent_path();
				if (!parent.empty()) {
					ScanTextureDirectory(parent.string());
				}
			}

			ImGui::Spacing();

			// サブフォルダ
			for (const auto& folder : availableFolders_)
			{
				std::string fullPath = currentTextureDir_ + "/" + folder;
				if (ImGui::Selectable(("\uf07b " + folder).c_str()))
				{
					ScanTextureDirectory(fullPath);
				}
			}

			if (!availableFolders_.empty() && !availableTextures_.empty())
				ImGui::Separator();

			// ファイル（グリッド表示）
			int columns = static_cast<int>(ImGui::GetContentRegionAvail().x / 80);
			columns = std::max(1, columns);

			if (ImGui::BeginTable("TextureGrid", columns))
			{
				for (const auto& file : availableTextures_)
				{
					ImGui::TableNextColumn();
					ImGui::PushID(file.c_str());

					TextureManager::GetInstance()->LoadTexture(file);
					auto cpuHandle = TextureManager::GetInstance()->GetsrvHandleGPU(file);

					if (cpuHandle.ptr != 0) {
						ImTextureID texID = static_cast<ImTextureID>(cpuHandle.ptr);

						if (ImGui::ImageButton("##img", texID, ImVec2(64, 64)))
						{
							strncpy_s(newEmitterTexturePath_, file.c_str(),
								sizeof(newEmitterTexturePath_) - 1);
							isOpen = false;
						}

						if (ImGui::IsItemHovered())
						{
							ImGui::BeginTooltip();
							ImGui::Text("%s",
								std::filesystem::path(file).filename().string().c_str());
							ImGui::EndTooltip();
						}
					}

					ImGui::PopID();
				}

				ImGui::EndTable();
			}

			ImGui::EndChild();
		}

		ImGui::PopStyleVar();
#endif
	}

	/// <summary>
	/// エディタータブ
	/// </summary>
	void GpuEmitManager::DrawEditorTab()
	{
#ifdef USE_IMGUI
		ImGui::BeginChild("Editor", ImVec2(0, 0), false);

		if (selectedGroupName_.empty() || selectedEmitterName_.empty())
		{
			ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f),
				"⚠ エミッターを選択してください");
			ImGui::Text("「エミッター管理」タブでエミッターを選択してください。");
			ImGui::EndChild();
			return;
		}

		auto* emitterData = GetEmitter(selectedGroupName_, selectedEmitterName_);
		if (!emitterData || !emitterData->emitter)
		{
			ImGui::EndChild();
			return;
		}

		// ヘッダー情報
		ImGui::SeparatorText(("編集中: " + emitterData->name).c_str());
		ImGui::TextDisabled("形状: %s", shapeNames_[static_cast<int>(emitterData->shape)]);

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		// スクロール領域
		ImGui::BeginChild("EditorScroll", ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);

		// パーティクルパラメータ編集
		if (DrawParticleParametersEditor(emitterData)) {
			UpdateParticleParams(emitterData);
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		// エミッター形状パラメータ編集
		if (ImGui::CollapsingHeader("エミッター形状設定", ImGuiTreeNodeFlags_DefaultOpen))
		{
			// 形状変更
			int currentShape = static_cast<int>(emitterData->shape);
			if (ImGui::Combo("形状", &currentShape, shapeNames_, IM_ARRAYSIZE(shapeNames_)))
			{
				emitterData->shape = static_cast<EmitterShape>(currentShape);
				emitterData->emitter->SetEmitterShape(emitterData->shape);
				UpdateEmitterParams(emitterData);
			}

			ImGui::Spacing();

			// 形状別パラメータ
			if (DrawShapeEditor(emitterData)) {
				UpdateEmitterParams(emitterData);
			}
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		// 統計情報
		if (ImGui::CollapsingHeader("パーティクル統計情報"))
		{
			auto stats = emitterData->emitter->GetGPUParticle()->GetCachedStats();

			if (stats.isValid) {
				ImGui::Text("アクティブ数: %u / %u", stats.activeCount, stats.maxParticles);
				ImGui::Text("未使用スロット数: %u", stats.freeCount);

				ImGui::ProgressBar(
					stats.usagePercent / 100.0f,
					ImVec2(-1, 0),
					std::format("{:.1f}%%", stats.usagePercent).c_str()
				);

				if (stats.freeListIndex < 0) {
					ImGui::TextColored(ImVec4(1, 0, 0, 1),
						"エラー: 空きパーティクルがありません！");
				}
			} else {
				ImGui::TextColored(ImVec4(1, 1, 0, 1),
					"統計情報を読み込み中...");
			}

			if (ImGui::Button("詳細統計を表示", ImVec2(-1, 0))) {
				emitterData->emitter->GetGPUParticle()->DrawStatsImGui();
			}
		}

		ImGui::EndChild();

		ImGui::EndChild();
#endif
	}
	/// <summary>
	/// 削除確認ダイアログ
	/// </summary>
	void GpuEmitManager::DrawDeleteDialog()
	{
#ifdef USE_IMGUI
		if (showDeleteDialog_) {
			ImGui::OpenPopup("削除確認");
		}

		ImVec2 center = ImGui::GetMainViewport()->GetCenter();
		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

		if (ImGui::BeginPopupModal("削除確認", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			// 削除対象によってメッセージを変更
			if (selectedGroupName_.empty()) {
				ImGui::Text("すべてのエミッターグループを削除しますか？");
				ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "この操作は取り消せません！");
			} else {
				ImGui::Text("選択中のグループ '%s' を削除しますか？", selectedGroupName_.c_str());
				ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "グループ内の全エミッターも削除されます！");
			}

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			// 削除実行ボタン
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
			if (ImGui::Button("削除する", ImVec2(120, 0))) {
				if (selectedGroupName_.empty()) {
					DeleteAllEmitterGroups();
				} else {
					DeleteEmitterGroup(selectedGroupName_);
				}

				showDeleteDialog_ = false;
				ImGui::CloseCurrentPopup();
			}
			ImGui::PopStyleColor();

			ImGui::SetItemDefaultFocus();
			ImGui::SameLine();

			if (ImGui::Button("キャンセル", ImVec2(120, 0))) {
				showDeleteDialog_ = false;
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}
#endif
	}



	/// <summary>
	/// 新しいエミッターを作成
	/// </summary>
	GpuEmitManager::EmitterData* GpuEmitManager::CreateEmitter(const std::string& groupName, const std::string& emitterName, std::string& texturePath, EmitterShape shape)
	{
		EmitterGroup* group = GetGroup(groupName);
		// グループが存在しない
		if (!group) return nullptr;

		// 同名のエミッターが存在する
		if (group->emitters.count(emitterName)) return nullptr;

		//-------------------- 値保存をしてから生成 --------------------//
		auto newEmitterData = std::make_unique<EmitterData>();
		newEmitterData->name = emitterName;
		newEmitterData->shape = shape;
		newEmitterData->isActive = true;
		newEmitterData->texturePath = texturePath;
		newEmitterData->emitter = std::make_unique<GPUEmitter>();

		// GPUEmitter の初期化
		newEmitterData->emitter->Initialize(camera_, texturePath);
		newEmitterData->emitter->SetEmitterShape(shape);

		// デフォルトパラメータ反映
		UpdateEmitterParams(newEmitterData.get());

		// グループのコンテナに追加
		group->emitters[emitterName] = std::move(newEmitterData);
		return group->emitters[emitterName].get();
	}

	/// <summary>
	/// エミッターを 1 つ削除
	/// </summary>
	void GpuEmitManager::DeleteEmitter(const std::string& groupName, const std::string& emitterName)
	{
		EmitterGroup* group = GetGroup(groupName);
		if (!group) return;

		if (group->emitters.count(emitterName)) {
			group->emitters.erase(emitterName);
			if (selectedEmitterName_ == emitterName) {
				selectedEmitterName_.clear();
			}
		}
	}

	/// <summary>
	/// 全エミッター削除
	/// </summary>
	void GpuEmitManager::DeleteAllEmitters()
	{
		for (auto& [name, data] : groups_) {
			data->emitters.clear();
		}
	}

	GpuEmitManager::EmitterGroup* GpuEmitManager::GetGroup(const std::string& groupName)
	{
		if (groups_.count(groupName)) {
			return groups_.at(groupName).get();
		}
		return nullptr;
	}

	GpuEmitManager::EmitterGroup* GpuEmitManager::CreateEmitterGroup(const std::string& groupName)
	{
		if (groups_.count(groupName)) {
			return nullptr; // 既に存在するなら作成しない
		}

		auto newGroup = std::make_unique<EmitterGroup>();
		newGroup->name = groupName;
		groups_[groupName] = std::move(newGroup);

		return groups_[groupName].get();
	}

	void GpuEmitManager::DeleteEmitterGroup(const std::string& groupName)
	{
		if (groups_.count(groupName)) {
			groups_.erase(groupName);
			if (selectedGroupName_ == groupName) {
				selectedGroupName_.clear();
				selectedEmitterName_.clear(); // グループ削除に伴い選択も解除
			}
		}
	}

	void GpuEmitManager::DeleteAllEmitterGroups()
	{
		// グループコンテナ全体をクリア
		groups_.clear();

		// 選択中のグループ名とエミッター名をクリアしてリセット
		selectedGroupName_.clear();
		selectedEmitterName_.clear();
	}

	void GpuEmitManager::PlayEmitterGroup(const std::string& groupName)
	{
		EmitterGroup* group = GetGroup(groupName);
		if (!group)return;

		// 再生中じゃない場合のみ処理する
		if (!group->isPlaying)
		{
			group->isPlaying = true;
			group->currentTime = 0.0f;
			// グループ内のエミッターを再生
			for (auto& [name, data] : group->emitters)
			{
				if (data->emitter) {
					data->emitter->Reset();
				}
			}
		}
	}

	void GpuEmitManager::StopEmitterGroup(const std::string& groupName)
	{
		EmitterGroup* group = GetGroup(groupName);
		if (!group)return;

		group->isPlaying = false;
		group->currentTime = 0.0f;

		// 停止時にエミッターをリセット
		for (auto& [name, data] : group->emitters)
		{
			if (data->emitter) {
				data->emitter->Reset();
			}
		}
	}

	/// <summary>
	/// エミッター名からデータ取得
	/// </summary>
	GpuEmitManager::EmitterData* GpuEmitManager::GetEmitter(const std::string& groupName, const std::string& emitterName)
	{
		EmitterGroup* group = GetGroup(groupName);
		if (!group) return nullptr;

		if (group->emitters.count(emitterName)) {
			return group->emitters.at(emitterName).get();
		}
		return nullptr;
	}

	/// <summary>
	/// 全エミッター名リスト取得
	/// </summary>
	std::vector<std::string> GpuEmitManager::GetEmitterNames() const
	{
		std::vector<std::string> names;
		for (const auto& [groupName, groupPtr] : groups_) {
			for (const auto& [emitterName, _] : groupPtr->emitters) {
				names.push_back(emitterName);
			}
		}
		return names;
	}

	/// <summary>
	/// 指定エミッターが存在するか
	/// </summary>
	bool GpuEmitManager::HasEmitter(const std::string& emitterName) const
	{
		for (const auto& [groupName, groupPtr] : groups_) {
			if (groupPtr->emitters.count(emitterName)) {
				return true;
			}
		}
		return false;
	}

	/// <summary>
	/// エミッターの形状パラメータを GPUEmitter に反映
	/// </summary>
	void GpuEmitManager::UpdateEmitterParams(EmitterData* emitterData)
	{
		if (!emitterData || !emitterData->emitter) {
			return;
		}

		switch (emitterData->shape)
		{
		case EmitterShape::Sphere:
		{
			const auto& p = emitterData->sphereParams;
			emitterData->emitter->UpdateSphereParams(
				p.translate, p.radius, p.count, p.emitInterval
			);
		}
		break;

		case EmitterShape::Box:
		{
			const auto& p = emitterData->boxParams;
			emitterData->emitter->UpdateBoxParams(
				p.translate, p.size, p.count, p.emitInterval
			);
		}
		break;

		case EmitterShape::Triangle:
		{
			const auto& p = emitterData->triangleParams;
			emitterData->emitter->UpdateTriangleParams(
				p.v1, p.v2, p.v3, p.translate, p.count, p.emitInterval
			);
		}
		break;

		case EmitterShape::Cone:
		{
			const auto& p = emitterData->coneParams;
			emitterData->emitter->UpdateConeParams(
				p.translate, p.direction, p.radius, p.height, p.count, p.emitInterval
			);
		}
		break;
		case EmitterShape::Mesh:
		{
			const auto& p = emitterData->meshParams;
			emitterData->emitter->UpdateMeshParams(
				p.model, p.translate, p.scale, p.rotation,
				p.count, p.emitInterval, p.emitMode
			);
		}
		break;
		}
	}

	/// <summary>
	/// JSON ファイルとしてエミッター情報を保存
	/// </summary>
	bool GpuEmitManager::SaveToFile(const std::string& filepath)
	{
		try
		{
			nlohmann::json json = ToJson();

			// 必要ならディレクトリ作成
			std::filesystem::path path(filepath);
			std::filesystem::create_directories(path.parent_path());

			std::ofstream file(filepath);
			if (!file.is_open()) {
				return false;
			}

			file << json.dump(4);
			return true;
		}
		catch (const std::exception& e)
		{
			std::cerr << "Error saving emitters: " << e.what() << std::endl;
			return false;
		}
	}

	/// <summary>
	/// JSON ファイルからエミッター情報を読み込み
	/// </summary>
	bool GpuEmitManager::LoadFromFile(const std::string& filepath)
	{
		try
		{
			std::ifstream file(filepath);

			if (!file.is_open()) {
				return false;
			}

			nlohmann::json json;
			file >> json;

			return FromJson(json);
		}
		catch (const std::exception& e)
		{
			std::cerr << "Error loading emitters: " << e.what() << std::endl;
			return false;
		}
	}

	/// <summary>
	/// 全エミッターを JSON 化
	/// </summary>
	nlohmann::json GpuEmitManager::ToJson() const
	{
		nlohmann::json json;
		json["version"] = "1.0";
		json["groups"] = nlohmann::json::array();

		for (const auto& [groupName, groupPtr] : groups_)
		{
			nlohmann::json groupJson;
			// ------------------------------------------------------------
			// エミッターグループのパラメータをシリアライズ
			// ------------------------------------------------------------
			groupJson["groupName"] = groupPtr->name;
			groupJson["isActive"] = groupPtr->isActive;
			groupJson["isPlaying"] = groupPtr->isPlaying;
			groupJson["currentTime"] = groupPtr->currentTime;
			groupJson["systemDuration"] = groupPtr->systemDuration;
			groupJson["translate"] = Vector3ToJson(groupPtr->translate);
			groupJson["emitters"] = nlohmann::json::array(); // グループ内のエミッター配列

			for (const auto& [emitterName, e] : groupPtr->emitters)
			{
				nlohmann::json j;
				//------------------------------------------------------------
				// 生成時に必要な情報
				//------------------------------------------------------------
				j["name"] = e->name;
				j["shape"] = static_cast<int>(e->shape);
				j["isActive"] = e->isActive;
				j["textureFilePath"] = e->texturePath;

				//------------------------------------------------------------
				// エミッターの形状
				//------------------------------------------------------------
				j["sphereParams"] = {
					{"translate", Vector3ToJson(e->sphereParams.translate)},
					{"radius",    e->sphereParams.radius},
					{"count",     e->sphereParams.count},
					{"emitInterval", e->sphereParams.emitInterval}
				};

				j["boxParams"] = {
					{"translate", Vector3ToJson(e->boxParams.translate)},
					{"size",      Vector3ToJson(e->boxParams.size)},
					{"count",     e->boxParams.count},
					{"emitInterval", e->boxParams.emitInterval}
				};

				j["triangleParams"] = {
					{"v1", Vector3ToJson(e->triangleParams.v1)},
					{"v2", Vector3ToJson(e->triangleParams.v2)},
					{"v3", Vector3ToJson(e->triangleParams.v3)},
					{"count", e->triangleParams.count},
					{"emitInterval", e->triangleParams.emitInterval}
				};

				j["coneParams"] = {
					{"translate", Vector3ToJson(e->coneParams.translate)},
					{"direction", Vector3ToJson(e->coneParams.direction)},
					{"radius",    e->coneParams.radius},
					{"height",    e->coneParams.height},
					{"count",     e->coneParams.count},
					{"emitInterval", e->coneParams.emitInterval}
				};
				j["meshParams"] = {
					{"modelName", e->meshParams.model ? e->meshParams.model->GetName() : ""},
					{"translate", Vector3ToJson(e->meshParams.translate)},
					{"scale", Vector3ToJson(e->meshParams.scale)},
					{"rotation", Vector4ToJson({e->meshParams.rotation.x,e->meshParams.rotation.y,e->meshParams.rotation.z,e->meshParams.rotation.w})},
					{"count", e->meshParams.count},
					{"emitInterval", e->meshParams.emitInterval},
					{"emitMode", static_cast<int>(e->meshParams.emitMode)}
				};



				//------------------------------------------------------------
				// パーティクルのパラメータ
				//------------------------------------------------------------
				j["particleParams"] = {
					{"lifeTime",    e->particleParams.lifeTime},
					{"lifeTimeVariance", e->particleParams.lifeTimeVariance},

					{"scale",          Vector3ToJson(e->particleParams.scale)},
					{"scaleVariance",  Vector3ToJson(e->particleParams.scaleVariance)},

					{"rotation",               e->particleParams.rotation},
					{"rotationVariance",       e->particleParams.rotationVariance},
					{"rotationSpeed",          e->particleParams.rotationSpeed},
					{"rotationSpeedVariance",  e->particleParams.rotationSpeedVariance},

					{"velocity",         Vector3ToJson(e->particleParams.velocity)},
					{"velocityVariance", Vector3ToJson(e->particleParams.velocityVariance)},

					{"color",         Vector4ToJson(e->particleParams.color)},
					{"colorVariance", Vector4ToJson(e->particleParams.colorVariance)},

					{"isBillboard", e->particleParams.isBillboard}
				};

				j["trail"] = {
				{"enabled", e->trailParams.isTrail},
				{"minDistance",e->trailParams.minDistance},
				{"lifeTime",e->trailParams.lifeTime },
				{"emissionCount",e->trailParams.emissionCount },
				{"inheritScale", e->trailParams.inheritScale }
				};
				groupJson["emitters"].push_back(j);
			}
			json["groups"].push_back(groupJson);
		}
		return json;
	}


	/// <summary>
	/// JSON からエミッター情報を復元
	/// </summary>
	bool GpuEmitManager::FromJson(const nlohmann::json& json)
	{
		try
		{
			// ロード前に既存グループを全て削除
			DeleteAllEmitterGroups();

			bool loaded = false;

			// 新しいグループ形式 ("groups") の読み込みを試みる
			if (json.contains("groups"))
			{
				// グループの配列をループ
				for (const auto& groupJson : json["groups"])
				{
					//------------------------------------------------------------
					// グループ情報の復元
					//------------------------------------------------------------
					// groupName がない場合に備えてデフォルト値 "LoadedGroup" を設定
					std::string groupName = groupJson.value("groupName", "LoadedGroup");

					// グループの作成
					EmitterGroup* group = CreateEmitterGroup(groupName);
					if (!group) continue;

					// グループパラメータの復元
					group->isActive = groupJson.value("isActive", true);
					group->isPlaying = groupJson.value("isPlaying", false);
					group->currentTime = groupJson.value("currentTime", 0.0f);
					group->systemDuration = groupJson.value("systemDuration", 0.0f);

					if (groupJson.contains("translate")) {
						group->translate = JsonToVector3(groupJson["translate"]);
					}

					if (groupJson.contains("emitters")) // エミッター配列があればループ
					{
						for (const auto& j : groupJson["emitters"])
						{
							// 補助関数でエミッターをロード
							LoadEmitterFromJson(groupName, j);
						}
						loaded = true; // グループが存在し、処理が実行された
					}
				}
			}
			return loaded; // ロードできたかどうかを返す
		}
		catch (const std::exception& e)
		{
			// デシリアライズ中にエラーが発生した場合
			std::cerr << "Error parsing JSON: " << e.what() << std::endl;
			return false;
		}
	}

	/// <summary>
	/// JSONから単一のエミッター情報を復元
	/// </summary>
	bool GpuEmitManager::LoadEmitterFromJson(const std::string& groupName, const nlohmann::json& j)
	{
		//------------------------------------------------------------
		// 生成時に必要な情報
		//------------------------------------------------------------
		std::string		name = j.value("name", "UnnamedEmitter");
		EmitterShape	shape = static_cast<EmitterShape>(j.value("shape", 0)); // 0はSphereを想定
		bool			isActive = j.value("isActive", true);
		std::string texturePath = j.value("textureFilePath", "");

		if (CreateEmitter(groupName, name, texturePath, shape))
		{
			// GetEmitter に groupName を渡す
			auto* e = GetEmitter(groupName, name);
			if (!e) return false;

			e->isActive = isActive;

			//------------------------------------------------------------
			// エミッターの形状
			//------------------------------------------------------------
			// Sphere
			if (j.contains("sphereParams")) {
				const auto& p = j["sphereParams"];
				e->sphereParams.translate = JsonToVector3(p["translate"]);
				e->sphereParams.radius = p["radius"];
				e->sphereParams.count = p["count"];
				e->sphereParams.emitInterval = p["emitInterval"];
			}

			// Box
			if (j.contains("boxParams")) {
				const auto& p = j["boxParams"];
				e->boxParams.translate = JsonToVector3(p["translate"]);
				e->boxParams.size = JsonToVector3(p["size"]);
				e->boxParams.count = p["count"];
				e->boxParams.emitInterval = p["emitInterval"];
			}

			// Triangle
			if (j.contains("triangleParams")) {
				const auto& p = j["triangleParams"];
				e->triangleParams.v1 = JsonToVector3(p["v1"]);
				e->triangleParams.v2 = JsonToVector3(p["v2"]);
				e->triangleParams.v3 = JsonToVector3(p["v3"]);
				e->triangleParams.count = p["count"];
				e->triangleParams.emitInterval = p["emitInterval"];
			}

			// Cone
			if (j.contains("coneParams")) {
				const auto& p = j["coneParams"];
				e->coneParams.translate = JsonToVector3(p["translate"]);
				e->coneParams.direction = JsonToVector3(p["direction"]);
				e->coneParams.radius = p["radius"];
				e->coneParams.height = p["height"];
				e->coneParams.count = p["count"];
				e->coneParams.emitInterval = p["emitInterval"];
			}
			// Mesh
			if (j.contains("meshParams")) {
				const auto& mp = j["meshParams"];

				std::string modelName = mp.value("modelName", "");
				if (!modelName.empty()) {
					e->meshParams.model = ModelManager::GetInstance()->FindModel(modelName);
				}

				e->meshParams.translate = JsonToVector3(mp["translate"]);
				e->meshParams.scale = JsonToVector3(mp["scale"]);
				Vector4 r = JsonToVector4(mp["rotation"]);
				e->meshParams.rotation = Quaternion(r.x, r.y, r.z, r.w);

				e->meshParams.count = mp["count"];
				e->meshParams.emitInterval = mp["emitInterval"];
				e->meshParams.emitMode = static_cast<MeshEmitMode>(mp["emitMode"]);
			}


			//------------------------------------------------------------
			// パーティクルのパラメータ
			//------------------------------------------------------------
			if (j.contains("particleParams"))
			{
				const auto& pp = j["particleParams"];
				e->particleParams.lifeTime = pp["lifeTime"];
				e->particleParams.lifeTimeVariance = pp["lifeTimeVariance"];
				e->particleParams.isBillboard = pp["isBillboard"];

				e->particleParams.scale = JsonToVector3(pp["scale"]);
				e->particleParams.scaleVariance = JsonToVector3(pp["scaleVariance"]);

				e->particleParams.rotation = pp["rotation"];
				e->particleParams.rotationVariance = pp["rotationVariance"];
				e->particleParams.rotationSpeed = pp["rotationSpeed"];
				e->particleParams.rotationSpeedVariance = pp["rotationSpeedVariance"];

				e->particleParams.velocity = JsonToVector3(pp["velocity"]);
				e->particleParams.velocityVariance = JsonToVector3(pp["velocityVariance"]);

				e->particleParams.color = JsonToVector4(pp["color"]);
				e->particleParams.colorVariance = JsonToVector4(pp["colorVariance"]);

			}
			//------------------------------------------------------------
			// トレイルのパラメータ
			//------------------------------------------------------------
			if (j.contains("trail"))
			{
				const auto& tp = j["trail"];
				e->trailParams.isTrail = tp.value("enabled", false);
				e->trailParams.minDistance = tp.value("minDistance", 0.1f);
				e->trailParams.lifeTime = tp.value("lifeTime", 1.0f);
				e->trailParams.emissionCount = tp.value("emissionCount", 1.0f);
				e->trailParams.inheritScale = tp.value("inheritScale", false);
			}

			// 適用
			UpdateEmitterParams(e);
			UpdateParticleParams(e);

			return true;
		}
		return false;
	}
	// 画像一覧をスキャン
	void GpuEmitManager::ScanTextureDirectory(const std::string& directory)
	{
		currentTextureDir_ = directory;
		availableTextures_.clear();
		availableFolders_.clear();

		for (auto& p : std::filesystem::directory_iterator(directory))
		{
			if (p.is_directory()) {
				availableFolders_.push_back(p.path().filename().string());
				continue;
			}

			if (!p.is_regular_file()) continue;

			std::string ext = p.path().extension().string();

			std::transform(
				ext.begin(), ext.end(), ext.begin(),
				[](unsigned char c) { return static_cast<char>(std::tolower(c)); }
			);

			if (ext == ".png" || ext == ".jpg" || ext == ".dds")
			{
				availableTextures_.push_back(p.path().string());
			}
		}
	}
	// JSONファイル一覧をスキャン
	void GpuEmitManager::ScanJsonDirectory(const std::string& directory)
	{
		// ディレクトリが存在しない、またはディレクトリでない場合はクリアして終了
		if (!std::filesystem::exists(directory) || !std::filesystem::is_directory(directory)) {
			availableJsonFiles_.clear();
			return;
		}

		currentJsonDir_ = directory;
		availableJsonFiles_.clear();

		for (auto& p : std::filesystem::directory_iterator(directory))
		{
			// ファイルであること
			if (!p.is_regular_file()) continue;

			// 拡張子が .json であること
			if (p.path().extension().string() == ".json")
			{
				availableJsonFiles_.push_back(p.path().filename().string());
			}
		}
	}
}
