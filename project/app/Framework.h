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

protected:
    // 派生クラスは終了を「要求」するだけにし、フラグを直接書き換えさせない
    void RequestEnd() { endRequest_ = true; }

private:
    bool endRequest_ = false;
};
