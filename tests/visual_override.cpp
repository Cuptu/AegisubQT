#include "VisualToolBase.h"
#include <cstdio>

#define CHECK(condition) do { if (!(condition)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); return 1; } } while (0)
int main() {
    auto set = &VisualToolBase::setOverrideTag;
    CHECK(set("{\\t(0,1000,\\frz90)}word", "\\frz", "45") == "{\\t(0,1000,\\frz90)\\frz45}word");
    CHECK(set("{\\frz10\\t(0,1000,\\frz90)\\frz20\\fr30}word", "\\frz", "45")
        == "{\\t(0,1000,\\frz90)\\frz45}word");
    CHECK(set("{\\frz10\\frx20\\fry30\\t(\\frz90)}word", "\\fr", "45")
        == "{\\frx20\\fry30\\t(\\frz90)\\fr45}word");
    CHECK(set("{\\clip(1,m 0 0 b 2 3 4 5 6 7)\\t(0,1000,\\clip(1,m 1 2 l 3 4)\\t(\\frz10))\\bord3}word", "\\iclip", "(10,20,30,40)")
        == "{\\t(0,1000,\\clip(1,m 1 2 l 3 4)\\t(\\frz10))\\bord3\\iclip(10,20,30,40)}word");
    CHECK(set("{\\iclip(1,2,3,4)\\t(\\iclip(5,6,7,8))}word", "\\clip", "(9,10,11,12)")
        == "{\\t(\\iclip(5,6,7,8))\\clip(9,10,11,12)}word");
    CHECK(set("{\\move(1,2,3,4,0,100)\\t(\\pos(6,7))\\pos(8,9)}word", "\\pos", "(10,20)")
        == "{\\t(\\pos(6,7))\\pos(10,20)}word");
    CHECK(set("{\\pos(1,2)\\t(\\move(1,2,3,4))}word", "\\move", "(5,6,7,8)")
        == "{\\t(\\move(1,2,3,4))\\move(5,6,7,8)}word");
    CHECK(set("{\\c&H123456&\\1c&H654321&\\t(\\c&HABCDEF&)}word", "\\1c", "&H0000FF&")
        == "{\\t(\\c&HABCDEF&)\\1c&H0000FF&}word");
    CHECK(set("{\\1c&H123456&\\t(\\1c&HABCDEF&)}word", "\\c", "&H0000FF&")
        == "{\\t(\\1c&HABCDEF&)\\c&H0000FF&}word");
    CHECK(set("{\\fscx\\fscx+120\\fscy85\\t(2,\\fscx50)}word", "\\fscx", "90")
        == "{\\fscy85\\t(2,\\fscx50)\\fscx90}word");
    CHECK(set("{prefix \\unknown(foo(bar),\\frz90)\\bord3.00\\frz20}word  ", "\\frz", "45")
        == "{prefix \\unknown(foo(bar),\\frz90)\\bord3.00\\frz45}word  ");
    CHECK(set("{note}word", "\\frz", "45") == "{\\frz45}{note}word");
    CHECK(set("{}word", "\\frz", "45") == "{\\frz45}word");
    CHECK(set("word{\\frz90}tail", "\\frz", "45") == "{\\frz45}word{\\frz90}tail");
    CHECK(set(QString::fromUtf8("{\\t(\\frz90)}字😀  "), "\\frz", "45") == QString::fromUtf8("{\\t(\\frz90)\\frz45}字😀  "));
    for (const auto &malformed : {"{\\t(0,100,\\frz90}word", "{\\frz10)}word"})
        CHECK(set(malformed, "\\frz", "45") == malformed);
    const QString text = "{\\t(\\frz90)\\frz45}word";
    CHECK(set(text, "\\frz", "45") == text);
    puts("PASS production VisualToolBase static override editing preserves transform/vector/unknown raw parameters, aliases, comments, Unicode, malformed scope and idempotence");
    return 0;
}
