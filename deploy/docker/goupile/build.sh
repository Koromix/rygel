#!/bin/sh -e

git worktree remove -f build 2>/dev/null || true

git worktree add build --detach
docker build -f Dockerfile -t goupile build
git worktree remove build
