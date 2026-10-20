#pragma once
#include "../engine/2d/sprite/Sprite.h"
#include "../engine/base/winapp/WinApp.h"
#include <array>
#include <cmath>
#include <memory>

// フェード中とシャッター開放中に、同じ緑の演出をそのまま使う。
class TransitionScanEffect {
public:
    void Initialize(SpriteCommon* common, ID3D12Resource* light) {
        for (auto& line : lines_) {
            line = std::make_unique<Sprite>();
            line->Initialize(common, light, "Resources/white2x2.png");
        }
        Update(0.0f, 0.0f);
    }
    void Update(float frame, float opacity) {
        for (size_t i = 0; i < lines_.size(); ++i) {
            if (!lines_[i]) { continue; }
            const float index = static_cast<float>(i);
            const float offset = index*WinApp::kClientHeight/static_cast<float>(lines_.size());
            const float jitter = std::sin(frame*0.83f+index*6.1f)*4.0f;
            const float y = std::fmod(frame*31.0f+offset+jitter,
                static_cast<float>(WinApp::kClientHeight)+40.0f)-20.0f;
            const float flicker = 0.11f+0.05f*(0.5f+0.5f*std::sin(frame*2.8f+index));
            lines_[i]->SetPosition({ 0, y });
            lines_[i]->SetSize({ static_cast<float>(WinApp::kClientWidth), i%2 == 0 ? 2.0f : 1.0f });
            lines_[i]->SetColor({ 0.2f, 0.85f, 0.4f, opacity*flicker });
            lines_[i]->Update();
        }
    }
    void Draw() {
        for (auto& line : lines_) { if (line) { line->Draw(); } }
    }
    void Reset() { for (auto& line : lines_) { line.reset(); } }
private:
    std::array<std::unique_ptr<Sprite>, 4> lines_;
};
