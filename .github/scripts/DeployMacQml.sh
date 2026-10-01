#!/usr/bin/env bash
set -euo pipefail
bundle="$1"
root="$(cd "$(dirname "$0")/../.." && pwd)"
resources="$bundle/Contents/Resources"
mkdir -p "$resources/appqml"
ditto "$root/qml" "$resources/appqml"
# Keep application source out of macdeployqt's Qt module destination. A partial
# QtQuick directory pre-created by an application override causes skipped files.
[[ ! -e "$resources/qml" ]]
macdeployqt "$bundle" -qmldir="$root/qml" -verbose=1
for file in QtQuick/qmldir QtQuick/libqtquick2plugin.dylib \
    QtQuick/Controls/qmldir QtQuick/Controls/libqtquickcontrols2plugin.dylib \
    QtQuick/Templates/qmldir QtQuick/Layouts/qmldir QtQuick/Dialogs/qmldir; do
    [[ -f "$resources/qml/$file" ]] || { echo "Missing deployed Qt QML file: $file" >&2; exit 1; }
done
# Apply the custom tooltip only after Qt's complete modules have been deployed.
cp "$root/qml/QtQuick/Controls/Fusion/ToolTip.qml" "$resources/qml/QtQuick/Controls/Fusion/ToolTip.qml"
rm -rf "$resources/appqml/QtQuick"
echo "Qt QML modules and separate application QML deployed successfully."
