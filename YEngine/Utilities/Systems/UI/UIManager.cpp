#include "UIManager.h"
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <iomanip>
#include <set>

#ifdef USE_IMGUI
#include <imgui.h>
#endif
#include <Loaders/Texture/TextureManager.h>

namespace YoRigine {
	const std::string UIManager::SCENE_DIRECTORY = "./Resources/UIScenes/";
	const std::string UIManager::UI_CONFIG_DIRECTORY = "./Resources/UIConfigs/";

	UIManager* UIManager::GetInstance() {
		static UIManager instance;
		return &instance;
	}

	/*==================================================================
							UI基本管理
	===================================================================*/

	void UIManager::AddUI(const std::string& id, std::unique_ptr<UIBase> ui) {
		if (!ui) return;

		uiElements_[id] = std::move(ui);
		RebuildDrawOrder();
	}

	void UIManager::RemoveUI(const std::string& id) {
		for (auto& [groupName, uiIds] : groups_) {
			auto it = std::find(uiIds.begin(), uiIds.end(), id);
			if (it != uiIds.end()) {
				uiIds.erase(it);
			}
		}

		uiElements_.erase(id);
		RebuildDrawOrder();
	}

	UIBase* UIManager::GetUI(const std::string& id) {
		auto it = uiElements_.find(id);
		if (it != uiElements_.end()) {
			return it->second.get();
		}
		return nullptr;
	}

	bool UIManager::HasUI(const std::string& id) const {
		return uiElements_.find(id) != uiElements_.end();
	}

	void UIManager::Clear() {
		uiElements_.clear();
		groups_.clear();
		drawOrder_.clear();
		selectedUIId_.clear();
	}

	/// <summary>
	///  UIのIDを変更（重複時は連番付与）
	/// </summary>
	/// <param name="oldId">現在のID</param>
	/// <param name="newId">変更したいID</param>
	/// <returns>成功時true</returns>
	bool UIManager::RenameUI(const std::string& oldId, const std::string& newId) {
		if (oldId == newId) return true;
		auto it = uiElements_.find(oldId);
		if (it == uiElements_.end()) return false;

		// 空や空白のみのIDは不可
		auto trim = [](std::string s) {
			s.erase(0, s.find_first_not_of(" \t\r\n"));
			s.erase(s.find_last_not_of(" \t\r\n") + 1);
			return s;
			};
		std::string base = trim(newId);
		if (base.empty()) return false;

		// 重複回避
		std::string finalId = base;
		int counter = 1;
		while (HasUI(finalId)) {
			finalId = base + "_" + std::to_string(counter++);
		}

		// 要素をムーブして差し替え
		auto uiPtr = std::move(it->second);
		uiElements_.erase(it);
		uiElements_.emplace(finalId, std::move(uiPtr));

		// グループ参照を更新
		for (auto& [gname, ids] : groups_) {
			for (auto& idRef : ids) {
				if (idRef == oldId) idRef = finalId;
			}
		}

		// 描画順を更新
		for (auto& idInOrder : drawOrder_) {
			if (idInOrder == oldId) idInOrder = finalId;
		}

		// 選択ID更新
		if (selectedUIId_ == oldId) selectedUIId_ = finalId;

		// 念のため順序再構築
		RebuildDrawOrder();
		return true;
	}


	/*==================================================================
							一括更新・描画
	===================================================================*/

	void UIManager::UpdateAll() {
		for (auto& [id, ui] : uiElements_) {
			if (ui) {
				ui->Update();
			}
		}
	}

	void UIManager::DrawAll() {
		for (const auto& id : drawOrder_) {
			auto it = uiElements_.find(id);
			if (it != uiElements_.end() && it->second) {
				it->second->Draw();
			}
		}
	}

	void UIManager::Draw(const std::string& id) {
		auto it = uiElements_.find(id);
		if (it != uiElements_.end() && it->second) {
			it->second->Draw();
		}
	}

	/*==================================================================
							レイヤー管理
	===================================================================*/

	void UIManager::SortByLayer() {
		RebuildDrawOrder();
	}

	void UIManager::ShowLayer(int layer, bool show) {
		for (auto& [id, ui] : uiElements_) {
			if (ui && ui->GetLayer() == layer) {
				ui->SetVisible(show);
			}
		}
	}

	void UIManager::ShowAll(bool show) {
		for (auto& [id, ui] : uiElements_) {
			if (ui) {
				ui->SetVisible(show);
			}
		}
	}

	std::vector<UIBase*> UIManager::GetUIsByLayer(int layer) {
		std::vector<UIBase*> result;
		for (auto& [id, ui] : uiElements_) {
			if (ui && ui->GetLayer() == layer) {
				result.push_back(ui.get());
			}
		}
		return result;
	}

	/*==================================================================
							シーン管理
	===================================================================*/

