#include <ECS.hh>
#include <glow/fwd.hh>
#include <animation/Animator.hh>

namespace SpriteRenderer {

    struct Instance {
        glow::SharedTexture2D image;
        tg::mat3x2 transform;
        float alpha;
        bool hidden = false;
    };

    class System
    {
        Game &mGame;
        ECS::ECS &mECS;

        glow::SharedVertexArray mQuadVao;
        glow::SharedProgram mShader;

        ECS::entity fadeInOutSprite;
        glow::SharedTexture2D mTexBlack;
        std::shared_ptr<Animator<FloatKeyFrame>> fadeInOutAnimator;

        tg::isize2 mWindowSize;

        void animateFade();
    public:
        /**
         *
         * @param position
         * @param size
         * @param image
         * @param alpha
         * @param anchor Set the anchor of the sprite, from 0-1.
         * @return
         */
        ECS::entity addSprite(tg::pos2 position, tg::size2 size, glow::SharedTexture2D image, float alpha = 1.0f, tg::vec2 anchor = tg::vec2(0.5, 0.5));
        void removeSprite(ECS::entity ent);
        void hideSprite(ECS::entity ent);
        void showSprite(ECS::entity ent);
        void setSpriteAlpha(ECS::entity ent, float alpha);

        void resize(int w, int h);
        System(Game&);
        void render();
        void startFadeAnimation(std::shared_ptr<Animator<FloatKeyFrame>>);
    };
}


