#include "GPUParticle.hlsli"

RWStructuredBuffer<Particle> g_Particles : register(u0);
ConstantBuffer<PerFrame> g_PerFrame : register(b0);

RWStructuredBuffer<int> g_FreeListIndex : register(u1);
RWStructuredBuffer<uint> g_FreeList : register(u2);
RWStructuredBuffer<uint> g_ActiveCount : register(u3);

[numthreads(1024, 1, 1)] // スレッド数は元のまま維持
void main(uint3 DTid : SV_DispatchThreadID)
{
    // 各スレッドが複数のパーティクルを担当
    uint baseIndex = DTid.x * kParticlesPerThread;
    
    for (uint i = 0; i < kParticlesPerThread; i++)
    {
        uint particleIndex = baseIndex + i;
        
        // 範囲チェック（新しい最大パーティクル数に対応）
        if (particleIndex >= kMaxParticles)
            break;
        
        // 生きてないパーティクルは何もしない
        if (g_Particles[particleIndex].isActive == 0)
        {
            continue;
        }
        
        /*==============================================================================================//

                                              生存時の処理

        //==============================================================================================*/
        if (g_Particles[particleIndex].currentTime < g_Particles[particleIndex].lifeTime)
        {
            // 位置更新（速度を適用）
            g_Particles[particleIndex].translate += g_Particles[particleIndex].velocity * g_PerFrame.deltaTime;
            
            // 時間更新
            g_Particles[particleIndex].currentTime += g_PerFrame.deltaTime;
            
            // アルファ値計算（ライフタイムに基づいてフェードアウト）
            float alpha = 1.0f - (g_Particles[particleIndex].currentTime / g_Particles[particleIndex].lifeTime);
            g_Particles[particleIndex].color.a = saturate(alpha);
            
            // スケールアニメーション（オプション）
            float lifeRatio = g_Particles[particleIndex].currentTime / g_Particles[particleIndex].lifeTime;
            float scaleMultiplier = 1.0f - lifeRatio * 0.5f; // 時間と共にスケール縮小
            g_Particles[particleIndex].scale *= scaleMultiplier;
        }
        else
        {
        /*==============================================================================================//

                                              死んだ時の処理

        //==============================================================================================*/
            // パーティクルが死んだ場合の処理
            g_Particles[particleIndex].scale = float3(0.0f, 0.0f, 0.0f);
            g_Particles[particleIndex].isActive = 0;
            
            // FreeListに返却（修正版）
            int currentFreeListIndex;
            InterlockedAdd(g_FreeListIndex[0], 1, currentFreeListIndex);
            
            // currentFreeListIndexは変更前の値
            // 変更後の値は currentFreeListIndex + 1
            int newIndex = currentFreeListIndex + 1;
            
            // 範囲チェック：0以上、kMaxParticles未満
            if (newIndex >= 0 && newIndex < (int) kMaxParticles)
            {
                g_FreeList[newIndex] = particleIndex;
            }
            else
            {
                // オーバーフローした場合は元に戻す
                InterlockedAdd(g_FreeListIndex[0], -1);
            }
            
            // アクティブカウントをデクリメント
            InterlockedAdd(g_ActiveCount[0], (uint) -1);
        }
    }
}