	bool UIManager::SaveScene(const std::string& sceneName) {
		if (sceneName.empty()) return false;

		try {
			// 必要なフォルダを作成
			std::filesystem::create_directories(SCENE_DIRECTORY);
			std::filesystem::create_directories(UI_CONFIG_DIRECTORY);
			const std::string sceneConfigDir = GetSceneConfigDir(sceneName);
			std::filesystem::create_directories(sceneConfigDir);

			nlohmann::json sceneData;
			nlohmann::json uiArray = nlohmann::json::array();

			// drawOrder_の順番でUIを保存
			for (const auto& id : drawOrder_) {
				auto it = uiElements_.find(id);
				if (it == uiElements_.end() || !it->second) continue;

				nlohmann::json uiData;
				uiData["id"] = id;

				// UIごとの設定を シーン名フォルダ/ID.json に保存
				uiData["configPath"] = sceneConfigDir + id + ".json";
				it->second->SaveToJSON(uiData["configPath"]);

				uiArray.push_back(uiData);
			}
			sceneData["uis"] = uiArray;

			// 描画順序を保存
			sceneData["drawOrder"] = drawOrder_;

			// グループ情報（そのまま）
			nlohmann::json groupsData;
			for (const auto& [groupName, uiIds] : groups_) {
				groupsData[groupName] = uiIds;
			}
			sceneData["groups"] = groupsData;

			// シーンJSONを書き出し
			const std::string scenePath = SCENE_DIRECTORY + sceneName + ".json";
			std::ofstream file(scenePath);
			if (!file.is_open()) return false;

			file << std::setw(4) << sceneData << std::endl;
			file.close();
			return true;
		}
		catch (const std::exception& e) {
			printf("シーン保存エラー: %s\n", e.what());
			return false;
		}
	}

	bool UIManager::LoadScene(const std::string& sceneName) {
		const std::string scenePath = SCENE_DIRECTORY + sceneName + ".json";
		std::ifstream f(scenePath);
		if (!f.is_open()) return false;

		nlohmann::json sceneData;
		f >> sceneData;
		f.close();

		// 既存UIをクリア
		Clear();

		const std::string sceneConfigDir = GetSceneConfigDir(sceneName);

		// UI 群の復元
		if (sceneData.contains("uis")) {
			for (const auto& uiEntry : sceneData["uis"]) {
				const std::string id = uiEntry.value("id", "");
				if (id.empty()) continue;

				// シーンJSONに書かれた configPath を最優先
				std::string cfgPath = uiEntry.value("configPath", "");

				// 無ければ新形式 <UIConfigs>/<SceneName>/<ID>.json
				if (cfgPath.empty()) {
					cfgPath = sceneConfigDir + id + ".json";
				}

				// 無ければ旧形式 <UIConfigs>/<ID>.json（後方互換）
				if (!std::filesystem::exists(cfgPath)) {
					const std::string legacy = UI_CONFIG_DIRECTORY + id + ".json";
					if (std::filesystem::exists(legacy)) {
						cfgPath = legacy;
					}
				}

				// 実体生成 - 直接uiElements_に追加してRebuildDrawOrderを回避
				auto ui = std::make_unique<UIBase>(id);
				ui->Initialize(cfgPath);
				uiElements_[id] = std::move(ui);
			}
		}

		// 描画順序の復元（保存されていれば優先、無ければレイヤー順で再構築）
		if (sceneData.contains("drawOrder")) {
			drawOrder_.clear();
			for (const auto& id : sceneData["drawOrder"]) {
				std::string idStr = id.get<std::string>();
				// 実際に存在するUIのみ追加
				if (uiElements_.find(idStr) != uiElements_.end()) {
					drawOrder_.push_back(idStr);
				}
			}
		} else {
			// 後方互換性：drawOrderが無い場合はレイヤー順で再構築
			RebuildDrawOrder();
		}

		// グループの復元（既存のままでOK）
		if (sceneData.contains("groups")) {
			for (auto it = sceneData["groups"].begin(); it != sceneData["groups"].end(); ++it) {
				const std::string groupName = it.key();
				for (const auto& id : it.value()) {
					AddToGroup(groupName, id.get<std::string>());
				}
			}
		}

		return true;
	}


	std::vector<std::string> UIManager::GetAvailableScenes() const {
		std::vector<std::string> scenes;

		if (!std::filesystem::exists(SCENE_DIRECTORY)) {
			return scenes;
		}

		for (const auto& entry : std::filesystem::directory_iterator(SCENE_DIRECTORY)) {
			if (entry.is_regular_file() && entry.path().extension() == ".json") {
				std::string filename = entry.path().stem().string();
				if (filename.find("temp_") != 0) {
					scenes.push_back(filename);
				}
			}
		}

		std::sort(scenes.begin(), scenes.end());
		return scenes;
	}

	bool UIManager::DeleteScene(const std::string& sceneName) {
		if (sceneName.empty()) return false;

		try {
			std::string scenePath = SCENE_DIRECTORY + sceneName + ".json";
			if (std::filesystem::exists(scenePath)) {
				return std::filesystem::remove(scenePath);
			}
			return false;
		}
		catch (const std::exception& e) {
			printf("シーン削除エラー: %s\n", e.what());
			return false;
		}
	}

