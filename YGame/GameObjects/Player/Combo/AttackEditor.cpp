#include "AttackEditor.h"
#include <algorithm>

#include <Debugger/Logger.h>
#ifdef USE_IMGUI
#include <imgui.h>
#endif

using json = nlohmann::json;

AttackDataEditor::AttackDataEditor()
{
	// デフォルトでは AttackDatabase の中身を編集
	attacks_ = &AttackDatabase::Get();

	std::fill(std::begin(nameBuffer_), std::end(nameBuffer_), '\0');
}

void AttackDataEditor::SetTarget(std::vector<AttackData>* list)
{
	attacks_ = list;
	currentIndex_ = (attacks_ && !attacks_->empty()) ? 0 : -1;
}

void AttackDataEditor::SetFilePath(const std::string& path)
{
	filePath_ = path;

	std::string msg = "[AttackEditor] File path set to: " + filePath_ + "\n";
	Logger(msg.c_str());
}

void AttackDataEditor::SetReloadCallback(std::function<void()> callback)
{
	onReloadCallback_ = callback;
}

void AttackDataEditor::DrawImGui()
{
#ifdef USE_IMGUI

	DrawToolbar();

	ImGui::Separator();

	// 左右2カラム
	ImGui::Columns(2, nullptr, true);

	DrawAttackList();

	ImGui::NextColumn();

	DrawAttackDetail();

	ImGui::Columns(1);
#endif

}

void AttackDataEditor::DrawToolbar()
{
#ifdef USE_IMGUI
	if (ImGui::Button("保存"))
	{
		SaveToJson();
		TriggerReload();
	}
	ImGui::SameLine();
	if (ImGui::Button("読み込み"))
	{
		LoadFromJson();
		TriggerReload();
	}
	ImGui::SameLine();
	if (ImGui::Button("保存 & リロード"))
	{
		SaveToJson();
		TriggerReload();
	}

	ImGui::SameLine();
	// ファイルパスを表示
	ImGui::Text("ファイル: %s", filePath_.c_str());
	ImGui::SameLine();
	// 攻撃 (attacks) の数を表示
	ImGui::Text("| 攻撃数: %d", attacks_ ? static_cast<int>(attacks_->size()) : 0);
	ImGui::Separator();
	if (ImGui::Checkbox("編集時に自動リロード", &autoReload_))
	{
		if (autoReload_)
		{
			Logger("[AttackEditor] 自動リロードが有効になりました\n");
		} else
		{
			Logger("[AttackEditor] 自動リロードが無効になりました\n");
		}
	}
	ImGui::SameLine();
	ImGui::TextDisabled("(?)");
	if (ImGui::IsItemHovered())
	{
		ImGui::BeginTooltip();
		ImGui::Text("編集時に攻撃設定を自動的にリロードします");
		ImGui::EndTooltip();
	}
#endif
}

void AttackDataEditor::DrawAttackList()
{
#ifdef USE_IMGUI

	if (!attacks_)
	{
		ImGui::Text("攻撃リストがありません。");
		return;
	}

	ImGui::Text("攻撃数 (%d)", static_cast<int>(attacks_->size()));
	ImGui::Separator();

	// 攻撃タイプごとにリストを分類するためのマップ (例: A_Arte, B_Arte, Arcane_Arte)
	std::map<AttackType, std::vector<int>> categorizedAttacks;
	for (int i = 0; i < static_cast<int>(attacks_->size()); ++i)
	{
		categorizedAttacks[attacks_->at(i).type].push_back(i);
	}

	// 攻撃タイプ名配列（DrawAttackDetailから再利用）
	static const char* attackTypes[] = { "A技 (軽)", "B技 (重)", "奥義 (究極)" };

	for (int typeIndex = 0; typeIndex < 3; ++typeIndex)
	{
		AttackType type = static_cast<AttackType>(typeIndex);

		// 攻撃タイプをヘッダーとして表示
		if (ImGui::CollapsingHeader(attackTypes[typeIndex], ImGuiTreeNodeFlags_DefaultOpen))
		{
			// そのタイプに属する攻撃をループ
			for (int i : categorizedAttacks[type])
			{
				const bool isSelected = (i == currentIndex_);
				const std::string label = attacks_->at(i).name + "##attack_" + std::to_string(i);

				// 🚀 選択中の攻撃を赤くする (カラーフィードバック)
				if (isSelected)
				{
					// 選択中の色を赤に設定（例: R=1.0, G=0.2, B=0.2）
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.2f, 0.2f, 1.0f));
				}

				if (ImGui::Selectable(label.c_str(), isSelected))
				{
					currentIndex_ = i;
				}

				// 🚀 色を元に戻す
				if (isSelected)
				{
					ImGui::PopStyleColor();
				}
			}
		}
	}
	ImGui::Separator();

	if (ImGui::Button("新規作成"))
	{
		NewAttack();
		if (autoReload_) TriggerReload();
	}
	ImGui::SameLine();

	if (ImGui::Button("複製"))
	{
		DuplicateAttack();
		if (autoReload_) TriggerReload();
	}
	ImGui::SameLine();

	if (ImGui::Button("削除"))
	{
		DeleteAttack();
		if (autoReload_) TriggerReload();
	}

