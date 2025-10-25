# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

add_library(sqlite3 SHARED ../../../vendor/sqlite3mc/sqlite3.c)
set_target_properties(sqlite3 PROPERTIES PREFIX "")

if(WIN32)
    target_compile_definitions(sqlite3 PRIVATE SQLITE_API=__declspec\(dllexport\))
endif()