	/*==================================================================
							グループ管理
	===================================================================*/

	void UIManager::AddToGroup(const std::string& groupName, const std::string& uiId) {
		if (!HasUI(uiId)) return;

		auto& group = groups_[groupName];
		if (std::find(group.begin(), group.end(), uiId) == group.end()) {
			group.push_back(uiId);
		}
	}

	void UIManager::RemoveFromGroup(const std::string& groupName, const std::string& uiId) {
		auto it = groups_.find(groupName);
		if (it != groups_.end()) {
			auto& group = it->second;
			group.erase(std::remove(group.begin(), group.end(), uiId), group.end());

			if (group.empty()) {
				groups_.erase(it);
			}
		}
	}

	void UIManager::ShowGroup(const std::string& groupName, bool show) {
		auto it = groups_.find(groupName);
		if (it != groups_.end()) {
			for (const auto& uiId : it->second) {
				auto* ui = GetUI(uiId);
				if (ui) {
					ui->SetVisible(show);
				}
			}
		}
	}

	std::vector<UIBase*> UIManager::GetGroup(const std::string& groupName) {
		std::vector<UIBase*> result;
		auto it = groups_.find(groupName);
		if (it != groups_.end()) {
			for (const auto& uiId : it->second) {
				auto* ui = GetUI(uiId);
				if (ui) {
					result.push_back(ui);
				}
			}
		}
		return result;
	}

	/*==================================================================
							検索・フィルタ
	===================================================================*/

	std::vector<UIBase*> UIManager::FindByName(const std::string& name) {
		std::vector<UIBase*> result;
		for (auto& [id, ui] : uiElements_) {
			if (ui && ui->GetName().find(name) != std::string::npos) {
				result.push_back(ui.get());
			}
		}
		return result;
	}

	std::vector<UIBase*> UIManager::FindByTexture(const std::string& texturePath) {
		std::vector<UIBase*> result;
		for (auto& [id, ui] : uiElements_) {
			if (ui && ui->GetTexturePath() == texturePath) {
				result.push_back(ui.get());
			}
		}
		return result;
	}

	/*==================================================================
							統計情報
	===================================================================*/

	UIManager::Statistics UIManager::GetStatistics() const {
		Statistics stats;
		stats.totalUIs = static_cast<int>(uiElements_.size());

		for (const auto& [id, ui] : uiElements_) {
			if (!ui) continue;

			if (ui->IsVisible()) {
				stats.visibleUIs++;
			} else {
				stats.hiddenUIs++;
			}

			int layer = ui->GetLayer();
			stats.uisByLayer[layer]++;
		}

		return stats;
	}

	/*==================================================================
							ImGuiデバッグ
	===================================================================*/

