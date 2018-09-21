// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "drdw.hh"

struct ReadableGhmDecisionNode {
    const char *key;
    const char *header;
    const char *text;
    const char *reverse;

    uint8_t function;
    Size children_idx;
    Size children_count;
};

struct BuildReadableGhmTreeContext {
    Span<const mco_GhmDecisionNode> ghm_nodes;
    Span<ReadableGhmDecisionNode> out_nodes;

    int8_t cmd;

    Allocator *str_alloc;
};

static bool ProcessGhmNode(BuildReadableGhmTreeContext &ctx, Size ghm_node_idx);
static Size ProcessGhmTest(BuildReadableGhmTreeContext &ctx,
                           const mco_GhmDecisionNode &ghm_node,
                           ReadableGhmDecisionNode *out_node)
{
    DebugAssert(ghm_node.type == mco_GhmDecisionNode::Type::Test);

    out_node->key = Fmt(ctx.str_alloc, "%1%2%3",
                        FmtHex(ghm_node.u.test.function).Pad0(-2),
                        FmtHex(ghm_node.u.test.params[0]).Pad0(-2),
                        FmtHex(ghm_node.u.test.params[1]).Pad0(-2)).ptr;

    // FIXME: Check children_idx and children_count
    out_node->function = ghm_node.u.test.function;
    out_node->children_idx = ghm_node.u.test.children_idx;
    out_node->children_count = ghm_node.u.test.children_count;

    switch (ghm_node.u.test.function) {
        case 0:
        case 1: {
            if (ghm_node.u.test.params[0] == 0) {
                out_node->text = "DP";

                int8_t prev_cmd = ctx.cmd;
                for (Size i = 1; i < ghm_node.u.test.children_count; i++) {
                    Size child_idx = ghm_node.u.test.children_idx + i;

                    ctx.cmd = (int8_t)i;
                    ctx.out_nodes[child_idx].header = Fmt(ctx.str_alloc, "D-%1",
                                                          FmtArg(ctx.cmd).Pad0(-2)).ptr;
                    if (UNLIKELY(!ProcessGhmNode(ctx, child_idx)))
                        return -1;
                }
                ctx.cmd = prev_cmd;

                return ghm_node.u.test.children_idx;
            } else if (ghm_node.u.test.params[0] == 1) {
                out_node->text = "DP";

                for (Size i = 1; i < ghm_node.u.test.children_count; i++) {
                    Size child_idx = ghm_node.u.test.children_idx + i;

                    ctx.out_nodes[child_idx].header = Fmt(ctx.str_alloc, "D-%1%2",
                                                          FmtArg(ctx.cmd).Pad0(-2),
                                                          FmtArg(i).Pad0(-2)).ptr;
                    if (UNLIKELY(!ProcessGhmNode(ctx, child_idx)))
                        return -1;
                }

                return ghm_node.u.test.children_idx;
            } else {
                out_node->text = Fmt(ctx.str_alloc, "DP (byte %1)",
                                     ghm_node.u.test.params[0]).ptr;
            }
        } break;

        case 2: {
            out_node->text = Fmt(ctx.str_alloc, "Acte A$%1.%2",
                                 ghm_node.u.test.params[0], ghm_node.u.test.params[1]).ptr;
        } break;

        case 3: {
            if (ghm_node.u.test.params[1] == 1) {
                out_node->text = Fmt(ctx.str_alloc, "Age (jours) > %1",
                                     ghm_node.u.test.params[0]).ptr;
                if (ghm_node.u.test.params[0] == 7) {
                    out_node->reverse = "Age (jours) ≤ 7";
                }
            } else {
                out_node->text = Fmt(ctx.str_alloc, "Age > %1",
                                     ghm_node.u.test.params[0]).ptr;
            }
        } break;

        case 5: {
            out_node->text = Fmt(ctx.str_alloc, "DP D$%1.%2",
                                 ghm_node.u.test.params[0], ghm_node.u.test.params[1]).ptr;
        } break;

        case 6: {
            out_node->text = Fmt(ctx.str_alloc, "DAS D$%1.%2",
                                 ghm_node.u.test.params[0], ghm_node.u.test.params[1]).ptr;
        } break;

        case 7: {
            out_node->text = Fmt(ctx.str_alloc, "DP / DR / DAS D$%1.%2",
                                 ghm_node.u.test.params[0], ghm_node.u.test.params[1]).ptr;
        } break;

        case 9: {
            // TODO: Text for test 9 is inexact
            out_node->text = Fmt(ctx.str_alloc, "Tous actes A$%1.%2",
                                 ghm_node.u.test.params[0], ghm_node.u.test.params[1]).ptr;
        } break;

        case 10: {
            out_node->text = Fmt(ctx.str_alloc, "2 actes A$%1.%2",
                                 ghm_node.u.test.params[0], ghm_node.u.test.params[1]).ptr;
        } break;

        case 13: {
            if (ghm_node.u.test.params[0] == 0) {
                out_node->text = Fmt(ctx.str_alloc, "DP D-%1",
                                     FmtArg(ghm_node.u.test.params[1]).Pad0(-2)).ptr;

                int8_t prev_cmd = ctx.cmd;
                for (Size i = 1; i < ghm_node.u.test.children_count; i++) {
                    Size child_idx = ghm_node.u.test.children_idx + i;

                    ctx.cmd = ghm_node.u.test.params[1];
                    if (UNLIKELY(!ProcessGhmNode(ctx, child_idx)))
                        return -1;
                }
                ctx.cmd = prev_cmd;

                return ghm_node.u.test.children_idx;
            } else if (ghm_node.u.test.params[0] == 1) {
                out_node->text = Fmt(ctx.str_alloc, "DP D-%1%2",
                                     FmtArg(ctx.cmd).Pad0(-2),
                                     FmtArg(ghm_node.u.test.params[1]).Pad0(-2)).ptr;
            } else {
                out_node->text = Fmt(ctx.str_alloc, "DP byte %1 = %2",
                                     ghm_node.u.test.params[0], ghm_node.u.test.params[1]).ptr;
            }
        } break;

        case 14: {
            switch (ghm_node.u.test.params[0]) {
                case '1': { out_node->text = "Sexe = Homme"; } break;
                case '2': { out_node->text = "Sexe = Femme"; } break;
                default: { return -1; } break;
            }
        } break;

        case 18: {
            // TODO: Text for test 18 is inexact
            out_node->text = Fmt(ctx.str_alloc, "2 DAS D$%1.%2",
                                 ghm_node.u.test.params[0], ghm_node.u.test.params[1]).ptr;
        } break;

        case 19: {
            switch (ghm_node.u.test.params[1]) {
                case 0: { out_node->text = Fmt(ctx.str_alloc, "Mode de sortie = %1",
                                               ghm_node.u.test.params[0]).ptr; } break;
                case 1: { out_node->text = Fmt(ctx.str_alloc, "Destination = %1",
                                               ghm_node.u.test.params[0]).ptr; } break;
                case 2: { out_node->text = Fmt(ctx.str_alloc, "Mode d'entrée = %1",
                                               ghm_node.u.test.params[0]).ptr; } break;
                case 3: { out_node->text = Fmt(ctx.str_alloc, "Provenance = %1",
                                               ghm_node.u.test.params[0]).ptr; } break;
                default: { return -1; } break;
            }
        } break;

        case 20: {
            out_node->text = Fmt(ctx.str_alloc, "Saut noeud %1",
                                 ghm_node.u.test.children_idx).ptr;
        } break;

        case 22: {
            uint16_t param = MakeUInt16(ghm_node.u.test.params[0], ghm_node.u.test.params[1]);
            out_node->text = Fmt(ctx.str_alloc, "Durée < %1", param).ptr;
        } break;

        case 26: {
            out_node->text = Fmt(ctx.str_alloc, "DR D$%1.%2",
                                 ghm_node.u.test.params[0], ghm_node.u.test.params[1]).ptr;
        } break;

        case 28: {
            out_node->text = Fmt(ctx.str_alloc, "Erreur non bloquante %1",
                                 ghm_node.u.test.params[0]).ptr;
        } break;

        case 29: {
            uint16_t param = MakeUInt16(ghm_node.u.test.params[0], ghm_node.u.test.params[1]);
            out_node->text = Fmt(ctx.str_alloc, "Durée = %1", param).ptr;
        } break;

        case 30: {
            uint16_t param = MakeUInt16(ghm_node.u.test.params[0], ghm_node.u.test.params[1]);
            out_node->text = Fmt(ctx.str_alloc, "Nombre de séances = %1", param).ptr;
            if (param == 0) {
                out_node->reverse = "Nombre de séances > 0";
            }
        } break;

        case 33: {
            out_node->text = Fmt(ctx.str_alloc, "Acte avec activité %1",
                                 ghm_node.u.test.params[0]).ptr;
        } break;

        case 34: {
            out_node->text = "Inversion DP / DR";
        } break;

        case 35: {
            out_node->text = "DP / DR inversés";
        } break;

        case 36: {
            out_node->text = Fmt(ctx.str_alloc, "DP ou DAS D$%1.%2",
                                 ghm_node.u.test.params[0], ghm_node.u.test.params[1]).ptr;
        } break;

        case 38: {
            out_node->text = Fmt(ctx.str_alloc, "GNN ≥ %1 et ≤ %2",
                                 ghm_node.u.test.params[0], ghm_node.u.test.params[1]).ptr;
        } break;

        case 39: {
            out_node->text = "Calcul du GNN";
        } break;

        case 41: {
            out_node->text = Fmt(ctx.str_alloc, "DP / DR / DAS D-%1%2",
                                 FmtArg(ghm_node.u.test.params[0]).Pad0(-2),
                                 FmtArg(ghm_node.u.test.params[1]).Pad0(-2)).ptr;
        } break;

        case 42: {
            uint16_t param = MakeUInt16(ghm_node.u.test.params[0], ghm_node.u.test.params[1]);
            out_node->text = Fmt(ctx.str_alloc, "Poids (NN) > 0 et < %1", param).ptr;
        } break;

        case 43: {
            out_node->text = Fmt(ctx.str_alloc, "DP ou DAS D-%1%2",
                                 FmtArg(ghm_node.u.test.params[0]).Pad0(-2),
                                 FmtArg(ghm_node.u.test.params[1]).Pad0(-2)).ptr;
        } break;

        default: {
            out_node->text = Fmt(ctx.str_alloc, "Test inconnu %1 (%2, %3)",
                                 ghm_node.u.test.function,
                                 ghm_node.u.test.params[0], ghm_node.u.test.params[1]).ptr;
        } break;
    }

    for (Size i = 1; i < ghm_node.u.test.children_count; i++) {
        Size child_idx = ghm_node.u.test.children_idx + i;
        if (UNLIKELY(!ProcessGhmNode(ctx, child_idx)))
            return -1;
    }

    return ghm_node.u.test.children_idx;
}

