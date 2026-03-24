#pragma once

struct SequenceStep {
    enum class Type { SetAxis, Wait };
    Type type    = Type::SetAxis;
    int axisIndex = -1;  // SetAxis
    int value     =  0;  // SetAxis (0-1000)
    int timeMs    =  0;  // Wait
};
