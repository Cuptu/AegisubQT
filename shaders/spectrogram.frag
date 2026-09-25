#version 440
layout(location = 0) in vec2 v_TexCoord;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    vec4 u_viewParams;   // (w_logical, h_logical, scrollLeft, msPerPixel)
    vec4 u_audioParams;  // (sampleRate, hopSamples, totalFrames, nbrBins)
    vec4 u_curveParams;  // (maxFreq, freqRef, posFref, amplitudeScale)
    vec4 u_selParams;    // (selStartMs, selEndMs, qt_Opacity, 0.0)
    vec4 u_windowParams; // (winStartFrame, winFrameCount, winFrameStep, viewportDeviceHeight)
};

layout(binding = 1) uniform sampler2D u_stftTexture;
layout(binding = 2) uniform sampler2D u_paletteTexture;

// The color palette texture contains 4 rows corresponding to Aegisub's 4 Icy Blue spectrum styles:
//   row 0 = Normal, row 1 = Inactive, row 2 = Selection, row 3 = Primary.
// Note: File-level mutable variables are intentionally omitted because SPIRV-Cross fails
// to rewrite them into HLSL under the Direct3D 11 RHI backend.
const float kPaletteRows = 4.0;
const float kRowNormal = 0.125;   // (0 + 0.5) / 4
const float kRowPrimary = 0.875;  // (3 + 0.5) / 4

/// Hybrid linear/logarithmic frequency mapping from Aegisub audio_renderer_spectrum.cpp:281-308.
/// posRel = 0 maps to bottom (minband), posRel = 1 maps to top (maxband).
float binAt(float posRel, float minband, float maxband, float scaleLog, float logRatio) {
    float bLin = minband + posRel * (maxband - minband);
    float bLog = minband * exp(posRel * scaleLog);
    return bLin + logRatio * (bLog - bLin);
}

/// Samples a single frequency bin. With nearest filtering, (b + 0.5) hits bin b texel center.
float powerAt(float u, float bin, float nbrBins) {
    return texture(u_stftTexture, vec2(u, clamp((bin + 0.5) / nbrBins, 0.0, 1.0))).r;
}

void main() {
    float vpW = u_viewParams.x;
    float scrollLeft = u_viewParams.z;
    float msPerPx = u_viewParams.w;

    float sampleRate = u_audioParams.x;
    float hopSamples = u_audioParams.y;
    float totalFrames = u_audioParams.z;
    float nbrBins = u_audioParams.w;

    float maxFreq = u_curveParams.x;
    float freqRef = u_curveParams.y;
    float posFref = u_curveParams.z;
    float amplitudeScale = u_curveParams.w;

    float selStartMs = u_selParams.x;
    float selEndMs = u_selParams.y;

    float winStart = u_windowParams.x;
    float winCount = max(1.0, u_windowParams.y);
    float winStep = max(1.0, u_windowParams.z);
    float devH = max(1.0, u_windowParams.w);

    // Following Aegisub audio_renderer_spectrum.cpp:314: each column samples at the left
    // edge time of the logical pixel (ax * pixel_ms). High-DPI physical pixels share this time.
    float ax = floor(v_TexCoord.x * vpW);
    float curMs = (scrollLeft + ax) * msPerPx;
    bool isSel = (curMs >= selStartMs && curMs <= selEndMs);
    float styleRow = isSel ? kRowPrimary : kRowNormal;

    // Background color: corresponds to zero power in the active palette row (Aegisub RenderBlank).
    vec4 blank = vec4(texture(u_paletteTexture, vec2(0.0, styleRow)).rgb, 1.0);

    if (curMs < 0.0) { fragColor = blank; return; }

    // Aegisub: size_t block_index = (ax * pixel_ms * sr / 1000) >> derivation_dist;
    float frameIdx = (curMs * sampleRate / 1000.0) / hopSamples;
    if (frameIdx >= totalFrames) { fragColor = blank; return; }

    float relFrame = (frameIdx - winStart) / winStep;
    if (relFrame < 0.0 || relFrame >= winCount) { fragColor = blank; return; }

    // Matches native Aegisub: exactly one STFT frame per screen column, no horizontal interpolation.
    float u = clamp((floor(relFrame) + 0.5) / winCount, 0.0, 1.0);

    // Hybrid linear/logarithmic frequency mapping (audio_renderer_spectrum.cpp:281-308).
    float minband = 1.0;
    float maxband = min(round(nbrBins * maxFreq / (sampleRate * 0.5)), nbrBins);
    if (minband >= maxband) { fragColor = blank; return; }

    float scaleLog = log(maxband / minband);
    float bFref = clamp(nbrBins * freqRef / (sampleRate * 0.5), 1.0, maxband - 1.0);
    float clin = minband + (maxband - minband) * posFref;
    float clog = minband * exp(posFref * scaleLog);
    float denom = clog - clin;
    float logRatio = abs(denom) > 1e-4 ? clamp((bFref - clin) / denom, 0.0, 1.0) : 0.0;

    // Row to frequency bin interval: invert Y from bottom up to match Aegisub coordinate frame.
    // Row y samples at binAt(y/H), spanning midpoints [mid(prv,cur), mid(cur,nxt)).
    float rowIdx = floor(v_TexCoord.y * devH);              // 0 = top-most scanline
    float yFromBottom = (devH - 1.0) - rowIdx;
    float bCur = binAt(clamp(yFromBottom / devH, 0.0, 1.0), minband, maxband, scaleLog, logRatio);
    float bPrv = binAt(clamp((yFromBottom - 1.0) / devH, 0.0, 1.0), minband, maxband, scaleLog, logRatio);
    float bNxt = (rowIdx < 0.5)
               ? maxband
               : binAt(clamp((yFromBottom + 1.0) / devH, 0.0, 1.0), minband, maxband, scaleLog, logRatio);

    float power;
    if (bNxt - bPrv < 2.0) {
        // Single bin spans the row: linear interpolation between adjacent bins.
        float b0 = floor(bCur);
        float b1 = min(b0 + 1.0, nbrBins - 1.0);
        float frac = bCur - b0;
        power = mix(powerAt(u, b0, nbrBins), powerAt(u, b1, nbrBins), frac);
    } else {
        // Multiple bins span the row: max-pool across the bin interval to prevent high-frequency dimming.
        float bInf = clamp(floor((bPrv + bCur) * 0.5), 0.0, nbrBins - 2.0);
        float bSup = clamp(floor((bCur + bNxt) * 0.5), bInf, nbrBins - 1.0);
        power = powerAt(u, bInf, nbrBins);
        for (int k = 1; k < 64; ++k) {
            float b = bInf + float(k);
            if (b >= bSup) break;
            power = max(power, powerAt(u, b, nbrBins));
        }
    }

    float palU = clamp(power * amplitudeScale, 0.0, 1.0);
    vec4 col = texture(u_paletteTexture, vec2(palU, styleRow));
    fragColor = vec4(col.rgb, 1.0);
}