	void UIManager::ImGuiDebug() {
#ifdef USE_IMGUI

		// タブバーで整理
		if (ImGui::BeginTabBar("UIManagerTabs")) {

			// ===== UIリスト & 編集タブ =====
			if (ImGui::BeginTabItem("UI編集")) {

				// 左側: UIリスト (幅25% - 少し狭めに)
				ImGui::BeginChild("UIList", ImVec2(ImGui::GetContentRegionAvail().x * 0.25f, 0), true);

				ImGui::Text("UI一覧 (%zu個)", uiElements_.size());
				ImGui::Separator();

				if (ImGui::Button("新規作成", ImVec2(-1, 0))) {
					std::string newId = GenerateUniqueID("NewUI");
					auto newUI = std::make_unique<UIBase>(newId);
					newUI->Initialize("./Resources/UIConfigs/" + newId + ".json");
					AddUI(newId, std::move(newUI));
					selectedUIId_ = newId;
				}

				if (ImGui::Button("全て表示", ImVec2(-1, 0))) {
					ShowAll(true);
				}
				if (ImGui::Button("全て非表示", ImVec2(-1, 0))) {
					ShowAll(false);
				}

				ImGui::Separator();

				// フィルター
				static char filterText[128] = "";
				ImGui::InputTextWithHint("##filter", "検索...", filterText, sizeof(filterText));

				// レイヤーでソート表示切り替え
				static bool sortByLayer = false;
				ImGui::Checkbox("レイヤーでグループ化", &sortByLayer);

				ImGui::Separator();

				// 削除対象のIDを記録
				std::string uiToDelete = "";

				if (sortByLayer) {
					// レイヤー別に階層表示（描画順序を保持）
					std::map<int, std::vector<std::pair<std::string, UIBase*>>> uiByLayer;

					// drawOrder_の順番でUIを収集
					for (const auto& id : drawOrder_) {
						auto it = uiElements_.find(id);
						if (it == uiElements_.end() || !it->second) continue;

						auto* ui = it->second.get();

						// フィルタリング
						if (strlen(filterText) > 0) {
							if (ui->GetName().find(filterText) == std::string::npos &&
								id.find(filterText) == std::string::npos) {
								continue;
							}
						}

						uiByLayer[ui->GetLayer()].push_back({ id, ui });
					}

					// レイヤーごとにツリー表示
					for (auto& [layer, uis] : uiByLayer) {
						ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.3f, 0.4f, 0.6f, 0.8f));

						bool layerOpen = ImGui::TreeNodeEx(
							(void*)(intptr_t)layer,
							ImGuiTreeNodeFlags_DefaultOpen,
							"レイヤー %d (%zu個)",
							layer,
							uis.size()
						);

						ImGui::PopStyleColor();

						if (layerOpen) {
							// レイヤー内での順序変更用
							std::string moveUpId = "";
							std::string moveDownId = "";

							for (size_t i = 0; i < uis.size(); ++i) {
								auto& [id, ui] = uis[i];
								ImGui::PushID(id.c_str());

								bool isSelected = (selectedUIId_ == id);
								bool visible = ui->IsVisible();

								// インデント
								ImGui::Indent(16.0f);

								// 上下ボタン（レイヤー内順序変更）
								ImGui::BeginGroup();
								if (i > 0) {
									if (ImGui::ArrowButton("##up", ImGuiDir_Up)) {
										moveUpId = id;
									}
									if (ImGui::IsItemHovered()) {
										ImGui::SetTooltip("レイヤー内で前面へ");
									}
								} else {
									ImGui::Dummy(ImVec2(18, 18));
								}
								ImGui::SameLine(0, 2);
								if (i < uis.size() - 1) {
									if (ImGui::ArrowButton("##down", ImGuiDir_Down)) {
										moveDownId = id;
									}
									if (ImGui::IsItemHovered()) {
										ImGui::SetTooltip("レイヤー内で背面へ");
									}
								} else {
									ImGui::Dummy(ImVec2(18, 18));
								}
								ImGui::EndGroup();

								ImGui::SameLine();

								// 表示/非表示チェックボックス
								if (ImGui::Checkbox("##visible", &visible)) {
									ui->SetVisible(visible);
								}

								ImGui::SameLine();

								// 選択可能なアイテム
								if (ImGui::Selectable(ui->GetName().c_str(), isSelected)) {
									selectedUIId_ = id;
								}

								// 右クリックメニュー
								if (ImGui::BeginPopupContextItem()) {
									if (ImGui::MenuItem("削除")) {
										uiToDelete = id;
									}
									if (ImGui::MenuItem("複製")) {
										std::string newId = GenerateUniqueID(id);
										auto newUI = std::make_unique<UIBase>(newId);
										newUI->Initialize("./Resources/UIConfigs/" + newId + ".json");
										newUI->CopyPropertiesFrom(ui);
										AddUI(newId, std::move(newUI));
									}
									ImGui::Separator();
									if (ImGui::MenuItem("前面へ移動")) {
										moveUpId = id;
									}
									if (ImGui::MenuItem("背面へ移動")) {
										moveDownId = id;
									}
									ImGui::EndPopup();
								}

								// ツールチップ
								if (ImGui::IsItemHovered()) {
									ImGui::BeginTooltip();
									ImGui::Text("ID: %s", id.c_str());
									auto pos = ui->GetPosition();
									ImGui::Text("位置: (%.1f, %.1f, %.1f)", pos.x, pos.y, pos.z);
									ImGui::Text("テクスチャ: %s", ui->GetTexturePath().c_str());
									// 描画順インデックスを表示
									auto orderIt = std::find(drawOrder_.begin(), drawOrder_.end(), id);
									if (orderIt != drawOrder_.end()) {
										size_t drawIndex = std::distance(drawOrder_.begin(), orderIt);
										ImGui::Text("描画順: %zu / %zu", drawIndex + 1, drawOrder_.size());
									}
									ImGui::EndTooltip();
								}

								ImGui::Unindent(16.0f);
								ImGui::PopID();
							}

							// レイヤー内順序変更の実行（描画順序を直接変更）
							if (!moveUpId.empty() && !moveDownId.empty()) {
								// 同時に2つは実行しない
								moveDownId.clear();
							}

							if (!moveUpId.empty()) {
								MoveDrawOrderForward(moveUpId);
							}

							if (!moveDownId.empty()) {
								MoveDrawOrderBackward(moveDownId);
							}

							ImGui::TreePop();
						}
					}
				} else {
					// 通常のリスト表示
					// 現在の描画順序でリストを作成
					std::vector<std::pair<std::string, UIBase*>> sortedUIs;
					for (const auto& id : drawOrder_) {
						auto it = uiElements_.find(id);
						if (it != uiElements_.end() && it->second) {
							auto* ui = it->second.get();

							// フィルタリング
							if (strlen(filterText) > 0) {
								if (ui->GetName().find(filterText) == std::string::npos &&
									id.find(filterText) == std::string::npos) {
									continue;
								}
							}

							sortedUIs.push_back({ id, ui });
						}
					}

					std::string moveUpId = "";
					std::string moveDownId = "";

					for (size_t i = 0; i < sortedUIs.size(); ++i) {
						auto& [id, ui] = sortedUIs[i];
						ImGui::PushID(id.c_str());

						bool isSelected = (selectedUIId_ == id);
						bool visible = ui->IsVisible();

						// 描画順変更ボタン
						ImGui::BeginGroup();
						if (i > 0) {
							if (ImGui::ArrowButton("##up", ImGuiDir_Up)) {
								moveUpId = id;
							}
							if (ImGui::IsItemHovered()) {
								ImGui::SetTooltip("前面へ");
							}
						} else {
							ImGui::Dummy(ImVec2(18, 18));
						}
						ImGui::SameLine(0, 2);
						if (i < sortedUIs.size() - 1) {
							if (ImGui::ArrowButton("##down", ImGuiDir_Down)) {
								moveDownId = id;
							}
							if (ImGui::IsItemHovered()) {
								ImGui::SetTooltip("背面へ");
							}
						} else {
							ImGui::Dummy(ImVec2(18, 18));
						}
						ImGui::EndGroup();
						ImGui::SameLine();

						// 表示/非表示チェックボックス
						if (ImGui::Checkbox("##visible", &visible)) {
							ui->SetVisible(visible);
						}

						ImGui::SameLine();

						// 選択可能なアイテム
						std::string displayName = ui->GetName() + " [L:" + std::to_string(ui->GetLayer()) + "]";
						if (ImGui::Selectable(displayName.c_str(), isSelected)) {
							selectedUIId_ = id;
						}

						// 右クリックメニュー
						if (ImGui::BeginPopupContextItem()) {
							if (ImGui::MenuItem("削除")) {
								uiToDelete = id;
							}
							if (ImGui::MenuItem("複製")) {
								std::string newId = GenerateUniqueID(id);
								auto newUI = std::make_unique<UIBase>(newId);
								newUI->Initialize("./Resources/UIConfigs/" + newId + ".json");
								newUI->CopyPropertiesFrom(ui);
								AddUI(newId, std::move(newUI));
							}
							ImGui::Separator();
							if (ImGui::MenuItem("前面へ移動")) {
								moveUpId = id;
							}
							if (ImGui::MenuItem("背面へ移動")) {
								moveDownId = id;
							}
							ImGui::EndPopup();
						}

						// ツールチップ
						if (ImGui::IsItemHovered()) {
							ImGui::BeginTooltip();
							ImGui::Text("ID: %s", id.c_str());
							ImGui::Text("レイヤー: %d", ui->GetLayer());
							auto pos = ui->GetPosition();
							ImGui::Text("位置: (%.1f, %.1f, %.1f)", pos.x, pos.y, pos.z);
							// 描画順インデックスを表示
							auto orderIt = std::find(drawOrder_.begin(), drawOrder_.end(), id);
							if (orderIt != drawOrder_.end()) {
								size_t drawIndex = std::distance(drawOrder_.begin(), orderIt);
								ImGui::Text("描画順: %zu / %zu", drawIndex + 1, drawOrder_.size());
							}
							ImGui::EndTooltip();
						}

						ImGui::PopID();
					}

					// 描画順変更処理
					if (!moveUpId.empty()) {
						MoveDrawOrderForward(moveUpId);
					}

					if (!moveDownId.empty()) {
						MoveDrawOrderBackward(moveDownId);
					}
				}

				// ループ外で削除
				if (!uiToDelete.empty()) {
					RemoveUI(uiToDelete);
					if (selectedUIId_ == uiToDelete) {
						selectedUIId_.clear();
					}
				}

				ImGui::EndChild();

				ImGui::SameLine();

				// 右側: UI詳細編集 (幅75% - 広めに)
				ImGui::BeginChild("UIDetails", ImVec2(0, 0), true);

				if (!selectedUIId_.empty()) {
					auto* selectedUI = GetUI(selectedUIId_);
					if (selectedUI) {
						// ヘッダー部分を強調
						ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 0.8f, 1.0f, 1.0f));
						ImGui::Text("編集中: %s", selectedUI->GetName().c_str());
						ImGui::PopStyleColor();

						ImGui::SameLine();
						ImGui::TextDisabled("(ID: %s)", selectedUIId_.c_str());

						ImGui::Separator();
						ImGui::Spacing();

						// スクロール可能な編集エリア
						ImGui::BeginChild("EditArea", ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);

						// --- ID編集ブロック ---
						ImGui::Separator();
						ImGui::Text("ID 設定");

						static char idEditBuf[128] = {};
						// バッファ初期化（選択が変わったら詰め替え）
						static std::string lastSelectedForBuf;
						if (lastSelectedForBuf != selectedUIId_) {
							memset(idEditBuf, 0, sizeof(idEditBuf));
							strncpy_s(idEditBuf, selectedUIId_.c_str(), sizeof(idEditBuf) - 1);
							lastSelectedForBuf = selectedUIId_;
						}

						ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 120.0f);
						ImGui::InputText("##id_edit", idEditBuf, sizeof(idEditBuf));
						ImGui::SameLine();
						if (ImGui::Button("ID変更", ImVec2(110, 0))) {
							std::string newId = idEditBuf;
							if (!newId.empty() && newId != selectedUIId_) {
								if (RenameUI(selectedUIId_, newId)) {
									// 成功したらバッファも同期
									strncpy_s(idEditBuf, selectedUIId_.c_str(), sizeof(idEditBuf) - 1);
									// すぐ保存しておきたい場合はここで現在シーン名に依存しない「UI設定のみ保存」も可
									// （今回は SaveScene 時に各UIのID名で保存される設計にしています）
								}
							}
						}

