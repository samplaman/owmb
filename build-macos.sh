#!/usr/bin/env bash
set -e

# OpenWav CMake & Kit Build Script for macOS (with Code Signing & Notarization)
# Usage:
#   ./build-macos.sh                                       # Builds for macOS Monterey Intel x86_64
#   ./build-macos.sh --universal                           # Builds Universal (arm64 + x86_64)
#   ./build-macos.sh --sign "Developer ID Application: .." # Builds and signs binaries
#   ./build-macos.sh --sign "..." --notarize               # Builds, signs, and notarizes with Apple

TARGET_TYPE="monterey-intel"
SIGN_IDENTITY="${CODESIGN_IDENTITY:-}"
DO_NOTARIZE=false
APPLE_ID="${APPLE_ID:-}"
APPLE_APP_SPECIFIC_PASSWORD="${APPLE_APP_SPECIFIC_PASSWORD:-}"
APPLE_TEAM_ID="${APPLE_TEAM_ID:-}"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --universal)
            TARGET_TYPE="universal"
            shift
            ;;
        --native)
            TARGET_TYPE="native"
            shift
            ;;
        --intel|--monterey-intel)
            TARGET_TYPE="monterey-intel"
            shift
            ;;
        --sign)
            SIGN_IDENTITY="$2"
            shift 2
            ;;
        --notarize)
            DO_NOTARIZE=true
            shift
            ;;
        --apple-id)
            APPLE_ID="$2"
            shift 2
            ;;
        --password)
            APPLE_APP_SPECIFIC_PASSWORD="$2"
            shift 2
            ;;
        --team-id)
            APPLE_TEAM_ID="$2"
            shift 2
            ;;
        *)
            echo "Unknown option: $1"
            echo "Usage: ./build-macos.sh [--intel|--universal|--native] [--sign <identity>] [--notarize] [--apple-id <email>] [--password <app-pwd>] [--team-id <team-id>]"
            exit 1
            ;;
    esac
done

echo "==> Building OpenWav for target: $TARGET_TYPE"

BUILD_DIR="build"
CMAKE_EXTRA_FLAGS=()
DIST_NAME=""

if [ "$TARGET_TYPE" = "monterey-intel" ]; then
    DIST_NAME="OWMB-macOS-Monterey-Intel-x64"
    CMAKE_EXTRA_FLAGS+=(
        "-DCMAKE_OSX_ARCHITECTURES=x86_64"
        "-DCMAKE_OSX_DEPLOYMENT_TARGET=12.0"
    )
elif [ "$TARGET_TYPE" = "universal" ]; then
    DIST_NAME="OWMB-macOS-Universal"
    CMAKE_EXTRA_FLAGS+=(
        "-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64"
        "-DCMAKE_OSX_DEPLOYMENT_TARGET=10.15"
    )
else
    DIST_NAME="OWMB-macOS-Native"
fi

# 1. Configure CMake
echo "==> Configuring CMake..."
cmake -B "$BUILD_DIR" -S . -DCMAKE_BUILD_TYPE=Release "${CMAKE_EXTRA_FLAGS[@]}"

# 2. Build
echo "==> Compiling OpenWav (VST3, AU, Standalone)..."
cmake --build "$BUILD_DIR" --config Release --parallel $(sysctl -n hw.ncpu 2>/dev/null || echo 4)

# 3. Code Signing (if identity provided or found in Keychain)
ENTITLEMENTS_FILE="entitlements.plist"

if [ -z "$SIGN_IDENTITY" ]; then
    # Try to auto-detect a Developer ID Application certificate
    DETECTED_ID=$(security find-identity -v -p codesigning 2>/dev/null | grep "Developer ID Application:" | head -n 1 | sed -E 's/.*"([^"]+)".*/\1/' || true)
    if [ -n "$DETECTED_ID" ]; then
        echo "==> Auto-detected Developer ID signing certificate: $DETECTED_ID"
        SIGN_IDENTITY="$DETECTED_ID"
    fi
fi