static bool ProcessGhmNode(BuildReadableGhmTreeContext &ctx, Size ghm_node_idx)
{
    for (Size i = 0;; i++) {
        if (UNLIKELY(i >= ctx.ghm_nodes.len)) {
            LogError("Empty GHM tree or infinite loop (%2)", ctx.ghm_nodes.len);
            return false;
        }

        DebugAssert(ghm_node_idx < ctx.ghm_nodes.len);
        const mco_GhmDecisionNode &ghm_node = ctx.ghm_nodes[ghm_node_idx];
        ReadableGhmDecisionNode *out_node = &ctx.out_nodes[ghm_node_idx];

        switch (ghm_node.type) {
            case mco_GhmDecisionNode::Type::Test: {
                ghm_node_idx = ProcessGhmTest(ctx, ghm_node, out_node);
                if (UNLIKELY(ghm_node_idx < 0))
                    return false;

                // GOTO is special
                if (ghm_node.u.test.function == 20)
                    return true;
            } break;

            case mco_GhmDecisionNode::Type::Ghm: {
                out_node->key = Fmt(ctx.str_alloc, "%1", ghm_node.u.ghm.ghm).ptr;
                if (ghm_node.u.ghm.error) {
                    out_node->text = Fmt(ctx.str_alloc, "GHM %1 [%2]",
                                         ghm_node.u.ghm.ghm, ghm_node.u.ghm.error).ptr;
                } else {
                    out_node->text = Fmt(ctx.str_alloc, "GHM %1", ghm_node.u.ghm.ghm).ptr;
                }
                return true;
            } break;
        }
    }

    return true;
}

