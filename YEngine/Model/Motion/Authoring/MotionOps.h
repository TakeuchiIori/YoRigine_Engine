#pragma once

#include "../Core/Motion.h"

#include <map>
#include <optional>
#include <string>

// ============================================================
// モーション編集の純データ操作
//
// ・ImGui / Model / ObjectManager / MotionEditorContext に依存しない
// ・入力の Motion は変更せず、結果を新しい値として返す（副作用なし）
// ・キャッシュ登録や MotionSystem への反映は呼び出し側（エディタ）の責務
// ============================================================
namespace MotionOps {

using NodeAnimationMap = std::map<std::string, Motion::NodeAnimation>;

// ------------------------------------------------------------
// 左右反転（ミラー）したモーションを生成する
//   ・Translate.x を反転、Rotate の y/z を反転
//   ・Left/Right、.L/.R、_L_/_R_ を含むボーン名はトラックを入れ替える
// ------------------------------------------------------------
Motion CreateMirrored(const Motion &source);

// ------------------------------------------------------------
// [startTime, endTime] を切り出した新しいモーションを生成する
//   ・開始/終了地点は補間値でキーを補い、時間は startTime 起点にずらす
//   ・endTime <= startTime の場合は std::nullopt
// ------------------------------------------------------------
std::optional<Motion> CreateTrimmed(const Motion &source, float startTime,
                                    float endTime);

// ------------------------------------------------------------
// 正再生 + 逆再生を結合した、長さ2倍のモーションを生成する
// ------------------------------------------------------------
Motion CreatePingPong(const Motion &source);

// ------------------------------------------------------------
// 区間移動の焼き込み
// ------------------------------------------------------------
struct TranslateOffsetBakeParams {
  std::string boneName;         // 対象ボーン
  QuaternionTransform fallback; // 対象ボーンにキーが無い場合の初期姿勢
  int startFrame = 0;           // 区間の開始フレーム（範囲外は丸める）
  int endFrame = 0; // 区間の終了フレーム（startFrame より小さければ入れ替える）
  int fps = 60;     // フレーム → 秒の換算レート
  Vector3 offset = {0, 0, 0}; // 区間内に加算する移動量（スケール適用済み）
};

struct TranslateOffsetBakeResult {
  NodeAnimationMap animations; // 焼き込み後の全ノードアニメーション
  int startFrame = 0;          // 実際に使われた開始フレーム（丸め後）
  int endFrame = 0;            // 実際に使われた終了フレーム（丸め後）
};

// 開始直前/開始/区間内キー/終了/終了直後に Translate
// キーを作り、区間だけ移動量を足す。 offset
// が実質ゼロの場合は何も変わらないため std::nullopt を返す。
std::optional<TranslateOffsetBakeResult>
BakeTranslateOffsetRange(const Motion &motion,
                         const TranslateOffsetBakeParams &params);

// ------------------------------------------------------------
// 時間を反転したモーションを生成する（逆再生版として保存する用途）
// ------------------------------------------------------------
Motion CreateTimeReversed(const Motion &source);

// ------------------------------------------------------------
// タイムスケールカーブをキー時間へ焼き込んだモーションを生成する
//   ・速度の逆数を積分して各キーの時刻を再マップし、長さも更新する
//   ・結果のモーションは SpeedCurve を持たない（等速）
//   ・カーブが無い / 長さ0 の場合は入力をそのまま返す
// ------------------------------------------------------------
Motion BakeSpeedCurve(const Motion &source);

// ============================================================
// キー単位の編集（NodeAnimationMap を直接書き換える）
// ============================================================

// 編集対象のチャンネル
enum class Channel { Translate, Rotate, Scale };

// チャンネルの組み合わせ（ビットフラグ）
enum ChannelMask : unsigned {
  kChannelTranslate = 1u << 0,
  kChannelRotate = 1u << 1,
  kChannelScale = 1u << 2,
  kChannelAll = kChannelTranslate | kChannelRotate | kChannelScale,
};

inline unsigned ToMask(Channel channel) {
  return 1u << static_cast<unsigned>(channel);
}

// time に T/R/S のキーを打つ（同時刻のキーがあれば上書き）。
// トラックが新規なら補間は Linear にする。
void InsertPoseKey(NodeAnimationMap &animations, const std::string &bone,
                   float time, const QuaternionTransform &pose);

// 削除の結果
enum class RemoveResult {
  Removed,           // 1つ以上削除した
  NotFound,          // 指定時刻にキーが無かった
  WouldEmptyChannel, // チャンネルが空になる（サンプリング不能）ため中止
};

// bone の time にあるキーを mask のチャンネルから削除する。
// どれか1つでもチャンネルが空になる場合は何も変更しない。
RemoveResult RemoveKeysAt(NodeAnimationMap &animations, const std::string &bone,
                          float time, unsigned mask = kChannelAll);

// 全ボーン・全チャンネルで、fromFrame にあるキーを delta フレームずらす
void ShiftKeysAtFrame(NodeAnimationMap &animations, int fromFrame, int delta,
                      int fps);

// bone の channel のキー時刻を newTimes で置き換える（値は保持）。
// キー数が一致しない場合は false を返して何もしない。
// 時刻は昇順に並べ直したうえで、元のキーと昇順同士で対応付ける。
bool RetimeChannel(NodeAnimationMap &animations, const std::string &bone,
                   Channel channel, std::vector<float> newTimes);

// time を fps のフレーム番号に丸める（タイムライン表示用）
inline int TimeToFrame(float time, int fps) {
  return static_cast<int>(time * static_cast<float>(fps) + 0.5f);
}

} // namespace MotionOps
