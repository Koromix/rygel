// Copyright (C) 2025  Niels Martignène <niels.martignene@protonmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "src/core/base/base.hh"
#include "structure.hh"

namespace RG {

static bool CheckStructureName(Span<const char> name)
{
    const auto test_char = [](char c) { return IsAsciiAlphaOrDigit(c) || c == '.' || c == '-' || c == '_' || c == ' '; };

    if (!name.len) {
        LogError("Structure name cannot be empty");
        return false;
    }
    if (!std::all_of(name.begin(), name.end(), test_char)) {
        LogError("Structure name must only contain alphanumeric, '.', '-', '_' or ' ' characters");
        return false;
    }

    return true;
}

bool StructureSetBuilder::LoadIni(StreamReader *st)
{
    RG_DEFER_NC(out_guard, len = set.structures.len) { set.structures.RemoveFrom(len); };

    IniParser ini(st);
    ini.PushLogFilter();
    RG_DEFER { PopLogFilter(); };

    bool valid = true;
    {
        IniProperty prop;
        while (ini.Next(&prop)) {
            if (!prop.section.len) {
                LogError("Property is outside section");
                return false;
            }
            valid &= CheckStructureName(prop.section);

            Structure structure = {};
            structure.name = DuplicateString(prop.section, &set.str_alloc).ptr;

            HashSet<drd_UnitCode> units_set;
            do {
                StructureEntity ent = {};

                ent.unit = drd_UnitCode::Parse(prop.key);
                valid &= ent.unit.IsValid();

                ent.path = DuplicateString(prop.value, &set.str_alloc).ptr;
                if (ent.path[0] != '|' || !ent.path[1]) {
                    LogError("Unit path does not start with '|'");
                    valid = false;
                }

                bool inserted;
                units_set.TrySet(ent.unit, &inserted);

                if (inserted) {
                    structure.entities.Append(ent);

                    Size *ptr = unit_reference_counts.TrySet(ent.unit, 0);
                    (*ptr)++;
                } else {
                    LogError("Ignoring duplicate unit %1 in structure '%2'",
                             ent.unit, structure.name);
                }
            } while (ini.NextInSection(&prop));

            std::sort(structure.entities.begin(), structure.entities.end(),
                      [](const StructureEntity &ent1, const StructureEntity &ent2) {
                return CmpStr(ent1.path, ent2.path) < 0;
            });

            bool inserted;
            structures_set.TrySet(structure.name, &inserted);

            if (inserted) {
                set.structures.Append(structure);
            } else {
                LogError("Duplicate structure '%1'", structure.name);
                valid = false;
            }
        }
    }
    if (!ini.IsValid() || !valid)
        return false;

    out_guard.Disable();
    return true;
}

bool StructureSetBuilder::LoadFiles(Span<const char *const> filenames)
{
    bool success = true;

    for (const char *filename: filenames) {
        CompressionType compression_type;
        Span<const char> extension = GetPathExtension(filename, &compression_type);

        bool (StructureSetBuilder::*load_func)(StreamReader *st);
        if (extension == ".ini") {
            load_func = &StructureSetBuilder::LoadIni;
        } else {
            LogError("Cannot load structures from file '%1' with unknown extension '%2'",
                     filename, extension);
            success = false;
            continue;
        }

        StreamReader st(filename, compression_type);
        if (!st.IsValid()) {
            success = false;
            continue;
        }
        success &= (this->*load_func)(&st);
    }

    return success;
}

void StructureSetBuilder::Finish(StructureSet *out_set)
{
    for (const auto &it: unit_reference_counts.table) {
        if (it.value != set.structures.len) {
            LogError("Unit %1 is missing in some structures", it.key);
        }
    }

    std::swap(*out_set, set);
}

bool LoadStructureSet(Span<const char *const> filenames, StructureSet *out_set)
{
    StructureSetBuilder structure_set_builder;
    if (!structure_set_builder.LoadFiles(filenames))
        return false;
    structure_set_builder.Finish(out_set);

    return true;
}

}