// TODO: Move to libdrd, add classifier_tree export to drdR
static bool BuildReadableGhmTree(Span<const mco_GhmDecisionNode> ghm_nodes,
                                 HeapArray<ReadableGhmDecisionNode> *out_nodes,
                                 Allocator *str_alloc)
{
    if (!ghm_nodes.len)
        return true;

    BuildReadableGhmTreeContext ctx = {};

    ctx.ghm_nodes = ghm_nodes;
    out_nodes->Grow(ghm_nodes.len);
    ctx.out_nodes = MakeSpan(out_nodes->end(), ghm_nodes.len);
    memset(ctx.out_nodes.ptr, 0, ctx.out_nodes.len * SIZE(*ctx.out_nodes.ptr));
    ctx.str_alloc = str_alloc;

    if (!ProcessGhmNode(ctx, 0))
        return false;

    out_nodes->len += ghm_nodes.len;
    return true;
}

Response ProduceClassifierTree(const ConnectionInfo *conn, const char *, CompressionType compression_type)
{
    Response response = {};
    const mco_TableIndex *index = GetIndexFromQueryString(conn, "tree.json", &response);
    if (!index)
        return response;

    // TODO: Generate ahead of time
    LinkedAllocator readable_nodes_alloc;
    HeapArray<ReadableGhmDecisionNode> readable_nodes;
    if (!BuildReadableGhmTree(index->ghm_nodes, &readable_nodes, &readable_nodes_alloc))
        return CreateErrorPage(500);

    response.code = 200;
    response.response = BuildJson(compression_type,
                                  [&](rapidjson::Writer<JsonStreamWriter> &writer) {
        LinkedAllocator temp_alloc;

        writer.StartArray();
        for (const ReadableGhmDecisionNode &readable_node: readable_nodes) {
            writer.StartObject();
            if (readable_node.header) {
                writer.Key("header"); writer.String(readable_node.header);
            }
            writer.Key("text"); writer.String(readable_node.text);
            if (readable_node.reverse) {
                writer.Key("reverse"); writer.String(readable_node.reverse);
            }
            if (readable_node.children_idx) {
                writer.Key("key"); writer.String(readable_node.key);
                writer.Key("test"); writer.Int(readable_node.function);
                writer.Key("children_idx"); writer.Int64(readable_node.children_idx);
                writer.Key("children_count"); writer.Int64(readable_node.children_count);
            }
            writer.EndObject();
        }
        writer.EndArray();
        return true;
    });

    return response;
}
