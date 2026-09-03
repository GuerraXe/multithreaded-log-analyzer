#include "support/test_framework.hpp"

#include "io/chunking.hpp"

#include <string>
#include <string_view>
#include <vector>

using namespace la;

namespace {

// Re-join chunks and check the split invariants against `buf`.
void check_covers(std::string_view buf, const std::vector<std::string_view>& chunks) {
    std::string joined;
    for (std::size_t i = 0; i < chunks.size(); ++i) {
        CHECK(!chunks[i].empty()); // no empty chunks
        if (i + 1 < chunks.size()) {
            CHECK(chunks[i].back() == '\n'); // interior chunks end on a line boundary
        }
        joined.append(chunks[i]);
    }
    CHECK_EQ(joined, std::string(buf)); // exact cover, no gap or overlap
}

} // namespace

TEST_CASE("chunking: empty buffer yields no chunks") {
    CHECK_EQ(split_into_chunks("", 4).size(), std::size_t{0});
    CHECK_EQ(split_into_chunks("", 1).size(), std::size_t{0});
}

TEST_CASE("chunking: n<=1 yields a single chunk equal to the whole buffer") {
    const std::string_view buf = "a\nb\nc\n";
    for (std::size_t n : {std::size_t{0}, std::size_t{1}}) {
        const auto c = split_into_chunks(buf, n);
        CHECK_EQ(c.size(), std::size_t{1});
        CHECK_EQ(std::string(c[0]), std::string(buf));
    }
}

TEST_CASE("chunking: split points snap forward to the next newline") {
    const std::string_view buf = "a\nb\nc\nd\n";
    // n=2: the byte target (offset 4) lands mid-line and snaps forward past
    // the newline at offset 5, so chunk 0 gets three lines.
    const auto c2 = split_into_chunks(buf, 2);
    CHECK_EQ(c2.size(), std::size_t{2});
    CHECK_EQ(std::string(c2[0]), std::string("a\nb\nc\n"));
    CHECK_EQ(std::string(c2[1]), std::string("d\n"));

    // n=4: the first target (offset 2) already sits on a newline, so chunk 0
    // is "a\nb\n"; the offset-4 target then equals pos and is skipped, giving
    // an even two-way split.
    const auto c4 = split_into_chunks(buf, 4);
    CHECK_EQ(c4.size(), std::size_t{2});
    CHECK_EQ(std::string(c4[0]), std::string("a\nb\n"));
    CHECK_EQ(std::string(c4[1]), std::string("c\nd\n"));
}

TEST_CASE("chunking: covers the buffer exactly for many n") {
    const std::string_view buf =
        "line one\nline two\nline three\nline four\nline five\nline six\n";
    for (std::size_t n = 1; n <= 16; ++n) {
        const auto c = split_into_chunks(buf, n);
        CHECK(c.size() <= n);
        check_covers(buf, c);
    }
}

TEST_CASE("chunking: a buffer with no newline is a single chunk") {
    const std::string_view buf = "no newlines anywhere in here";
    const auto c = split_into_chunks(buf, 8);
    CHECK_EQ(c.size(), std::size_t{1});
    CHECK_EQ(std::string(c[0]), std::string(buf));
}

TEST_CASE("chunking: one long line among short ones is not split") {
    const std::string_view buf =
        "x\nAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\ny\n";
    const auto c = split_into_chunks(buf, 3);
    check_covers(buf, c);
    // the 58-char line must appear intact in exactly one chunk
    int hits = 0;
    for (const auto& ch : c) {
        if (ch.find(std::string(58, 'A')) != std::string_view::npos) ++hits;
    }
    CHECK_EQ(hits, 1);
}

TEST_CASE("chunking: no trailing newline; last chunk need not end with newline") {
    const std::string_view buf = "a\nb\nc\nd";
    const auto c = split_into_chunks(buf, 2);
    check_covers(buf, c);
    CHECK(c.back().back() != '\n');
}

TEST_CASE("chunking: n greater than line count returns at most line-count chunks") {
    const std::string_view buf = "a\nb\nc\n";
    const auto c = split_into_chunks(buf, 100);
    CHECK(c.size() <= std::size_t{3});
    check_covers(buf, c);
}
