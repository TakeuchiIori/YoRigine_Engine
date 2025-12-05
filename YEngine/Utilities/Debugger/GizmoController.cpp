#include "GizmoController.h"
#include "Systems/Camera/Camera.h"

///************************* 基本関数 *************************///
#ifdef USE_IMGUI


void GizmoController::Initialize()
{
#ifdef USE_IMGUI
	// ImGuiコンテキスト設定
	ImGuizmo::SetImGuiContext(ImGui::GetCurrentContext());
	isInitialized_ = true;
#endif
}

bool GizmoController::DrawGizmo(
	Camera* camera,
	Transform& transform,
	Matrix4x4& worldMatrix,
	const ImVec2& viewportPos,
	const ImVec2& viewportSize)
{
#ifdef USE_IMGUI
	if (!isInitialized_ || !camera) return false;

	// カメラのビュー・プロジェクション行列を取得
	Matrix4x4 viewMatrix = camera->GetViewMatrix();
	Matrix4x4 projectionMatrix = camera->GetProjectionMatrix();

	// ImGuizmo用の配列形式に変換
	float view[16], proj[16], model[16];
	MatrixToImGuizmo(viewMatrix, view);
	MatrixToImGuizmo(projectionMatrix, proj);
	MatrixToImGuizmo(worldMatrix, model);

	// ビューポート設定とフレーム初期化
	ImGuizmo::SetRect(viewportPos.x, viewportPos.y, viewportSize.x, viewportSize.y);
	ImGuizmo::BeginFrame();

	// 操作モードを変換
	ImGuizmo::OPERATION operation;
	switch (settings_.operation)
	{
	case Operation::Translate: operation = ImGuizmo::TRANSLATE; break;
	case Operation::Rotate:    operation = ImGuizmo::ROTATE;    break;
	case Operation::Scale:     operation = ImGuizmo::SCALE;     break;
	default:                   operation = ImGuizmo::TRANSLATE; break;
	}

	// 座標系モードを変換
	ImGuizmo::MODE mode = (settings_.mode == Mode::World) ? ImGuizmo::WORLD : ImGuizmo::LOCAL;

	// スナップ値の設定
	float* snap = nullptr;
	float snapRad = DegToRad(settings_.rotationSnapDegrees);
	float snapArray[3] = { settings_.snapValues.x, settings_.snapValues.y, settings_.snapValues.z };

	if (settings_.useSnap)
	{
		snap = (settings_.operation == Operation::Rotate) ? &snapRad : snapArray;
	}

	// ギズモを描画して操作を受け付ける
	bool manipulated = ImGuizmo::Manipulate(
		view, proj,
		operation,
		mode,
		model,
		nullptr,
		snap
	);

	// ギズモ操作中はマウスキャプチャを無効化
	if (ImGuizmo::IsUsing())
	{
		ImGui::GetIO().WantCaptureMouse = false;
	}

	// 操作があった場合、トランスフォームを更新
	if (manipulated)
	{
		worldMatrix = ImGuizmoToMatrix(model);
		DecomposeMatrix(worldMatrix, transform);
		return true;
	}

	return false;
#else
	return false;
#endif
}

bool GizmoController::DrawGizmoMulti(
	Camera* camera,
	std::vector<Transform*>& transforms,
	const ImVec2& viewportPos,
	const ImVec2& viewportSize)
{
#ifdef USE_IMGUI
	if (!isInitialized_ || !camera || transforms.empty()) return false;

	// 全オブジェクトの中心位置を計算
	Vector3 centerPos = { 0.0f, 0.0f, 0.0f };
	for (const auto* transform : transforms)
	{
		centerPos.x += transform->position.x;
		centerPos.y += transform->position.y;
		centerPos.z += transform->position.z;
	}
	centerPos.x /= static_cast<float>(transforms.size());
	centerPos.y /= static_cast<float>(transforms.size());
	centerPos.z /= static_cast<float>(transforms.size());

	// 中心位置でギズモを表示
	Transform centerTransform;
	centerTransform.position = centerPos;
	centerTransform.rotation = transforms[0]->rotation;
	centerTransform.scale = transforms[0]->scale;

	// ワールド行列を作成
	Matrix4x4 worldMatrix = MakeAffineMatrix(
		centerTransform.scale,
		centerTransform.rotation,
		centerTransform.position
	);

	Vector3 oldCenter = centerPos;

	// ギズモを描画
	bool manipulated = DrawGizmo(camera, centerTransform, worldMatrix, viewportPos, viewportSize);

	if (manipulated)
	{
		// 移動差分を計算して全オブジェクトに適用
		Vector3 delta = {
			centerTransform.position.x - oldCenter.x,
			centerTransform.position.y - oldCenter.y,
			centerTransform.position.z - oldCenter.z
		};

		for (auto* transform : transforms)
		{
			transform->position.x += delta.x;
			transform->position.y += delta.y;
			transform->position.z += delta.z;

			// 回転・スケールは操作モードに応じて同期
			if (settings_.operation == Operation::Rotate)
			{
				transform->rotation = centerTransform.rotation;
			} else if (settings_.operation == Operation::Scale)
			{
				transform->scale = centerTransform.scale;
			}
		}
		return true;
	}

	return false;
#else
	return false;
#endif
}