						// 参考表示：このUIが保存される設定パス（プレビュー）
						ImGui::TextDisabled("保存先プレビュー: %s%s.json", UI_CONFIG_DIRECTORY.c_str(), selectedUIId_.c_str());

						ImGui::Spacing();
						ImGui::Separator();
						// --- ここまでID編集ブロック ---

						// 既存のテクスチャ選択UI・個別UIのImGui
						DisplayImprovedTextureSelector(selectedUI);
						selectedUI->ImGUi();

						ImGui::EndChild();
					}
				} else {
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
					ImGui::SetCursorPosY(ImGui::GetWindowHeight() * 0.4f);

					float textWidth = ImGui::CalcTextSize("UIを選択してください").x;
					ImGui::SetCursorPosX((ImGui::GetWindowWidth() - textWidth) * 0.5f);
					ImGui::Text("UIを選択してください");

					ImGui::PopStyleColor();
				}

				ImGui::EndChild();

				ImGui::EndTabItem();
			}

			// ===== シーン管理タブ =====
			if (ImGui::BeginTabItem("シーン管理")) {

				static char sceneName[128] = "";
				ImGui::InputTextWithHint("##scenename", "シーン名を入力", sceneName, sizeof(sceneName));

				if (ImGui::Button("現在のレイアウトを保存", ImVec2(-1, 0))) {
					if (strlen(sceneName) > 0) {
						if (SaveScene(sceneName)) {
							ImGui::OpenPopup("SceneSaved");
						}
					}
				}

				if (ImGui::BeginPopupModal("SceneSaved", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
					ImGui::Text("シーンを保存しました!");
					if (ImGui::Button("OK", ImVec2(120, 0))) {
						ImGui::CloseCurrentPopup();
					}
					ImGui::EndPopup();
				}

				ImGui::Separator();
				ImGui::Text("保存済みシーン:");

				auto scenes = GetAvailableScenes();
				for (const auto& scene : scenes) {
					ImGui::PushID(scene.c_str());

					if (ImGui::Button("読込", ImVec2(60, 0))) {
						LoadScene(scene);
					}
					ImGui::SameLine();

					if (ImGui::Button("削除", ImVec2(60, 0))) {
						ImGui::OpenPopup("ConfirmDelete");
					}

					ImGui::SameLine();
					ImGui::Text("%s", scene.c_str());

					// 削除確認ポップアップ
					if (ImGui::BeginPopupModal("ConfirmDelete", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
						ImGui::Text("シーン '%s' を削除しますか?", scene.c_str());
						ImGui::Separator();

						if (ImGui::Button("はい", ImVec2(120, 0))) {
							DeleteScene(scene);
							ImGui::CloseCurrentPopup();
						}
						ImGui::SameLine();
						if (ImGui::Button("いいえ", ImVec2(120, 0))) {
							ImGui::CloseCurrentPopup();
						}
						ImGui::EndPopup();
					}

					ImGui::PopID();
				}

				if (scenes.empty()) {
					ImGui::TextDisabled("保存されたシーンがありません");
				}

				ImGui::EndTabItem();
			}

			// ===== グループ管理タブ =====
			if (ImGui::BeginTabItem("グループ管理")) {

				static char newGroupName[128] = "";
				ImGui::InputTextWithHint("##groupname", "新規グループ名", newGroupName, sizeof(newGroupName));

				ImGui::Separator();

				for (auto& [groupName, uiIds] : groups_) {
					if (ImGui::TreeNode(groupName.c_str())) {
						ImGui::Text("UI数: %zu", uiIds.size());

						if (ImGui::Button("表示")) {
							ShowGroup(groupName, true);
						}
						ImGui::SameLine();
						if (ImGui::Button("非表示")) {
							ShowGroup(groupName, false);
						}

						ImGui::Separator();

						for (const auto& uiId : uiIds) {
							auto* ui = GetUI(uiId);
							if (ui) {
								ImGui::BulletText("%s", ui->GetName().c_str());
							}
						}

						ImGui::TreePop();
					}
				}

				if (groups_.empty()) {
					ImGui::TextDisabled("グループがありません");
				}

				ImGui::EndTabItem();
			}

			// ===== 統計情報タブ =====
			if (ImGui::BeginTabItem("統計情報")) {

				auto stats = GetStatistics();

				ImGui::Text("総UI数: %d", stats.totalUIs);
				ImGui::Text("表示中: %d", stats.visibleUIs);
				ImGui::Text("非表示: %d", stats.hiddenUIs);

				ImGui::Separator();
				ImGui::Text("レイヤー別:");

				for (const auto& [layer, count] : stats.uisByLayer) {
					ImGui::BulletText("レイヤー %d: %d個", layer, count);
				}

				ImGui::EndTabItem();
			}

			ImGui::EndTabBar();
		}

#endif
	}

	/*==================================================================
							ヘルパー関数
	===================================================================*/

	void UIManager::DisplayImprovedTextureSelector(UIBase* ui)
	{
		if (!ui) return;
#ifdef USE_IMGUI


		if (ImGui::CollapsingHeader("テクスチャ設定", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Indent(10.0f);

			// =========================================================
			// 現在のテクスチャ
			// =========================================================
			std::string currentTexture = ui->GetTexturePath();

			ImGui::Text("現在のテクスチャ:");
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f),
				currentTexture.empty() ? "(なし)" : currentTexture.c_str());

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			// =========================================================
			// 検索バー
			// =========================================================
			static char textureFilter[128] = "";
			ImGui::PushItemWidth(-1);
			ImGui::InputTextWithHint("##texturefilter", "テクスチャを検索...", textureFilter, sizeof(textureFilter));
			ImGui::PopItemWidth();
			ImGui::Spacing();

			// =========================================================
			// ディレクトリ確認
			// =========================================================
			const std::string textureDir = "./Resources/Textures/";
			if (!std::filesystem::exists(textureDir))
			{
				ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "テクスチャフォルダが見つかりません");
				return;
			}

			// =========================================================
			// スクロール領域
			// =========================================================
			ImGui::BeginChild("TextureList", ImVec2(0, 280), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);

			auto IsTextureExt = [](const std::string& ext)
				{
					static const std::vector<std::string> validExt = { ".png", ".jpg", ".jpeg", ".bmp", ".tga",".dds" };
					return std::find(validExt.begin(), validExt.end(), ext) != validExt.end();
				};

			// =========================================================
			// 再帰ディレクトリ描画（クリック展開対応）
			// =========================================================
			std::function<void(const std::filesystem::path&)> drawDirectory =
				[&](const std::filesystem::path& path)
				{
					std::vector<std::filesystem::directory_entry> entries;
					for (const auto& e : std::filesystem::directory_iterator(path))
						entries.push_back(e);

					std::sort(entries.begin(), entries.end(),
						[](const auto& a, const auto& b)
						{
							// フォルダ優先、その中で名前順
							if (a.is_directory() != b.is_directory())
								return a.is_directory() > b.is_directory();
							return a.path().filename().string() < b.path().filename().string();
						});

					for (const auto& e : entries)
					{
						std::string name = e.path().filename().string();
						std::string ext = e.path().extension().string();
						std::string rel = std::filesystem::relative(e.path(), textureDir).string();
						std::replace(rel.begin(), rel.end(), '\\', '/');
						std::string fullPath = "Resources/Textures/" + rel;

						ImGui::PushID(fullPath.c_str());

						// ===== フォルダ =====
						if (e.is_directory())
						{
							// ★クリックで展開できるように設定
							bool open = ImGui::TreeNodeEx(
								name.c_str(),
								ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth,
								"[DIR] %s", name.c_str());

							if (open)
							{
								drawDirectory(e.path());
								ImGui::TreePop();
							}
						}
						// ===== テクスチャファイル =====
						else if (e.is_regular_file() && IsTextureExt(ext))
						{
							// 検索フィルタ
							if (strlen(textureFilter) > 0 && name.find(textureFilter) == std::string::npos)
							{
								ImGui::PopID();
								continue;
							}

							bool isSelected = (currentTexture == fullPath);

							if (isSelected)
							{
								ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 1.0f, 0.4f, 1.0f));
								ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.2f, 0.5f, 0.2f, 0.5f));
							}

							if (ImGui::Selectable(name.c_str(), isSelected))
							{
								ui->SetTexture(fullPath);
							}

							if (isSelected)
								ImGui::PopStyleColor(2);
						}

						ImGui::PopID();
					}
				};
			drawDirectory(textureDir);

			ImGui::EndChild();
			ImGui::Unindent(10.0f);
		}
