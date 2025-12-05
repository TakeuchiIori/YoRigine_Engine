#pragma once

// C++
#include <vector>
#include <stack>
#include <string>

// Math
#include "Vector3.h"
#include "Matrix4x4.h"

// ImGui
#ifdef USE_IMGUI
#include "imgui.h"
#include "ImGuizmo.h"


// 前方宣言
class Camera;

// ギズモコントローラークラス
class GizmoController
{
public:
	///************************* 列挙型 *************************///

	// 操作モード
	enum class Operation
	{
		Translate,  // 移動
		Rotate,     // 回転
		Scale       // スケール
	};

	// 座標系モード
	enum class Mode
	{
		World,  // ワールド座標
		Local   // ローカル座標
	};

	///************************* 構造体 *************************///

	// トランスフォーム情報
	struct Transform
	{
		Vector3 position = { 0.0f, 0.0f, 0.0f };
		Vector3 rotation = { 0.0f, 0.0f, 0.0f }; // ラジアン
		Vector3 scale = { 1.0f, 1.0f, 1.0f };

		// 比較演算子
		bool operator==(const Transform& other) const {
			return position.x == other.position.x && position.y == other.position.y && position.z == other.position.z &&
				rotation.x == other.rotation.x && rotation.y == other.rotation.y && rotation.z == other.rotation.z &&
				scale.x == other.scale.x && scale.y == other.scale.y && scale.z == other.scale.z;
		}
		bool operator!=(const Transform& other) const {
			return !(*this == other);
		}
	};

	// ギズモ設定
	struct Settings
	{
		Operation operation = Operation::Translate;
		Mode mode = Mode::World;
		bool useSnap = false;
		Vector3 snapValues = { 1.0f, 1.0f, 1.0f };
		float rotationSnapDegrees = 15.0f;
	};

public:
	///************************* 基本関数 *************************///

	GizmoController() = default;
	~GizmoController() = default;

	// 初期化
	void Initialize();

	// ギズモ描画
	bool DrawGizmo(
		Camera* camera,
		Transform& transform,
		Matrix4x4& worldMatrix,
		const ImVec2& viewportPos,
		const ImVec2& viewportSize
	);

	// 設定UI描画
	void DrawSettings();

	// ショートカットキー処理
	void HandleShortcuts();

	// ギズモ描画（複数オブジェクト対応）
	bool DrawGizmoMulti(
		Camera* camera,
		std::vector<Transform*>& transforms,
		const ImVec2& viewportPos,
		const ImVec2& viewportSize
	);

	// 数値入力UI描画
	void DrawNumericInput(Transform& transform);

	// 履歴管理
	void PushHistory(const Transform& transform);
	bool Undo(Transform& transform);
	bool Redo(Transform& transform);
	void ClearHistory();

	// 履歴管理（複数オブジェクト用）
	void PushHistoryMulti(const std::vector<Transform>& transforms);
	bool UndoMulti(std::vector<Transform*>& transforms);
	bool RedoMulti(std::vector<Transform*>& transforms);

public:
	///************************* アクセッサ *************************///

	// 操作モード設定
	void SetOperation(Operation op) { settings_.operation = op; }
	// 座標系モード設定
	void SetMode(Mode mode) { settings_.mode = mode; }
	// スナップ使用設定
	void SetUseSnap(bool use) { settings_.useSnap = use; }
	// スナップ値設定
	void SetSnapValues(const Vector3& snap) { settings_.snapValues = snap; }
	// 回転スナップ設定
	void SetRotationSnap(float degrees) { settings_.rotationSnapDegrees = degrees; }

	// 操作モード取得
	Operation GetOperation() const { return settings_.operation; }
	// 座標系モード取得
	Mode GetMode() const { return settings_.mode; }
	// スナップ使用状態取得
	bool IsUsingSnap() const { return settings_.useSnap; }
	// スナップ値取得
	Vector3 GetSnapValues() const { return settings_.snapValues; }
	// 回転スナップ取得
	float GetRotationSnap() const { return settings_.rotationSnapDegrees; }

	// ギズモ操作中判定
	bool IsUsing() const;
	// ギズモホバー判定
	bool IsOver() const;

	// 履歴情報取得
	bool CanUndo() const { return !undoStack_.empty(); }
	bool CanRedo() const { return !redoStack_.empty(); }
	size_t GetUndoStackSize() const { return undoStack_.size(); }
	size_t GetRedoStackSize() const { return redoStack_.size(); }

	// 履歴情報取得（複数オブジェクト用）
	bool CanUndoMulti() const { return !undoStackMulti_.empty(); }
	bool CanRedoMulti() const { return !redoStackMulti_.empty(); }

private:
	///************************* 内部処理 *************************///

	// Matrix4x4をImGuizmo形式に変換
	void MatrixToImGuizmo(const Matrix4x4& matrix, float* imguizmoMatrix) const;
	// ImGuizmo形式をMatrix4x4に変換
	Matrix4x4 ImGuizmoToMatrix(const float* imguizmoMatrix) const;
	// 行列からトランスフォーム抽出
	void DecomposeMatrix(const Matrix4x4& matrix, Transform& transform) const;
	// 度数法→ラジアン変換
	float DegToRad(float deg) const { return deg * (3.14159265359f / 180.0f); }
	// ラジアン→度数法変換
	float RadToDeg(float rad) const { return rad * (180.0f / 3.14159265359f); }

private:
	///************************* メンバ変数 *************************///

	Settings settings_;
	bool isInitialized_ = false;

	// 履歴管理（単一オブジェクト用）
	std::stack<Transform> undoStack_;
	std::stack<Transform> redoStack_;
	static const size_t MAX_HISTORY_SIZE = 100;

	// 履歴管理（複数オブジェクト用）
	std::stack<std::vector<Transform>> undoStackMulti_;
	std::stack<std::vector<Transform>> redoStackMulti_;
};
#endif