void GizmoController::DrawSettings()
{
#ifdef USE_IMGUI
	if (ImGui::CollapsingHeader("ギズモ設定"))
	{
		// 操作モード
		ImGui::Text("操作モード");
		if (ImGui::RadioButton("移動", settings_.operation == Operation::Translate))
			settings_.operation = Operation::Translate;
		ImGui::SameLine();
		if (ImGui::RadioButton("回転", settings_.operation == Operation::Rotate))
			settings_.operation = Operation::Rotate;
		ImGui::SameLine();
		if (ImGui::RadioButton("スケール", settings_.operation == Operation::Scale))
			settings_.operation = Operation::Scale;

		ImGui::Separator();

		// 座標系モード
		ImGui::Text("座標系");
		if (ImGui::RadioButton("ワールド座標", settings_.mode == Mode::World))
			settings_.mode = Mode::World;
		ImGui::SameLine();
		if (ImGui::RadioButton("ローカル座標", settings_.mode == Mode::Local))
			settings_.mode = Mode::Local;

		ImGui::Separator();

		// スナップ設定
		ImGui::Checkbox("スナップ", &settings_.useSnap);
		if (settings_.useSnap)
		{
			if (settings_.operation == Operation::Rotate)
			{
				ImGui::DragFloat("スナップ角度", &settings_.rotationSnapDegrees, 0.1f, 0.1f, 45.0f, "%.1f°");
			} else
			{
				ImGui::DragFloat3("スナップ値", &settings_.snapValues.x, 0.1f, 0.1f, 10.0f);
			}
		}

		ImGui::Separator();
		ImGui::Text("ショートカット: T(移動), R(回転), S(スケール)");
	}
#endif
}