#endif // _DEBUG
	}




	void UIManager::DisplayTextureDirectory(const std::string& path, const std::string& baseDir, UIBase* ui, const char* filter) {
		if (!std::filesystem::exists(path)) return;
#ifdef USE_IMGUI


		// ファイルとディレクトリを分けて収集
		std::vector<std::filesystem::directory_entry> dirs;
		std::vector<std::filesystem::directory_entry> files;

		for (const auto& entry : std::filesystem::directory_iterator(path)) {
			if (entry.is_directory()) {
				dirs.push_back(entry);
			} else if (entry.is_regular_file()) {
				// 画像ファイルのみ
				std::string ext = entry.path().extension().string();
				if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".tga") {
					files.push_back(entry);
				}
			}
		}

		// ディレクトリをソート
		std::sort(dirs.begin(), dirs.end(), [](const auto& a, const auto& b) {
			return a.path().filename().string() < b.path().filename().string();
			});

		// ファイルをソート
		std::sort(files.begin(), files.end(), [](const auto& a, const auto& b) {
			return a.path().filename().string() < b.path().filename().string();
			});

		// ディレクトリを階層表示
		for (const auto& entry : dirs) {
			std::string folderName = entry.path().filename().string();

			// フォルダアイコンと色付き表示
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.3f, 1.0f));
			bool isOpen = ImGui::TreeNodeEx(
				entry.path().string().c_str(),
				ImGuiTreeNodeFlags_None,
				"📁 %s",
				folderName.c_str()
			);
			ImGui::PopStyleColor();

			if (isOpen) {
				DisplayTextureDirectory(entry.path().string(), baseDir, ui, filter);
				ImGui::TreePop();
			}
		}

		// ファイルを表示
		for (const auto& entry : files) {
			std::string filename = entry.path().filename().string();
			std::string relativePath = std::filesystem::relative(entry.path(), baseDir).string();

			// フィルター適用
			if (strlen(filter) > 0) {
				if (filename.find(filter) == std::string::npos) {
					continue;
				}
			}

			// 現在選択中のテクスチャはハイライト
			bool isSelected = (ui->GetTexturePath().find(filename) != std::string::npos);

			if (isSelected) {
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 1.0f, 0.3f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.2f, 0.5f));
			}

			// ファイル拡張子アイコン
			std::string ext = entry.path().extension().string();
			std::string icon = "🖼️";

			// 選択可能なボタンとして表示
			std::string buttonLabel = icon + " " + filename;
			if (ImGui::Selectable(buttonLabel.c_str(), isSelected, 0, ImVec2(-1, 0))) {
				// 相対パスで設定
				std::string fullPath = "./Resources/Textures/" + relativePath;
				// バックスラッシュをスラッシュに変換
				std::replace(fullPath.begin(), fullPath.end(), '\\', '/');
				ui->SetTexture(fullPath);
			}

			if (isSelected) {
				ImGui::PopStyleColor(2);
			}

			// ホバー時に詳細情報を表示
			if (ImGui::IsItemHovered()) {
				ImGui::BeginTooltip();
				ImGui::Text("ファイル名: %s", filename.c_str());
				ImGui::Text("パス: %s", relativePath.c_str());

				// ファイルサイズを表示
				auto fileSize = std::filesystem::file_size(entry.path());
				if (fileSize < 1024) {
					ImGui::Text("サイズ: %zu bytes", fileSize);
				} else if (fileSize < 1024 * 1024) {
					ImGui::Text("サイズ: %.2f KB", fileSize / 1024.0);
				} else {
					ImGui::Text("サイズ: %.2f MB", fileSize / (1024.0 * 1024.0));
				}

				ImGui::EndTooltip();
			}
		}
