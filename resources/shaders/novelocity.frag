#version 450
// The depth prepass's colour for a surface that moves its own vertices (the sea's sheet following the eye): no move of
// its own known - the temporal resolve goes by its depth
layout(location = 0) out vec2 outVelocity;
void main() { outVelocity = vec2(10000.0); }