void GizmoController::DrawNumericInput(Transform& transform)
{
#ifdef USE_IMGUI
	if (ImGui::CollapsingHeader("数値入力", ImGuiTreeNodeFlags_DefaultOpen))
	{
		bool changed = false;

		// 位置入力
		ImGui::Text("位置");
		ImGui::PushItemWidth(80.0f);
		ImGui::Text("X:"); ImGui::SameLine();
		if (ImGui::DragFloat("##PosX", &transform.position.x, 0.1f)) changed = true;
		ImGui::SameLine();
		ImGui::Text("Y:"); ImGui::SameLine();
		if (ImGui::DragFloat("##PosY", &transform.position.y, 0.1f)) changed = true;
		ImGui::SameLine();
		ImGui::Text("Z:"); ImGui::SameLine();
		if (ImGui::DragFloat("##PosZ", &transform.position.z, 0.1f)) changed = true;
		ImGui::PopItemWidth();

		ImGui::Separator();

		// 回転入力（度数法で表示）
		ImGui::Text("回転");
		Vector3 rotationDeg = {
			RadToDeg(transform.rotation.x),
			RadToDeg(transform.rotation.y),
			RadToDeg(transform.rotation.z)
		};

		ImGui::PushItemWidth(80.0f);
		ImGui::Text("X:"); ImGui::SameLine();
		if (ImGui::DragFloat("##RotX", &rotationDeg.x, 1.0f)) {
			transform.rotation.x = DegToRad(rotationDeg.x);
			changed = true;
		}
		ImGui::SameLine();
		ImGui::Text("Y:"); ImGui::SameLine();
		if (ImGui::DragFloat("##RotY", &rotationDeg.y, 1.0f)) {
			transform.rotation.y = DegToRad(rotationDeg.y);
			changed = true;
		}
		ImGui::SameLine();
		ImGui::Text("Z:"); ImGui::SameLine();
		if (ImGui::DragFloat("##RotZ", &rotationDeg.z, 1.0f)) {
			transform.rotation.z = DegToRad(rotationDeg.z);
			changed = true;
		}
		ImGui::PopItemWidth();

		ImGui::Separator();

		// スケール入力
		ImGui::Text("スケール");
		ImGui::PushItemWidth(80.0f);
		ImGui::Text("X:"); ImGui::SameLine();
		if (ImGui::DragFloat("##ScaleX", &transform.scale.x, 0.01f, 0.001f, 100.0f)) changed = true;
		ImGui::SameLine();
		ImGui::Text("Y:"); ImGui::SameLine();
		if (ImGui::DragFloat("##ScaleY", &transform.scale.y, 0.01f, 0.001f, 100.0f)) changed = true;
		ImGui::SameLine();
		ImGui::Text("Z:"); ImGui::SameLine();
		if (ImGui::DragFloat("##ScaleZ", &transform.scale.z, 0.01f, 0.001f, 100.0f)) changed = true;
		ImGui::PopItemWidth();

		// 統一スケールオプション
		ImGui::SameLine();
		static bool uniformScale = true;
		ImGui::Checkbox("統一", &uniformScale);

		if (uniformScale && changed)
		{
			// XYZの平均値で統一
			float avg = (transform.scale.x + transform.scale.y + transform.scale.z) / 3.0f;
			transform.scale = { avg, avg, avg };
		}

		ImGui::Separator();

		// リセットボタン
		if (ImGui::Button("位置リセット")) {
			transform.position = { 0.0f, 0.0f, 0.0f };
			changed = true;
		}
		ImGui::SameLine();
		if (ImGui::Button("回転リセット")) {
			transform.rotation = { 0.0f, 0.0f, 0.0f };
			changed = true;
		}
		ImGui::SameLine();
		if (ImGui::Button("スケールリセット")) {
			transform.scale = { 1.0f, 1.0f, 1.0f };
			changed = true;
		}
	}
#endif
}

void GizmoController::HandleShortcuts()
{
#ifdef USE_IMGUI
	if (ImGui::GetIO().WantCaptureKeyboard) return;

	if (ImGui::IsKeyPressed(ImGuiKey_T))
		settings_.operation = Operation::Translate;
	if (ImGui::IsKeyPressed(ImGuiKey_R))
		settings_.operation = Operation::Rotate;
	if (ImGui::IsKeyPressed(ImGuiKey_S))
		settings_.operation = Operation::Scale;
#endif
}

///************************* 履歴管理 *************************///

void GizmoController::PushHistory(const Transform& transform)
{
	// スタックサイズが上限に達したら最古の履歴を削除
	if (undoStack_.size() >= MAX_HISTORY_SIZE)
	{
		std::stack<Transform> temp;
		while (undoStack_.size() > 1)
		{
			temp.push(undoStack_.top());
			undoStack_.pop();
		}
		undoStack_.pop();
		while (!temp.empty())
		{
			undoStack_.push(temp.top());
			temp.pop();
		}
	}

	undoStack_.push(transform);
	// 新しい操作が行われたのでRedoスタックをクリア
	while (!redoStack_.empty()) redoStack_.pop();
}

bool GizmoController::Undo(Transform& transform)
{
	if (undoStack_.empty()) return false;

	// 現在の状態をRedoスタックに保存
	redoStack_.push(transform);

	// Undoスタックから復元
	transform = undoStack_.top();
	undoStack_.pop();

	return true;
}

bool GizmoController::Redo(Transform& transform)
{
	if (redoStack_.empty()) return false;

	// 現在の状態をUndoスタックに保存
	undoStack_.push(transform);

	// Redoスタックから復元
	transform = redoStack_.top();
	redoStack_.pop();

	return true;
}

void GizmoController::ClearHistory()
{
	while (!undoStack_.empty()) undoStack_.pop();
	while (!redoStack_.empty()) redoStack_.pop();
	while (!undoStackMulti_.empty()) undoStackMulti_.pop();
	while (!redoStackMulti_.empty()) redoStackMulti_.pop();
}

