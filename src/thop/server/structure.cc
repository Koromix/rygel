// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../libcc/libcc.hh"
#include "structure.hh"

bool StructureSetBuilder::LoadIni(StreamReader &st)
{
    DEFER_NC(out_guard, len = set.structures.len) { set.structures.RemoveFrom(len); };

    IniParser ini(&st);
    ini.reader.PushLogHandler();
    DEFER { PopLogHandler(); };

    bool valid = true;
    {
        IniProperty prop;
        while (ini.Next(&prop)) {
            if (!prop.section.len) {
                LogError("Property is outside section");
                return false;
            }

            Structure structure = {};

            // TODO: Check validity, or maybe the INI parser checks are enough?
            structure.name = DuplicateString(prop.section, &set.str_alloc).ptr;

            HashSet<drd_UnitCode> units_set;
            do {
                StructureEntity ent = {};

                ent.unit = drd_UnitCode::FromString(prop.key);
                valid &= ent.unit.IsValid();

                ent.path = DuplicateString(prop.value, &set.str_alloc).ptr;
                if (ent.path[0] != '|' || !ent.path[1]) {
                    LogError("Unit path does not start with '|'");
                    valid = false;
                }

                if (units_set.Append(ent.unit).second) {
                    structure.entities.Append(ent);

                    Size *ref_count = unit_reference_counts.Append(ent.unit, 0).first;
                    (*ref_count)++;
                } else {
                    LogError("Ignoring duplicate unit %1 in structure '%2'",
                             ent.unit, structure.name);
                }
            } while (ini.NextInSection(&prop));

            std::sort(structure.entities.begin(), structure.entities.end(),
                      [](const StructureEntity &ent1, const StructureEntity &ent2) {
                return CmpStr(ent1.path, ent2.path) < 0;
            });

            if (structures_set.Append(structure.name).second) {
                set.structures.Append(structure);
            } else {
                LogError("Duplicate structure '%1'", structure.name);
                valid = false;
            }
        }
    }
    if (ini.error || !valid)
        return false;

    out_guard.disable();
    return true;
}

bool StructureSetBuilder::LoadFiles(Span<const char *const> filenames)
{
    bool success = true;

    for (const char *filename: filenames) {
        CompressionType compression_type;
        Span<const char> extension = GetPathExtension(filename, &compression_type);

        bool (StructureSetBuilder::*load_func)(StreamReader &st);
        if (extension == ".ini") {
            load_func = &StructureSetBuilder::LoadIni;
        } else {
            LogError("Cannot load structures from file '%1' with unknown extension '%2'",
                     filename, extension);
            success = false;
            continue;
        }

        StreamReader st(filename, compression_type);
        if (st.error) {
            success = false;
            continue;
        }
        success &= (this->*load_func)(st);
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

    SwapMemory(out_set, &set, SIZE(set));
}

bool LoadStructureSet(Span<const char *const> filenames, StructureSet *out_set)
{
    StructureSetBuilder structure_set_builder;
    if (!structure_set_builder.LoadFiles(filenames))
        return false;
    structure_set_builder.Finish(out_set);

    return true;
}
