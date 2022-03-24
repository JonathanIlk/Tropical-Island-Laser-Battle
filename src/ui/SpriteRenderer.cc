#include "SpriteRenderer.hh"
#include <ECS/Join.hh>
#include <Game.hh>
#include <MathUtil.hh>
#include <animation/AnimatorManager.hh>
#include <glow/objects.hh>
#include <typed-geometry/tg-std.hh>
#include <utility>

using namespace SpriteRenderer;

System::System(Game& game) : mGame(game), mECS(game.mECS)
{
    mShader = mGame.mSharedResources.sprite;
    mQuadVao = mGame.mSharedResources.spriteQuad;
    mTexBlack = glow::Texture2D::createFromFile("../data/textures/black.png", glow::ColorSpace::sRGB);
}

void System::resize(int w, int h) {
    mWindowSize = {w, h};
}

void System::render() {
    animateFade();

    auto shader = mShader->use();
    auto vao = mQuadVao->bind();
    auto sizeFactor = tg::vec2(2.f / mWindowSize.width, -2.f / mWindowSize.height);

    for (const auto& pair : mECS.sprites) {
        auto &[id, instance] = pair;

        if (instance.hidden) {continue;}

        auto transform = tg::mat2::diag(sizeFactor) * instance.transform;
        transform[2] += tg::vec2(-1, 1);
        shader["uTransform"] = transform;
        shader["uImage"] = instance.image;
        shader["uAlpha"] = instance.alpha;

        vao.draw();
    }
}
ECS::entity System::addSprite(tg::pos2 position, tg::size2 size, glow::SharedTexture2D image, float alpha, tg::vec2 anchor) {
    auto spriteEnt = mECS.newEntity();

    tg::mat3x2 transform;
    transform[0][0] = size.width;
    transform[1][1] = -size.height;
    position -= anchor * size;
    transform[2] = tg::vec2(position);

    mECS.sprites.emplace(spriteEnt, Instance{std::move(image), transform, alpha});

    return spriteEnt;
}

void System::removeSprite(ECS::entity ent) {
    mECS.sprites.erase(ent);
}

void System::hideSprite(ECS::entity ent) {
    mECS.sprites[ent].hidden = true;
}

void System::showSprite(ECS::entity ent) {
    mECS.sprites[ent].hidden = false;
}

void System::setSpriteAlpha(ECS::entity ent, float alpha) {
    mECS.sprites[ent].alpha = alpha;
}

void System::animateFade() {
    if (fadeInOutAnimator && !fadeInOutAnimator->finished)
    {
        setSpriteAlpha(fadeInOutSprite, fadeInOutAnimator->currentState().mValue.value);
    }
}
void System::startFadeAnimation(std::shared_ptr<Animator<FloatKeyFrame>> animator) {
    fadeInOutSprite = addSprite(tg::pos2(0, 0), tg::size2(mGame.getWindowWidth(), mGame.getWindowHeight()), mTexBlack, 0.0f, tg::vec2(0.0f, 0.0f));
    fadeInOutAnimator = std::move(animator);
    AnimatorManager::start(fadeInOutAnimator);
}
