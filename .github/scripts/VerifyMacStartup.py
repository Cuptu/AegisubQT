"""Run the final deployed application, including its real Main.qml and window."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument("bundle", type=Path)
args = parser.parse_args()
bundle = args.bundle.resolve()
resources = bundle / "Contents/Resources"
for name in ("QtQuick/qmldir", "QtQuick/libqtquick2plugin.dylib",
             "QtQuick/Controls/qmldir", "QtQuick/Controls/libqtquickcontrols2plugin.dylib"):
    if not (resources / "qml" / name).is_file():
        raise SystemExit(f"Missing final DMG QML module file: {name}")
if not (resources / "appqml/Main.qml").is_file():
    raise SystemExit("Missing final DMG application Main.qml")

with tempfile.TemporaryDirectory(prefix="Aegisub startup 核验 ") as work:
    work = Path(work)
    (work / "qml").mkdir()
    (work / "qml/Main.qml").write_text(
        'import QtQml; QtObject { Component.onCompleted: { '
        'console.error("AEGISUB_UNTRUSTED_CWD_QML_EXECUTED"); Qt.quit() } }', encoding="utf-8")
    for backend in ("software", "default"):
        env = dict(os.environ)
        for key in list(env):
            if key.startswith(("DYLD_", "QML_", "QML2_", "QT_", "LUA_", "LUAJIT_")):
                env.pop(key)
        env.update(PATH="/usr/bin:/bin:/usr/sbin:/sbin", HOME=str(work),
                   QT_QPA_PLATFORM="cocoa")
        if backend == "software":
            env["QT_QUICK_BACKEND"] = "software"
        log = work / f"startup-cocoa-{backend}.log"
        verification = subprocess.run([str(bundle / "Contents/MacOS/AegisubQT"), "--verify-subtitle-renderer"],
                                      cwd=work, env=env, capture_output=True, timeout=30)
        verification_text = (verification.stdout + verification.stderr).decode("utf-8", errors="replace")
        print(verification_text, flush=True)
        if verification.returncode or "PASS packaged SRT import and libass subtitle rendering" not in verification_text:
            raise SystemExit("Final DMG cannot import/render SRT with its packaged libass")
        with log.open("wb") as output:
            process = subprocess.Popen([str(bundle / "Contents/MacOS/AegisubQT")],
                                       cwd=work, env=env, stdout=output, stderr=output)
            survived = False
            try:
                process.wait(timeout=8)
            except subprocess.TimeoutExpired:
                survived = True
            finally:
                if process.poll() is None:
                    process.terminate()
                    try:
                        process.wait(timeout=5)
                    except subprocess.TimeoutExpired:
                        process.kill()
                        process.wait()
        text = log.read_text(encoding="utf-8", errors="replace")
        print(text, flush=True)
        if not survived or "QML loaded. Root objects count: 1" not in text or \
                "AEGISUB_UNTRUSTED_CWD_QML_EXECUTED" in text:
            raise SystemExit(f"Final DMG main application failed cocoa/{backend} startup (exit {process.returncode})")
        print(f"PASS final DMG isolated main application startup: cocoa/{backend}", flush=True)