#endif
}

void AttackDataEditor::DrawAttackDetail()
{
#ifdef USE_IMGUI

	// 攻撃リストが存在しない、または無効なインデックスが選択されている場合
	if (!attacks_ || currentIndex_ < 0 || currentIndex_ >= static_cast<int>(attacks_->size()))
	{
		// 「攻撃が選択されていません」と表示
		ImGui::Text("攻撃が選択されていません。");
		return;
	}

	// 選択された攻撃データへの参照を取得
	AttackData& attack = attacks_->at(currentIndex_);

	// 攻撃タイプの名前配列
	static const char* attackTypes[] = { "A技 (軽)", "B技 (重)", "奥義 (究極)" };

	// 「詳細」セクションのヘッダー
	ImGui::Text("詳細");
	ImGui::Separator();

	// 変更フラグ
	bool changed = false;

	// 名前
	{
		// 現在の攻撃名をバッファにコピー
		std::snprintf(nameBuffer_, sizeof(nameBuffer_), "%s", attack.name.c_str());
		// 「名前」のテキスト入力フィールド
		if (ImGui::InputText("名前", nameBuffer_, sizeof(nameBuffer_)))
		{
			// 入力内容を攻撃名に反映
			attack.name = nameBuffer_;
			changed = true;
		}
	}

	ImGui::Separator();

	//------------------------------------------------------------
	// 基本情報
	//------------------------------------------------------------
	// 折りたたみヘッダー (デフォルトで開いた状態)
	if (ImGui::CollapsingHeader("基本情報", ImGuiTreeNodeFlags_None))
	{
		char animBuffer[256];
		// 現在のアニメーション名をバッファにコピー
		std::snprintf(animBuffer, sizeof(animBuffer), "%s", attack.animationName.c_str());
		// 「アニメーション名」のテキスト入力フィールド
		if (ImGui::InputText("アニメーション名", animBuffer, sizeof(animBuffer)))
		{
			attack.animationName = animBuffer;
			changed = true;
		}

		int currentType = static_cast<int>(attack.type);
		// 「タイプ」のコンボボックス
		if (ImGui::Combo("タイプ", &currentType, attackTypes, 3))
		{
			// 選択内容を攻撃タイプに反映
			attack.type = static_cast<AttackType>(currentType);
			changed = true;
		}
	}

	//------------------------------------------------------------
	// タイミング設定
	//------------------------------------------------------------
	// 折りたたみヘッダー (デフォルトで開いた状態)
	if (ImGui::CollapsingHeader("タイミング", ImGuiTreeNodeFlags_None))
	{
		changed |= ImGui::InputFloat("持続時間", &attack.duration, 0.01f, 0.1f, "%.2f");
		changed |= ImGui::InputFloat("硬直時間", &attack.recovery, 0.01f, 0.1f, "%.2f");
		changed |= ImGui::InputFloat("継続受付時間", &attack.continueWindow, 0.01f, 0.1f, "%.2f");
		changed |= ImGui::InputFloat("モーション速度", &attack.motionSpeed, 0.01f, 0.1f, "%.2f");
	}

	//------------------------------------------------------------
	// ダメージ・効果
	//------------------------------------------------------------
	if (ImGui::CollapsingHeader("ダメージ & 効果", ImGuiTreeNodeFlags_None))
	{
		changed |= ImGui::InputFloat("基本ダメージ", &attack.baseDamage, 1.0f, 10.0f, "%.1f");
		changed |= ImGui::InputFloat("ノックバック", &attack.knockback, 0.1f, 1.0f, "%.1f");
		changed |= ImGui::InputFloat("ノックバック持続時間", &attack.knockbackDuaration, 0.1f, 1.0f, "%.2f");
		changed |= ImGui::InputFloat3("攻撃範囲", &attack.attackRange.x);

		char effectBuffer[256];
		std::snprintf(effectBuffer, sizeof(effectBuffer), "%s", attack.effect.c_str());
		//if (ImGui::InputText("効果", effectBuffer, sizeof(effectBuffer)))
		//{
		//	attack.effect = effectBuffer;
		//	changed = true;
		//}
	}

	//------------------------------------------------------------
	// CC設定
	//------------------------------------------------------------
	if (ImGui::CollapsingHeader("CCシステム"))
	{
		changed |= ImGui::InputInt("CC消費", &attack.ccCost);
		changed |= ImGui::InputInt("CCヒット時回復", &attack.ccOnHit);
	}

	//------------------------------------------------------------
	// コンボ特性
	//------------------------------------------------------------
	if (ImGui::CollapsingHeader("コンボ特性"))
	{
		changed |= ImGui::Checkbox("キャンセル可能", &attack.canCancel);
		changed |= ImGui::Checkbox("任意に連携可能", &attack.canChainToAny);
		// ツリーノード
		if (ImGui::TreeNode("推奨次攻撃"))
		{
			// 推奨次攻撃のリストをループ処理
			for (size_t i = 0; i < attack.preferredNext.size(); ++i)
			{
				ImGui::PushID(static_cast<int>(i));

				int currentPreferred = static_cast<int>(attack.preferredNext[i]);
				// 推奨攻撃タイプのコンボボックス
				if (ImGui::Combo(("##" + std::to_string(i)).c_str(), &currentPreferred, attackTypes, 3))
				{
					attack.preferredNext[i] = static_cast<AttackType>(currentPreferred);
					changed = true;
				}

				ImGui::SameLine();
				// 削除ボタン
				if (ImGui::Button("X"))
				{
					// 要素を削除
					attack.preferredNext.erase(attack.preferredNext.begin() + i);
					changed = true;
					ImGui::PopID();
					break;
				}
				ImGui::PopID();
			}
			if (ImGui::Button("推奨を追加"))
			{
				attack.preferredNext.push_back(AttackType::A_Arte);
				changed = true;
			}

			ImGui::TreePop();
		}
	}

	//------------------------------------------------------------
	// 特殊効果
	//------------------------------------------------------------
	//if (ImGui::CollapsingHeader("特殊効果"))
	//{
	//	changed |= ImGui::Checkbox("打ち上げ", &attack.launches);
	//	changed |= ImGui::Checkbox("壁バウンド", &attack.wallBounce);
	//	changed |= ImGui::Checkbox("地面バウンド", &attack.groundBounce);
	//}

	// 自動リロード
	if (changed && autoReload_)
	{
		SaveToJson();
		TriggerReload();
	}
#endif
}

