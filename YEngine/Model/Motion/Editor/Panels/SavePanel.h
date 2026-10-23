#pragma once
#include "IMotionEditorPanel.h"

#include <string>
#include <vector>

#ifdef USE_IMGUI
#include <FileOperations/FileBrowser.h>
#endif

// .anim の保存/読み込みと、glTF からのアニメーション読み込みのダイアログ
class SavePanel : public IMotionEditorPanel {
public:
  void DrawImGui() override;

protected:
  void OnInitialize() override;

private:
  void DrawFileDialog();
  void DrawSourceDialog();
  void DrawAnimBrowserWindow();
  void DrawSourceBrowserWindow();

  // glTF を選んで、含まれるアニメーション名の候補を取得する
  void SelectSourceModel(const std::string &path);

  // .anim
  std::string animPath_ = "Resources/Binary/edited_motion.anim";
  bool saveTimeReversed_ = false;

  // glTF
  std::string sourceModelPath_;
  std::string sourceObjectPath_;
  std::string sourceAnimationName_;
  std::vector<std::string> sourceAnimationNames_;
  std::string sourceMessage_;

#ifdef USE_IMGUI
  YoRigine::FileBrowser animBrowser_;
  YoRigine::FileBrowser sourceBrowser_;
  bool animBrowserOpen_ = false;
  bool sourceBrowserOpen_ = false;
#endif
};
