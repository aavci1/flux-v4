#!/bin/bash
set -euo pipefail

APP="$1"
SIGN_ID="$2"
INSTALLER_ID="$3"
OUT_PKG="$4"
ENTITLEMENTS="${APP}/Contents/Resources/sandbox.entitlements"

if [[ ! -d "${APP}" ]]; then
  echo "App bundle not found: ${APP}" >&2
  exit 1
fi
if [[ -z "${SIGN_ID}" || -z "${INSTALLER_ID}" ]]; then
  echo "Set FLUX_SIGN_APP_ID and FLUX_SIGN_INSTALLER_ID before running this target." >&2
  exit 1
fi

codesign --force --options runtime --timestamp \
  --entitlements "${ENTITLEMENTS}" \
  --sign "${SIGN_ID}" \
  --deep "${APP}"

codesign --verify --deep --strict --verbose=2 "${APP}"

productbuild --component "${APP}" /Applications \
  --sign "${INSTALLER_ID}" "${OUT_PKG}"
