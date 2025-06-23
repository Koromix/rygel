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
#include "repository.hh"
#include "priv_tape.hh"

namespace RG {

void MigrateLegacySnapshot1(HeapArray<uint8_t> *blob)
{
    if (blob->len < RG_SIZE(SnapshotHeader1))
        return;

    SnapshotHeader1 *header1 = (SnapshotHeader1 *)blob->ptr;
    SnapshotHeader2 header2 = {};

    header2.time = header1->time;
    header2.size = header1->size;
    header2.stored = header1->stored;
    MemCpy(header2.channel, header1->channel, RG_SIZE(header2.channel));

    MemCpy(blob->ptr, &header2, RG_SIZE(SnapshotHeader2));
}

void MigrateLegacySnapshot2(HeapArray<uint8_t> *blob)
{
    if (blob->len < RG_SIZE(SnapshotHeader2))
        return;

    Size from = offsetof(SnapshotHeader2, channel);
    Size to = offsetof(SnapshotHeader3, channel);

    blob->Grow(to - from);

    MemMove(blob->ptr + to, blob->ptr + from, blob->len - from);
    blob->len += to - from;

    // This field did not exist before
    SnapshotHeader3 *header = (SnapshotHeader3 *)blob->ptr;
    header->added = 0;
}

void MigrateLegacyEntries1(HeapArray<uint8_t> *blob, Size start)
{
    if (blob->len < RG_SIZE(int64_t))
        return;

    blob->Grow(RG_SIZE(DirectoryHeader));

    MemMove(blob->ptr + start + RG_SIZE(DirectoryHeader), blob->ptr + start, blob->len);
    blob->len += RG_SIZE(DirectoryHeader) - RG_SIZE(int64_t);

    DirectoryHeader *header = (DirectoryHeader *)(blob->ptr + start);

    MemCpy(&header->size, blob->end(), RG_SIZE(int64_t));
    header->entries = 0;
}

void MigrateLegacyEntries2(HeapArray<uint8_t> *blob, Size start)
{
    HeapArray<uint8_t> entries;
    Size offset = start + RG_SIZE(DirectoryHeader);

    while (offset < blob->len) {
        RawEntry *ptr = (RawEntry *)(blob->ptr + offset);
        Size skip = ptr->GetSize() - 16;

        if (blob->len - offset < skip)
            break;

        entries.Grow(ptr->GetSize());
        MemCpy(entries.end(), blob->ptr + offset, skip);
        MemMove(entries.end() + offsetof(RawEntry, btime), entries.end() + offsetof(RawEntry, ctime), skip - offsetof(RawEntry, ctime));
        MemSet(entries.end() + offsetof(RawEntry, atime), 0, RG_SIZE(RawEntry::atime));
        entries.len += ptr->GetSize();

        offset += skip;
    }

    blob->RemoveFrom(start + RG_SIZE(DirectoryHeader));
    blob->Append(entries);
}

void MigrateLegacyEntries3(HeapArray<uint8_t> *blob, Size start)
{
    HeapArray<uint8_t> entries;
    Size offset = start + RG_SIZE(DirectoryHeader);

    while (offset < blob->len) {
        if (blob->len - offset < 92)
            break;

        blob->ptr[offset + 33] = blob->ptr[offset + 34];
        MemMove(blob->ptr + offset + 34, blob->ptr + offset + 36, blob->len - offset - 36);
        blob->len -= 2;

        RawEntry *ptr = (RawEntry *)(blob->ptr + offset);
        Size skip = ptr->GetSize();

        offset += skip;
    }
}

}
