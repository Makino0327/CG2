#pragma once

class BaseScene; // ★前方宣言（ここではincludeしない）

class Framework {
public:
    virtual ~Framework() = default;

    virtual void Initialize() {}
    virtual void Update() {}
    virtual void Finalize() {}

    void Run();
    virtual void Draw() = 0;

    virtual bool IsEndRequest() const { return endRequest_; }

    // ★シーンをセットする（SceneManagerが後でやるならここ経由が楽）
    void SetScene(BaseScene* scene) { scene_ = scene; }

protected:
    bool endRequest_ = false;

    // ★今のシーン（所有権はここでは持たない前提）
    BaseScene* scene_ = nullptr;
};
