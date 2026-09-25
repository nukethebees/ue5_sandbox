#include <algorithm>
#include <codegen/schema/schema_version.h>
#include <gtest/gtest.h>
#include <ioj/layout/graph_view.hpp>
#include <ioj/layout/schema_loader.hpp>

namespace ioj::layout {
namespace {

TEST(GraphView, RealProjectModuleProjectionReducesScopeAndKeepsBoundaries) {
    auto const loaded{load_lispb_schema(
        std::filesystem::path{IOJ_SOURCE_DIR} / "lispb/project.lispb", "sandbox-code")};
    ASSERT_TRUE(loaded.loaded);
    auto const& types{loaded.document->types()};
    auto const project{project_graph(types, {})};
    GraphScope scope{};
    scope.module = "native_world_aabbs";
    auto const module{project_graph(types, scope)};
    ASSERT_FALSE(module.nodes.empty());
    EXPECT_LT(module.nodes.size() * 10, project.nodes.size());
    EXPECT_TRUE(std::ranges::any_of(module.nodes, [](auto const& node) { return node.boundary; }));
    std::vector<GraphPoint> sizes(project.nodes.size(), {240.0F, 60.0F});
    auto const positions{layout_graph(project, sizes)};
    auto const fit{fit_graph(positions, sizes, {1200.0F, 800.0F})};
    for (std::size_t index{}; index < positions.size(); ++index) {
        EXPECT_GE(positions[index][0] * fit.zoom + fit.pan[0], 0.0F);
        EXPECT_LE((positions[index][1] + sizes[index][1]) * fit.zoom + fit.pan[1], 800.0F);
    }
}
auto graph_fixture() -> lispb::schema::TypeGraph {
    codegen::Manifest manifest{};
    manifest.schema_version = codegen::manifest_schema_version;
    for (auto const name : {"a", "b", "c"}) {
        codegen::NormalModuleSchema module{};
        module.settings.name = name;
        module.settings.header = std::string{name} + ".h";
        codegen::RecordSchema record{};
        record.name = std::string{name} + "Record";
        for (auto const field : {"max", "x"}) {
            codegen::RecordMemberSchema member{};
            member.name = field;
            member.type.name = std::string{name} == "a" ? "bRecord" : "float";
            record.members.push_back(member);
        }
        module.declarations.push_back(record);
        manifest.modules.emplace_back(module);
    }
    return lispb::schema::resolve_type_graph(manifest);
}

TEST(GraphView, ModuleScopeKeepsDirectBoundariesWithoutExpandingTheirDependencies) {
    auto const types{graph_fixture()};
    GraphScope scope{};
    scope.module = "a";
    auto const graph{project_graph(types, scope)};
    ASSERT_EQ(graph.nodes.size(), 2);
    ASSERT_EQ(graph.edges.size(), 1);
    auto const& edge{graph.edges.front()};
    EXPECT_FALSE(graph.nodes[edge.user].boundary);
    EXPECT_TRUE(graph.nodes[edge.dependency].boundary);
    EXPECT_EQ(types.type(graph.nodes[edge.dependency].type).identity.name, "bRecord");
    EXPECT_EQ(edge.labels, (std::vector<std::string>{"max", "x"}));
    EXPECT_EQ(edge.summary, "max, x");
    scope.members.push_back(types.type(*types.find_declared("c", "cRecord")).identity);
    EXPECT_EQ(project_graph(types, scope).nodes.size(), 4);
    EXPECT_EQ(project_graph(types, {}).nodes.size(), types.types().size());
}

TEST(GraphView, FitAllEnclosesLargeNegativeAndTinyViewportBoundsBelowManualZoomFloor) {
    std::vector<GraphPoint> positions{{-10000.0F, -9000.0F}, {20000.0F, 80000.0F}};
    std::vector<GraphPoint> sizes{{320.0F, 64.0F}, {190.0F, 64.0F}};
    for (auto const viewport : {GraphPoint{800.0F, 600.0F}, GraphPoint{12.0F, 9.0F}}) {
        auto const fit{fit_graph(positions, sizes, viewport)};
        EXPECT_LT(fit.zoom, 0.1F);
        EXPECT_GT(fit.zoom, 0.0F);
        for (std::size_t index{}; index < positions.size(); ++index) {
            for (std::size_t axis{}; axis < 2; ++axis) {
                EXPECT_GE(positions[index][axis] * fit.zoom + fit.pan[axis], 0.0F);
                EXPECT_LE((positions[index][axis] + sizes[index][axis]) * fit.zoom + fit.pan[axis],
                          viewport[axis]);
            }
        }
    }
}

TEST(GraphView, DependencyRanksHandleCyclesAndPackLargeRanksDeterministically) {
    GraphProjection graph;
    graph.nodes.resize(200);
    graph.edges.push_back({0, 1, {}, {}});
    graph.edges.push_back({1, 0, {}, {}});
    graph.edges.push_back({2, 0, {}, {}});
    std::vector<GraphPoint> sizes(graph.nodes.size(), {200.0F, 60.0F});
    auto const positions{layout_graph(graph, sizes)};
    EXPECT_EQ(positions, layout_graph(graph, sizes));
    EXPECT_GT(positions[2][0], positions[0][0]);
    for (std::size_t first{}; first < positions.size(); ++first) {
        EXPECT_LT(positions[first][1], 2200.0F);
        for (std::size_t second{first + 1}; second < positions.size(); ++second) {
            EXPECT_TRUE(positions[first][0] + sizes[first][0] <= positions[second][0] ||
                        positions[second][0] + sizes[second][0] <= positions[first][0] ||
                        positions[first][1] + sizes[first][1] <= positions[second][1] ||
                        positions[second][1] + sizes[second][1] <= positions[first][1]);
        }
    }
}

TEST(GraphView, BoundedLabelsRetainFullTokensAndDistantEdgesRemainHittable) {
    std::vector<std::string> labels{std::string(200, 'x'), "other", "third"};
    auto const summary{graph_label_summary(labels)};
    EXPECT_LE(summary.size(), 53);
    EXPECT_TRUE(summary.ends_with("(+1)"));
    EXPECT_EQ(labels.front().size(), 200);
    EXPECT_TRUE(graph_segment_hit({5.0F, 2.0F}, {0.0F, 0.0F}, {10.0F, 0.0F}, 3.0F));
    EXPECT_FALSE(graph_segment_hit({15.0F, 0.0F}, {0.0F, 0.0F}, {10.0F, 0.0F}, 3.0F));
}
} // namespace
} // namespace ioj::layout
