#ifndef MWINPUT_GYROMANAGER
#define MWINPUT_GYROMANAGER

#include <array>

namespace MWInput
{
    class GyroManager
    {
    public:
        void update(float dt, std::array<float, 3> values) const;

        void setGuiCursorEnabled(bool enabled) { mGuiCursorEnabled = enabled; }

    private:
        friend class InputManager;
        bool mGuiCursorEnabled = true;
    };
}

#endif // !MWINPUT_GYROMANAGER
