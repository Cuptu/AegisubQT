#include "LuaTextExtents.h"

#include <QFontDatabase>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
struct Extents { double width, height, descent, leading; };
void require(bool condition, const char *message) {
    if (!condition) throw std::runtime_error(message);
}
bool close(double a, double b, double tolerance = 0.000001) {
    return std::abs(a - b) <= tolerance;
}
Extents measure(const Automation::AssStyleExtents &style, const QString &text) {
    Extents result{};
    require(Automation::CalculateTextExtents(style, text, result.width,
        result.height, result.descent, result.leading), "measurement failed");
    require(std::isfinite(result.width) && std::isfinite(result.height) &&
        std::isfinite(result.descent) && std::isfinite(result.leading), "nonfinite metrics");
    return result;
}
}

int main(int argc, char **argv) {
    try {
        Automation::AssStyleExtents style;
#ifdef AEGISUB_TEST_QT_TEXT_EXTENTS
        Extents unavailable{1, 2, 3, 4};
        require(!Automation::CalculateTextExtents(style, "test", unavailable.width,
            unavailable.height, unavailable.descent, unavailable.leading), "Qt backend needs GUI application");
        require(unavailable.width == 0 && unavailable.height == 0 &&
            unavailable.descent == 0 && unavailable.leading == 0, "failure must clear metrics");
#endif
        QGuiApplication app(argc, argv);
#ifdef _WIN32
        // The offscreen platform does not enumerate Windows system fonts.
        // Load actual installed font files into this test process explicitly.
        const QDir fontDirectory(qEnvironmentVariable("WINDIR") + "/Fonts");
        for (const auto &filename : {"arial.ttf", "times.ttf", "segoeui.ttf", "seguiemj.ttf"})
            QFontDatabase::addApplicationFont(fontDirectory.filePath(filename));
#else
        // Some offscreen builds use a basic font database on Unix too.
        for (const auto &filename : {"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
                "/System/Library/Fonts/Supplemental/Arial.ttf", "/System/Library/Fonts/Helvetica.ttc"})
            if (QFile::exists(filename)) QFontDatabase::addApplicationFont(filename);
#endif
        const auto families = QFontDatabase::families();
        require(!families.isEmpty(), "test requires an installed scalable font");
        for (const auto &preferred : {QString("Arial"), QString("DejaVu Sans"), QString("Helvetica")}) {
            if (families.contains(preferred)) { style.font = preferred; break; }
        }
        style.fontsize = 20.4;
        const auto small = measure(style, "AV To ffi");
        style.fontsize = 20.49;
        const auto large = measure(style, "AV To ffi");
        std::cout << "font=" << style.font.toStdString() << " size20.4=" << small.width << 'x' << small.height
                  << " size20.49=" << large.width << 'x' << large.height << '\n';
        require(large.width > small.width && large.height > small.height,
            "20.4 and 20.49 must not collapse to the same integer size");
        require(close(large.height, style.fontsize, 1.0 / 64.0), "height must follow ASS cell size");

        style.scalex = 175;
        style.scaley = 63;
        const auto scaled = measure(style, "AV To ffi");
        require(close(scaled.width, large.width * 1.75) && close(scaled.height, large.height * .63) &&
            close(scaled.descent, large.descent * .63) && close(scaled.leading, large.leading * .63),
            "ASS X/Y scale must apply to the correct dimensions");
        style.scalex = style.scaley = 100;

        for (double spacing : {1.25, -0.375}) {
            style.spacing = spacing;
            const auto pair = measure(style, "AV");
            const auto first = measure(style, "A");
            const auto second = measure(style, "V");
            require(close(pair.width, first.width + second.width),
                "spacing must measure characters independently and include final spacing");
#ifdef AEGISUB_TEST_QT_TEXT_EXTENTS
            const auto supplementary = QString::fromUcs4(U"\U0001F600");
            require(close(measure(style, supplementary + "A").width,
                measure(style, supplementary).width + first.width), "spacing must preserve Unicode code points");
#endif
        }
        for (double spacing : {0.0, 1.25}) {
            style.spacing = spacing;
            const auto empty = measure(style, "");
            require(empty.width == 0 && empty.height == 0, "empty text must have zero width and height");
        }

        // With spacing zero, the whole string must retain font kerning. Check
        // a real font with a known pair, rather than asserting fixed pixels.
        bool foundKerning = false;
        style.spacing = 0;
        for (const auto &family : {QString("Arial"), QString("Times New Roman"),
                QString("DejaVu Sans"), QString("Helvetica")}) {
            if (!families.contains(family)) continue;
            style.font = family;
            if (measure(style, "AV").width < measure(style, "A").width + measure(style, "V").width - .01) {
                foundKerning = true;
                break;
            }
        }
#ifdef AEGISUB_TEST_QT_TEXT_EXTENTS
        require(foundKerning, "Qt zero-spacing path must retain kerning for an installed test font");
#else
        // GDI's GetTextExtentPoint32 has its own shaping semantics; preserve
        // the native path without imposing Qt's kerning behavior on it.
        (void)foundKerning;
#endif
        style.bold = style.italic = style.underline = style.strikeout = true;
        require(measure(style, QString::fromUtf8("字幕 AV")).width > 0, "styled Unicode measurement");
        for (double badSize : {0.0, -1.0, std::numeric_limits<double>::infinity(),
                std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::max()}) {
            style.fontsize = badSize;
            Extents invalid{1, 2, 3, 4};
            require(!Automation::CalculateTextExtents(style, "test", invalid.width,
                invalid.height, invalid.descent, invalid.leading), "invalid size must fail before integer conversion");
            require(invalid.width == 0 && invalid.height == 0 && invalid.descent == 0 && invalid.leading == 0,
                "invalid measurement must clear output");
        }
        std::cout << "PASS fractional size, scaling, spacing, Unicode, kerning, empty text and invalid input\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "FAIL " << error.what() << '\n';
        return 1;
    }
}
