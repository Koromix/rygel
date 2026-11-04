#!/bin/bash

set -e

cd "$(dirname $0)/../../.."

WORKFLOW=koffi_pgo_macos.yml
ARTIFACT=koffi_pgo_macos_arm64

commit="$(git rev-parse HEAD)"
request="${commit}_$(date -u +%Y%m%dT%H%M%SZ)"
download_dir="bin/Koffi/github/$commit/darwin_arm64"

echo ">> Triggering PGO build on GitHub Actions..."

gh workflow run $WORKFLOW -f "request=$request" -f "commit=$commit"

echo ">> Looking for workflow run..."

run_id=""
for ((i = 0; i < 60; i++)); do
    run_id=$(gh run list --workflow $WORKFLOW --event workflow_dispatch --limit 100 \
                         --json databaseId,displayTitle --jq ".[] | select(.displayTitle == \"PGO ARM64 $request\") | .databaseId")
    test -n "$run_id" && break
    sleep 2
done
if [[ -z "$run_id" ]]; then
    echo "Could not find the workflow run" >&2
    exit 1
fi

echo ">> Watching run $run_id..."

gh run watch "$run_id" --exit-status --compact

echo ">> Downloading binary: $download_dir"

rm -rf "$download_dir"
mkdir -p "$download_dir"
gh run download "$run_id" --name $ARTIFACT --dir "$download_dir"
