#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <tuple>

#include "engines.hpp"
#include "matching/version1/engine.hpp"
#include "matching/workload/command_generator.hpp"
#include "printers.hpp"

// Runs every engine listed in engines.hpp side by side with the reference engine on the same
// generated command streams, and checks that each command gives the same event batch. Version 1
// is the reference, so an engine is correct when it matches Version 1 batch for batch.

using namespace matching;

namespace {

// The engine every other engine is compared with. Version 1 is in AnyEngine too, so it is also
// compared with itself, which checks that a stream gives the same batches every time it runs.
using Reference = version1::ReferenceEngine;

struct Profile {
    const char* name;
    workload::GeneratorConfig config;
};

// The default config sends every kind of malformed command, and the benchmark profiles send only
// well-formed ones.
constexpr std::array Profiles{
    Profile{"default", workload::GeneratorConfig{}},
    Profile{"realistic", workload::profiles::realistic},
    Profile{"aggressive", workload::profiles::aggressive},
};
constexpr std::size_t CommandsPerSeed = 20'000;
constexpr std::array<std::uint64_t, 4> Seeds{1, 2, 3, 4};

// The parameters are the engine's index in test::AnyEngine, the profile's index in Profiles and
// the generator's seed.
class EngineDifferentialTest
    : public ::testing::TestWithParam<std::tuple<std::size_t, std::size_t, std::uint64_t>> {};

} // namespace

INSTANTIATE_TEST_SUITE_P(
    AllEngines, EngineDifferentialTest,
    ::testing::Combine(::testing::Range<std::size_t>(0, test::engine_names.size()),
                       ::testing::Range<std::size_t>(0, Profiles.size()),
                       ::testing::ValuesIn(Seeds)),
    [](const ::testing::TestParamInfo<EngineDifferentialTest::ParamType>& info) {
        return std::string(test::engine_names[std::get<0>(info.param)]) + "_" +
               Profiles[std::get<1>(info.param)].name + "_seed" +
               std::to_string(std::get<2>(info.param));
    });

TEST_P(EngineDifferentialTest, SameBatchesAsReference) {
    const auto [engine_index, profile_index, seed] = GetParam();
    const Profile& profile = Profiles[profile_index];
    // Rerun one stream with --gtest_filter='*_<profile>_seed<seed>'.
    SCOPED_TRACE(std::string(profile.name) + " seed " + std::to_string(seed));

    workload::CommandGenerator generator(seed, profile.config);
    const std::size_t symbol_count = profile.config.symbol_count;
    Reference reference(symbol_count);
    test::AnyEngine engine = test::make_engine(engine_index, symbol_count);

    for (std::size_t i = 1; i <= CommandsPerSeed; ++i) {
        const EngineCommand command = generator.next();
        const EventBatch reference_batch = test::process(reference, command);
        const EventBatch engine_batch = test::process(engine, command);
        EXPECT_EQ(engine_batch, reference_batch);
        if (HasFailure()) {
            FAIL() << "first difference at command " << i << " of " << profile.name << " seed "
                   << seed << ": " << ::testing::PrintToString(command);
        }
    }
}
