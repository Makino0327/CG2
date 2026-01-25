#pragma once

// シーン基底クラス
class BaseScene
{
public: // メンバ関数
    virtual ~BaseScene() = default;

    virtual void Initialize() = 0;
    virtual void Finalize() = 0;
    virtual void Update() = 0;
    virtual void Draw() = 0;
};
