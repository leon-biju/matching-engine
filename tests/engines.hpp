#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "matching/core/engine.hpp"
#include "matching/version1/engine.hpp"

namespace matching::test {

// Every engine the shared tests run against. To add an engine, append its type to AnyEngine
// and its name to engine_names, in the same position. Nothing else needs to change.
using AnyEngine = std::variant<version1::ReferenceEngine>;
inline constexpr std::array engine_names{"version1"};

static_assert(engine_names.size() == std::variant_size_v<AnyEngine>,
              "engine_names must name every engine in AnyEngine");

template <typename>
inline constexpr bool all_engines = false;
template <typename... Es>
inline constexpr bool all_engines<std::variant<Es...>> = (Engine<Es> && ...);
static_assert(all_engines<AnyEngine>, "every engine in AnyEngine must satisfy matching::Engine");

namespace detail {

template <std::size_t... I>
AnyEngine make_engine(std::size_t index, std::vector<std::string> symbols,
                      std::index_sequence<I...>) {
    using Factory = AnyEngine (*)(std::vector<std::string>);
    constexpr std::array<Factory, sizeof...(I)> factories{
        [](std::vector<std::string> s) { return AnyEngine{std::in_place_index<I>, std::move(s)}; }...};
    return factories.at(index)(std::move(symbols));
}

} // namespace detail

// Builds the engine at `index` in AnyEngine.
inline AnyEngine make_engine(std::size_t index, std::vector<std::string> symbols) {
    return detail::make_engine(index, std::move(symbols),
                               std::make_index_sequence<std::variant_size_v<AnyEngine>>{});
}

} // namespace matching::test
