#!/bin/bash

set -e

cd "$(dirname $0)/../../../.."

WORKFLOW=koffi_pgo_macos_arm64.yml
ARTIFACT=koffi_pgo_darwin_arm64

DOWNLOAD_DIR=$1

if [ -z "$DOWNLOAD_DIR" ]; then
    echo "Missing download directory argument" >&2
    exit 1
fi

commit="$(git rev-parse HEAD)"
request="$(date -u +%Y%m%dT%H%M%SZ) ${commit}"

echo ">> Triggering PGO build on GitHub Actions..."

gh workflow run $WORKFLOW -f "request=$request" -f "commit=$commit"

echo ">> Looking for workflow run..."

run_id=""
for ((i = 0; i < 60; i++)); do
    run_id=$(gh run list --workflow $WORKFLOW --event workflow_dispatch --limit 100 \
                         --json databaseId,displayTitle --jq ".[] | select(.displayTitle == \"$request\") | .databaseId")
    test -n "$run_id" && break
    sleep 2
done
if [[ -z "$run_id" ]]; then
    echo "Could not find the workflow run" >&2
    exit 1
fi

echo ">> Watching run $run_id..."

gh run watch "$run_id" --exit-status --compact

echo ">> Downloading binary: $DOWNLOAD_DIR"

rm -rf "$DOWNLOAD_DIR"
mkdir -p "$DOWNLOAD_DIR"
gh run download "$run_id" --name $ARTIFACT --dir "$DOWNLOAD_DIR"
