#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
build_dir="$(cd "$1" && pwd)"
app="$build_dir/ExeCon.app"
if [[ -z "${DEVELOPER_DIR:-}" && -d /Applications/Xcode.app/Contents/Developer ]]; then
  export DEVELOPER_DIR=/Applications/Xcode.app/Contents/Developer
fi
if [[ "${EXECON_SWIFTC_APP:-0}" != 1 ]] && xcodebuild -checkFirstLaunchStatus >"$build_dir/xcode-check.log" 2>&1; then
  xcodebuild -quiet -project app/ExeCon.xcodeproj -scheme ExeCon -configuration Debug \
    -derivedDataPath "$build_dir/app-derived" CODE_SIGNING_ALLOWED=NO \
    EXECON_ENGINE_DIR="$build_dir" build
  ditto "$build_dir/app-derived/Build/Products/Debug/ExeCon.app" "$app"
else
  echo 'Xcode unavailable or not initialized; building app with Command Line Tools swiftc.' >&2
  echo "Xcode check details: $build_dir/xcode-check.log" >&2
  mkdir -p "$app/Contents/MacOS" "$app/Contents/Resources" "$build_dir/swift-module-cache"
  env DEVELOPER_DIR=/Library/Developer/CommandLineTools /Library/Developer/CommandLineTools/usr/bin/swiftc -warnings-as-errors \
    -sdk /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk -target arm64-apple-macos14.0 -module-cache-path "$build_dir/swift-module-cache" \
    -import-objc-header app/BridgingHeader.h -I engine/api -L "$build_dir" -lexecon_engine -lc++ \
    app/ConsoleDecoder.swift app/Runner.swift app/ExeConApp.swift -o "$app/Contents/MacOS/ExeCon"
  python3 - "$app/Contents/Info.plist" <<'PY'
import plistlib,re,sys
with open('app/Info.plist','rb') as f: info=plistlib.load(f)
config=open('config/product.xcconfig').read()
identity=re.search(r'^PRODUCT_BUNDLE_IDENTIFIER\s*=\s*(\S+)',config,re.M).group(1)
info['CFBundleIdentifier']=identity
info['CFBundleExecutable']='ExeCon'
with open(sys.argv[1],'wb') as f: plistlib.dump(info,f)
PY
fi
mkdir -p "$app/Contents/Helpers"
cp "$build_dir/execon" "$app/Contents/Helpers/execon"
scripts/sign.sh "$app/Contents/Helpers/execon"
codesign --force --sign "${EXECON_SIGN_IDENTITY:--}" --options runtime "$app"
codesign --verify --strict --deep "$app"
printf 'Built %s\n' "$app"
