#include <gtest/gtest.h>

#include <cstddef>
#include <sstream>
#include <stdexcept>
#include <string>

#include "matching/workload/command_generator.hpp"
#include "matching/workload/workload_file.hpp"

using namespace matching;
using namespace matching::workload;

namespace {

// The default generator config sends every kind of malformed command, so a round trip of its
// stream covers absent prices, prices of 0, unknown symbols and zero quantities.
Workload generated(std::size_t count) {
    CommandGenerator generator(3);
    Workload workload{.symbol_count = generator.config().symbol_count, .commands = {}};
    for (std::size_t i = 0; i < count; ++i) {
        workload.commands.push_back(generator.next());
    }
    return workload;
}

std::string written(const Workload& workload) {
    std::ostringstream out;
    write_workload(out, workload);
    return out.str();
}

Workload read(const std::string& bytes) {
    std::istringstream in(bytes);
    return read_workload(in);
}

} // namespace

TEST(WorkloadFileTest, RoundTripsEveryCommand) {
    const Workload workload = generated(5'000);
    EXPECT_EQ(read(written(workload)), workload);
}

TEST(WorkloadFileTest, RoundTripsEmptyWorkload) {
    const Workload workload{.symbol_count = 2, .commands = {}};
    EXPECT_EQ(read(written(workload)), workload);
}

TEST(WorkloadFileTest, HasFixedSizeRecords) {
    EXPECT_EQ(written(generated(10)).size(), format::header_size + 10 * format::command_size);
}

TEST(WorkloadFileTest, WritesLittleEndianFields) {
    const Workload workload{
        .symbol_count = 1,
        .commands = {NewOrder{.id = 0x0102,
                              .symbol = 0,
                              .side = OrderSide::Sell,
                              .type = OrderType::LimitIOC,
                              .qty = 7,
                              .price = 0x0A0B}},
    };
    const std::string bytes = written(workload);
    const std::string record = bytes.substr(format::header_size);
    EXPECT_EQ(bytes.substr(0, 4), "MEWL");
    EXPECT_EQ(record[1], 1); // Sell
    EXPECT_EQ(record[2], 1); // LimitIOC
    EXPECT_EQ(record[3], 1); // Has a price
    EXPECT_EQ(record[8], 0x02);
    EXPECT_EQ(record[9], 0x01);
    EXPECT_EQ(record[16], 7);
    EXPECT_EQ(record[24], 0x0B);
    EXPECT_EQ(record[25], 0x0A);
}

TEST(WorkloadFileTest, RejectsWrongMagic) {
    std::string bytes = written(generated(1));
    bytes[0] = 'X';
    EXPECT_THROW(read(bytes), std::runtime_error);
}

TEST(WorkloadFileTest, RejectsWrongVersion) {
    std::string bytes = written(generated(1));
    bytes[4] = 2;
    EXPECT_THROW(read(bytes), std::runtime_error);
}

TEST(WorkloadFileTest, RejectsTruncatedFile) {
    const std::string bytes = written(generated(3));
    EXPECT_THROW(read(bytes.substr(0, bytes.size() - 1)), std::runtime_error);
    EXPECT_THROW(read(bytes.substr(0, format::header_size - 1)), std::runtime_error);
}

TEST(WorkloadFileTest, RejectsInvalidFieldValues) {
    const Workload workload{
        .symbol_count = 1,
        .commands = {NewOrder{.id = 1,
                              .symbol = 0,
                              .side = OrderSide::Buy,
                              .type = OrderType::LimitGTC,
                              .qty = 1,
                              .price = 1}},
    };
    // The kind, side, type and has-price bytes.
    for (const std::size_t offset : {0, 1, 2, 3}) {
        std::string bytes = written(workload);
        bytes[format::header_size + offset] = 9;
        EXPECT_THROW(read(bytes), std::runtime_error) << "byte " << offset;
    }
}