void AttackDataEditor::NewAttack()
{
	if (!attacks_)
	{
		return;
	}

	AttackData data;

	// デフォルト値を設定
	data.name = "NewAttack_" + std::to_string(attacks_->size());
	data.animationName = "Idle";
	data.type = AttackType::A_Arte;
	data.duration = 0.3f;
	data.recovery = 0.2f;
	data.continueWindow = 0.3f;
	data.baseDamage = 30.0f;
	data.knockback = 5.0f;
	data.knockbackDuaration = 0.5f;
	data.attackRange = { 2.0f, 1.0f, 1.5f };
	data.ccCost = 1;
	data.ccOnHit = 0;
	data.canCancel = true;
	data.canChainToAny = true;
	data.launches = false;
	data.wallBounce = false;
	data.groundBounce = false;
	data.effect = "";
	data.motionSpeed = 1.0f;

	attacks_->push_back(data);
	currentIndex_ = static_cast<int>(attacks_->size()) - 1;

	Logger("[AttackEditor] New attack created\n");
}

void AttackDataEditor::DuplicateAttack()
{
	if (!attacks_ || currentIndex_ < 0 || currentIndex_ >= static_cast<int>(attacks_->size()))
	{
		return;
	}

	AttackData copy = attacks_->at(currentIndex_);
	copy.name += "_copy";

	attacks_->push_back(copy);
	currentIndex_ = static_cast<int>(attacks_->size()) - 1;

	Logger("[AttackEditor] Attack duplicated\n");
}

