#pragma once

class Framework {
public:
    virtual ~Framework() = default;

    virtual void Initialize() {}
    virtual void Update() {}
    virtual void Finalize() {}

    void Run();
    // 純粋仮想関数（必須）
    virtual void Draw() = 0;

    // 終了チェック（const にしておくと Game 側と一致しやすい）
    virtual bool IsEndRequest() const { return endRequest_; }

protected:
    bool endRequest_ = false;
};
