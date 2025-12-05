#include "GPUParticle.hlsli"
#include "../Method/Random.hlsli"

RWStructuredBuffer<Particle> g_Particles : register(u0);
RWStructuredBuffer<int> g_FreeListIndex : register(u1);
RWStructuredBuffer<uint> g_FreeList : register(u2);
RWStructuredBuffer<uint> g_ActiveCount : register(u3);


[numthreads(1024, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    // 最初のスレッドだけがFreeListIndexを初期化
    if (DTid.x == 0)
    {
        g_FreeListIndex[0] = kMaxParticles - 1;
        g_ActiveCount[0] = 0;
    }
    
    // 全スレッドの同期を取る
    GroupMemoryBarrierWithGroupSync();
    
    uint baseIndex = DTid.x * kParticlesPerThread;
    
    // 各スレッドが担当するパーティクルを初期化
    for (uint i = 0; i < kParticlesPerThread; i++)
    {
        uint particleindex = baseIndex + i;
        
        // 範囲チェック
        if (particleindex < kMaxParticles)
        {
            // パーティクルを完全にクリア
            g_Particles[particleindex].translate = float3(0.0f, 0.0f, 0.0f);
            g_Particles[particleindex].scale = float3(1.0f, 1.0f, 1.0f);
            g_Particles[particleindex].rotate = 0.0f;
            g_Particles[particleindex].lifeTime = 0.0f;
            g_Particles[particleindex].currentTime = 0.0f;
            g_Particles[particleindex].velocity = float3(0.0f, 0.0f, 0.0f);
            g_Particles[particleindex].color = float4(1.0f, 1.0f, 1.0f, 0.0f); // ★alpha=0で非表示
            g_Particles[particleindex].isBillboard = 1;
            g_Particles[particleindex].isActive = 0; // ★非アクティブ状態
            
            // FreeListに追加（利用可能なパーティクルとしてマーク）
            g_FreeList[particleindex] = particleindex;
        }
    }
}