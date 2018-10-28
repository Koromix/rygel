// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "thop.hh"
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
                if (prop.key == "DispenseMode") {
                    const OptionDesc *desc = std::find_if(std::begin(mco_DispenseModeOptions),
                                                          std::end(mco_DispenseModeOptions),
                                                          [&](const OptionDesc &desc) { return TestStr(desc.name, prop.value.ptr); });
                    if (desc == std::end(mco_DispenseModeOptions)) {
                        LogError("Unknown dispensation mode '%1'", prop.value);
                        valid = false;
                    }
                    set.dispense_mode = (mco_DispenseMode)(desc - mco_DispenseModeOptions);
                } else {
                    LogError("Unknown attribute '%1'", prop.key);
                    valid = false;
                }
            } else {
                Structure structure = {};

                // TODO: Check validity, or maybe the INI parser checks are enough?
                structure.name = MakeString(&set.str_alloc, prop.section).ptr;

                do {
                    StructureEntity ent = {};

                    ent.unit = UnitCode::FromString(prop.key);
                    valid &= ent.unit.IsValid();

                    ent.path = MakeString(&set.str_alloc, prop.value).ptr;
                    if (ent.path[0] != '|' || !ent.path[1]) {
                        LogError("Unit path does not start with '|'");
                        valid = false;
                    }

                    structure.entities.Append(ent);
                } while (ini.NextInSection(&prop));

                std::sort(structure.entities.begin(), structure.entities.end(),
                          [](const StructureEntity &ent1, const StructureEntity &ent2) {
                    return CmpStr(ent1.path, ent2.path) < 0;
                });

                if (map.Append(structure.name).second) {
                    set.structures.Append(structure);
                } else {
                    LogError("Duplicate structure '%1'", structure.name);
                    valid = false;
                }
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
    SwapMemory(out_set, &set, SIZE(set));
}
