#!/usr/bin/env bash

PROJECT=$1       # ${{ inputs.project }}
INPUT_VERSION=$2 # ${{ inputs.version }}
INCREMENT=$3     # ${{ inputs.increment || 'patch' }}

LATEST_TAG=$(git tag --list --format="%(refname:short)" --sort taggerdate "${PROJECT}*" 2>/dev/null | tail -1)
LATEST_TAG=$(echo "$LATEST_TAG" | awk -F/ '{print $NF}')
VERSION=${LATEST_TAG:-0.0.0}
if [[ -n "${INPUT_VERSION}" ]]; then
  # Strip 'v' prefix if present
  VERSION=${INPUT_VERSION#v}
else
  # Ensure we have a full semver (pad with .0 if needed)
  case $(echo "$VERSION" | tr '.' '\n' | wc -l) in
    1) VERSION="$VERSION.0.0" ;;
    2) VERSION="$VERSION.0" ;;
  esac
  npm install -g semver 2>&1 >/dev/null
  NEW_VERSION=$(semver -i $INCREMENT $VERSION)
  VERSION="$NEW_VERSION"
fi
echo "version=$VERSION"
echo "prev_version=${LATEST_TAG}"