#else
		(void)path;
		(void)baseDir;
		(void)ui;
		(void)filter;
#endif

	}

	void UIManager::RebuildDrawOrder() {
		drawOrder_.clear();

		std::vector<std::pair<std::string, int>> uiWithLayer;
		for (const auto& [id, ui] : uiElements_) {
			if (ui) {
				uiWithLayer.push_back({ id, ui->GetLayer() });
			}
		}

		std::sort(uiWithLayer.begin(), uiWithLayer.end(),
			[](const auto& a, const auto& b) {
				return a.second < b.second;
			});

		for (const auto& [id, layer] : uiWithLayer) {
			drawOrder_.push_back(id);
		}
	}

	void UIManager::MoveDrawOrderForward(const std::string& uiId) {
		auto it = std::find(drawOrder_.begin(), drawOrder_.end(), uiId);
		if (it != drawOrder_.end() && it != drawOrder_.begin()) {
			// 1つ前の要素と入れ替え（描画順で前面に = インデックスが後ろに）
			std::iter_swap(it, it - 1);
		}
	}

	void UIManager::MoveDrawOrderBackward(const std::string& uiId) {
		auto it = std::find(drawOrder_.begin(), drawOrder_.end(), uiId);
		if (it != drawOrder_.end() && it + 1 != drawOrder_.end()) {
			// 1つ後ろの要素と入れ替え（描画順で背面に = インデックスが前に）
			std::iter_swap(it, it + 1);
		}
	}

	std::string UIManager::GenerateUniqueID(const std::string& baseName) {
		std::string id = baseName;
		int counter = 1;

		while (HasUI(id)) {
			id = baseName + "_" + std::to_string(counter);
			counter++;
		}

		return id;
	}

	std::string UIManager::GetSceneConfigDir(const std::string& sceneName)
	{
		return UI_CONFIG_DIRECTORY + sceneName + "/";
	}
}