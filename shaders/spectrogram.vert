#version 440
layout(location = 0) in vec4 qt_Vertex;
layout(location = 1) in vec2 qt_MultiTexCoord0;

layout(location = 0) out vec2 v_TexCoord;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    vec4 u_viewParams;  // (w, h, scrollLeft, msPerPixel)
    vec4 u_audioParams; // (sampleRate, hopSamples, totalFrames, nbrBins)
    vec4 u_curveParams; // (maxFreq, freqRef, posFref, amplitudeScale)
    vec4 u_selParams;   // (selStartMs, selEndMs, qt_Opacity, 0.0)
};

void main() {
    gl_Position = qt_Matrix * qt_Vertex;
    v_TexCoord = qt_MultiTexCoord0;
}
