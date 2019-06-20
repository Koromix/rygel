@echo off
setlocal

REM This Source Code Form is subject to the terms of the Mozilla Public
REM License, v. 2.0. If a copy of the MPL was not distributed with this
REM file, You can obtain one at http://mozilla.org/MPL/2.0/.

cd "%~dp0"
REM npm install --silent --no-audit --no-optional --no-bin-links
node "node_modules\@babel\cli\bin\babel.js" --filename compat.js --config-file ./config.js
