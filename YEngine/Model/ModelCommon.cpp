#include "ModelCommon.h"

ModelCommon* ModelCommon::GetInstance() {
    static ModelCommon instance;
    return &instance;
}

void ModelCommon::Initialize(DirectXCommon* dxCommon) {
    dxCommon_ = dxCommon;
}