void GizmoController::PushHistoryMulti(const std::vector<Transform>& transforms)
{
	// スタックサイズが上限に達したら最古の履歴を削除
	if (undoStackMulti_.size() >= MAX_HISTORY_SIZE)
	{
		std::stack<std::vector<Transform>> temp;
		while (undoStackMulti_.size() > 1)
		{
			temp.push(undoStackMulti_.top());
			undoStackMulti_.pop();
		}
		undoStackMulti_.pop();
		while (!temp.empty())
		{
			undoStackMulti_.push(temp.top());
			temp.pop();
		}
	}

	undoStackMulti_.push(transforms);
	while (!redoStackMulti_.empty()) redoStackMulti_.pop();
}

bool GizmoController::UndoMulti(std::vector<Transform*>& transforms)
{
	if (undoStackMulti_.empty() || transforms.size() != undoStackMulti_.top().size())
		return false;

	// 現在の状態をRedoスタックに保存
	std::vector<Transform> currentState;
	for (const auto* t : transforms)
	{
		currentState.push_back(*t);
	}
	redoStackMulti_.push(currentState);

	// 保存された状態を全オブジェクトに復元
	const auto& savedState = undoStackMulti_.top();
	for (size_t i = 0; i < transforms.size(); ++i)
	{
		*transforms[i] = savedState[i];
	}
	undoStackMulti_.pop();

	return true;
}

bool GizmoController::RedoMulti(std::vector<Transform*>& transforms)
{
	if (redoStackMulti_.empty() || transforms.size() != redoStackMulti_.top().size())
		return false;

	// 現在の状態をUndoスタックに保存
	std::vector<Transform> currentState;
	for (const auto* t : transforms)
	{
		currentState.push_back(*t);
	}
	undoStackMulti_.push(currentState);

	// 保存された状態を全オブジェクトに復元
	const auto& savedState = redoStackMulti_.top();
	for (size_t i = 0; i < transforms.size(); ++i)
	{
		*transforms[i] = savedState[i];
	}
	redoStackMulti_.pop();

	return true;
}

///************************* 状態取得 *************************///

bool GizmoController::IsUsing() const
{
#ifdef USE_IMGUI
	return ImGuizmo::IsUsing();
#else
	return false;
#endif
}

bool GizmoController::IsOver() const
{
#ifdef USE_IMGUI
	return ImGuizmo::IsOver();
#else
	return false;
#endif
}

///************************* 内部処理 *************************///

void GizmoController::MatrixToImGuizmo(const Matrix4x4& matrix, float* imguizmoMatrix) const
{
	if (!imguizmoMatrix) return;
	// 4x4行列を1次元配列に変換
	for (int i = 0; i < 4; ++i)
	{
		for (int j = 0; j < 4; ++j)
		{
			imguizmoMatrix[i * 4 + j] = matrix.m[i][j];
		}
	}
}

Matrix4x4 GizmoController::ImGuizmoToMatrix(const float* imguizmoMatrix) const
{
	Matrix4x4 m{};
	if (!imguizmoMatrix) return m;
	// 1次元配列を4x4行列に変換
	for (int r = 0; r < 4; ++r)
	{
		for (int c = 0; c < 4; ++c)
		{
			m.m[r][c] = imguizmoMatrix[r * 4 + c];
		}
	}
	return m;
}

void GizmoController::DecomposeMatrix(const Matrix4x4& matrix, Transform& transform) const
{
#ifdef USE_IMGUI
	const float* m = reinterpret_cast<const float*>(&matrix.m[0][0]);

	// ImGuizmoで行列を位置・回転・スケールに分解
	float t[3]{}, rDeg[3]{}, s[3]{};
	ImGuizmo::DecomposeMatrixToComponents(const_cast<float*>(m), t, rDeg, s);

	// トランスフォームに格納（回転は度数法→ラジアン変換）
	transform.position = { t[0], t[1], t[2] };
	transform.rotation = { DegToRad(rDeg[0]), DegToRad(rDeg[1]), DegToRad(rDeg[2]) };
	transform.scale = { s[0], s[1], s[2] };
#endif
}
#endif // _DEBUG