if [ -n "$SIGN_IDENTITY" ]; then
    echo "==> Signing binaries with identity: $SIGN_IDENTITY"
    if [ ! -f "$ENTITLEMENTS_FILE" ]; then
        echo "Warning: $ENTITLEMENTS_FILE not found. Signing without explicit entitlements."
        ENTITLEMENTS_ARG=""
    else
        ENTITLEMENTS_ARG="--entitlements $ENTITLEMENTS_FILE"
    fi

    # Find and sign VST3, AU, and Standalone App bundles
    while IFS= read -r -d '' bundle; do
        echo "    -> Signing bundle: $bundle"
        codesign --force --deep --options runtime --timestamp \
            $ENTITLEMENTS_ARG \
            --sign "$SIGN_IDENTITY" "$bundle"
        codesign --verify --deep --strict --verbose=2 "$bundle"
    done < <(find "$BUILD_DIR/OpenWav_artefacts" \( -name "OWMB.vst3" -o -name "OWMB.component" -o -name "OWMB.app" \) -print0)
    echo "==> Code signing complete!"
else
    echo "==> Note: No codesigning identity provided. Binaries built unsigned (adhoc)."
fi

# 4. Packaging
echo "==> Packaging release into dist/$DIST_NAME..."
rm -rf "dist/$DIST_NAME" "$DIST_NAME.zip" "$DIST_NAME.dmg"
mkdir -p "dist/$DIST_NAME"

find "$BUILD_DIR/OpenWav_artefacts" -name "OWMB.vst3" -exec cp -R {} "dist/$DIST_NAME/" \; 2>/dev/null || true
find "$BUILD_DIR/OpenWav_artefacts" -name "OWMB.component" -exec cp -R {} "dist/$DIST_NAME/" \; 2>/dev/null || true
find "$BUILD_DIR/OpenWav_artefacts" -name "OWMB.app" -exec cp -R {} "dist/$DIST_NAME/" \; 2>/dev/null || true

# 4a. Create ZIP Archive
cd "dist/$DIST_NAME"
zip -r -y "../../$DIST_NAME.zip" .
cd ../..

# 4b. Create DMG Disk Image
echo "==> Creating Disk Image: $DIST_NAME.dmg..."
hdiutil create -volname "OWMB" -srcfolder "dist/$DIST_NAME" -ov -format UDZO "$DIST_NAME.dmg"

if [ -n "$SIGN_IDENTITY" ]; then
    echo "==> Signing Disk Image: $DIST_NAME.dmg"
    codesign --force --sign "$SIGN_IDENTITY" --timestamp "$DIST_NAME.dmg"
    codesign --verify --verbose=2 "$DIST_NAME.dmg"
fi

# 5. Notarization & Stapling (if enabled)
if [ "$DO_NOTARIZE" = true ]; then
    if [ -z "$APPLE_ID" ] || [ -z "$APPLE_APP_SPECIFIC_PASSWORD" ] || [ -z "$APPLE_TEAM_ID" ]; then
        echo "ERROR: Notarization requires --apple-id, --password (app-specific), and --team-id"
        exit 1
    fi

    echo "==> Submitting $DIST_NAME.zip to Apple Notary Service..."
    xcrun notarytool submit "$DIST_NAME.zip" \
        --apple-id "$APPLE_ID" \
        --password "$APPLE_APP_SPECIFIC_PASSWORD" \
        --team-id "$APPLE_TEAM_ID" \
        --wait

    echo "==> Submitting $DIST_NAME.dmg to Apple Notary Service..."
    xcrun notarytool submit "$DIST_NAME.dmg" \
        --apple-id "$APPLE_ID" \
        --password "$APPLE_APP_SPECIFIC_PASSWORD" \
        --team-id "$APPLE_TEAM_ID" \
        --wait

    echo "==> Stapling notarization ticket to Standalone .app..."
    for app in $(find "dist/$DIST_NAME" -name "OWMB.app"); do
        xcrun stapler staple "$app"
    done

    echo "==> Stapling notarization ticket to DMG..."
    xcrun stapler staple "$DIST_NAME.dmg"

    # Re-package zip to include stapled ticket
    echo "==> Updating $DIST_NAME.zip with stapled ticket..."
    cd "dist/$DIST_NAME"
    zip -r -y "../../$DIST_NAME.zip" .
    cd ../..
    echo "==> Notarization & Stapling Complete!"
fi

echo "==> Build and Packaging Complete!"
echo "    Zip Archive: $DIST_NAME.zip"
echo "    Disk Image:  $DIST_NAME.dmg"
echo "    Contents in: dist/$DIST_NAME"