void AttackDataEditor::DeleteAttack()
{
	if (!attacks_ || currentIndex_ < 0 || currentIndex_ >= static_cast<int>(attacks_->size()))
	{
		return;
	}

	attacks_->erase(attacks_->begin() + currentIndex_);

	if (currentIndex_ >= static_cast<int>(attacks_->size()))
	{
		currentIndex_ = static_cast<int>(attacks_->size()) - 1;
	}

	Logger("[AttackEditor] Attack deleted\n");
}

void AttackDataEditor::MoveUp()
{
	if (!attacks_ || currentIndex_ <= 0 || currentIndex_ >= static_cast<int>(attacks_->size()))
	{
		return;
	}

	std::swap(attacks_->at(currentIndex_), attacks_->at(currentIndex_ - 1));
	--currentIndex_;
}

void AttackDataEditor::MoveDown()
{
	if (!attacks_ || currentIndex_ < 0 || currentIndex_ + 1 >= static_cast<int>(attacks_->size()))
	{
		return;
	}

	std::swap(attacks_->at(currentIndex_), attacks_->at(currentIndex_ + 1));
	++currentIndex_;
}

void AttackDataEditor::LoadFromJson()
{
	Logger("[AttackEditor] ===== Load Start =====\n");

	if (!attacks_)
	{
		Logger("[AttackEditor] ERROR: attacks_ is null!\n");
		return;
	}

	std::string msg = "[AttackEditor] Loading from: " + filePath_ + "\n";
	Logger(msg.c_str());

	bool loadResult = AttackDatabase::LoadFromFile(filePath_);

	msg = "[AttackEditor] LoadFromFile result: " + std::string(loadResult ? "SUCCESS" : "FAILED") + "\n";
	Logger(msg.c_str());

	if (loadResult)
	{
		attacks_ = &AttackDatabase::Get();

		msg = "[AttackEditor] After load - New attacks count: " + std::to_string(attacks_->size()) + "\n";
		Logger(msg.c_str());

		if (!attacks_->empty())
		{
			currentIndex_ = std::clamp(currentIndex_, 0, static_cast<int>(attacks_->size()) - 1);
		} else
		{
			currentIndex_ = -1;
		}

		Logger("[AttackEditor] ===== Load Success =====\n");
	} else
	{
		Logger("[AttackEditor] ===== Load Failed =====\n");
	}
}

void AttackDataEditor::SaveToJson()
{
	Logger("[AttackEditor] ===== Save Start =====\n");

	std::string msg = "[AttackEditor] Saving to: " + filePath_ + "\n";
	Logger(msg.c_str());

	msg = "[AttackEditor] Attack count: " + std::to_string(attacks_ ? attacks_->size() : 0) + "\n";
	Logger(msg.c_str());

	bool saveResult = AttackDatabase::SaveToFile(filePath_);

	msg = "[AttackEditor] SaveToFile result: " + std::string(saveResult ? "SUCCESS" : "FAILED") + "\n";
	Logger(msg.c_str());

	if (saveResult)
	{
		Logger("[AttackEditor] ===== Save Success =====\n");
	} else
	{
		Logger("[AttackEditor] ===== Save Failed =====\n");
	}
}

void AttackDataEditor::TriggerReload()
{
	if (onReloadCallback_)
	{
		Logger("[AttackEditor] Triggering reload callback...\n");
		onReloadCallback_();
		Logger("[AttackEditor] Reload callback completed\n");
	}
}