#include "report/json_renderer.hpp"

#include "core/version.hpp"
#include "parse/log_record.hpp"
#include "parse/timestamp.hpp"

#include <cstdio>
#include <ostream>
#include <string>
#include <vector>

namespace la {
namespace {

// Minimal streaming JSON writer: tracks indentation and, per open container,
// whether a child has been written yet (so commas and empty-container
// collapsing are correct). Deliberately tiny; the output shape is fixed by
// render_json below. Output is 2-space-indented, LF-terminated, ASCII.
class JsonWriter {
public:
    explicit JsonWriter(std::ostream& os) : os_(os) {}

    void begin_object() { open('{'); }
    void begin_object(std::string_view key) {
        key_(key);
        open('{');
    }
    void end_object() { close('}'); }

    void begin_array(std::string_view key) {
        key_(key);
        open('[');
    }
    void end_array() { close(']'); }

    // Start an object as an array element.
    void element_object() {
        separator_();
        os_ << '{';
        push();
    }

    void field(std::string_view key, std::uint64_t v) {
        key_(key);
        os_ << v;
    }
    void field(std::string_view key, std::int64_t v) {
        key_(key);
        os_ << v;
    }
    void field(std::string_view key, int v) { field(key, static_cast<std::int64_t>(v)); }
    void field(std::string_view key, std::string_view v) {
        key_(key);
        os_ << json_quote(v);
    }
    void field_raw(std::string_view key, std::string_view raw) {
        key_(key);
        os_ << raw;
    }
    void field_latency(std::string_view key, std::int64_t ms) {
        key_(key);
        os_ << ((ms == kNoValueMs || ms == kOverflowMs) ? std::string("null")
                                                        : std::to_string(ms));
    }
    void field_fixed(std::string_view key, double v) {
        key_(key);
        char buf[64];
        std::snprintf(buf, sizeof buf, "%.4f", v);
        os_ << buf;
    }

private:
    struct Frame {
        bool has_child = false;
    };

    void indent() {
        for (std::size_t i = 0; i < stack_.size(); ++i) os_ << "  ";
    }
    // Emit ",\n<indent>" or "\n<indent>" depending on whether the current
    // container already has a child; mark that it now does.
    void separator_() {
        if (!stack_.empty()) {
            if (stack_.back().has_child) os_ << ',';
            stack_.back().has_child = true;
        }
        os_ << '\n';
        indent();
    }
    void key_(std::string_view key) {
        separator_();
        os_ << json_quote(key) << ": ";
    }
    void open(char c) {
        os_ << c;
        push();
    }
    void close(char c) {
        const bool had_child = !stack_.empty() && stack_.back().has_child;
        if (!stack_.empty()) stack_.pop_back();
        if (had_child) {
            os_ << '\n';
            indent();
        }
        os_ << c;
    }
    void push() { stack_.push_back(Frame{}); }

    std::ostream& os_;
    std::vector<Frame> stack_;
};

void write_endpoint(JsonWriter& w, const EndpointRow& e) {
    w.element_object();
    w.field("endpoint", e.endpoint);
    w.field("count", e.count);
    w.field("timed", e.timed);
    w.field_fixed("mean_ms", e.mean_ms);
    w.field_fixed("stddev_ms", e.stddev_ms);
    w.field_latency("min_ms", e.min_ms);
    w.field_latency("max_ms", e.max_ms);
    w.field_latency("p50_ms", e.p50_ms);
    w.field_latency("p90_ms", e.p90_ms);
    w.field_latency("p99_ms", e.p99_ms);
    w.end_object();
}

} // namespace

std::string json_quote(std::string_view s) {
    std::string out;
    out.reserve(s.size() + 2);
    out.push_back('"');
    for (const unsigned char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof buf, "\\u%04x", c);
                    out += buf;
                } else {
                    out.push_back(static_cast<char>(c));
                }
        }
    }
    out.push_back('"');
    return out;
}

void render_json(const Report& r, std::ostream& os) {
    JsonWriter w(os);
    w.begin_object();

    w.field("tool", version_string());
    w.field("top_n", r.top_n);
    w.field_raw("exact_percentiles", r.exact_percentiles ? "true" : "false");

    w.begin_object("input");
    w.field("bytes", r.bytes);
    w.field("lines", r.total_lines);
    w.field("records", r.records);
    w.field("kept", r.kept);
    w.field("malformed", r.malformed);
    w.field("blank", r.blank);
    w.end_object();

    w.begin_object("severity");
    for (int i = 0; i < kLevelCount; ++i) {
        w.field(to_string(static_cast<Level>(i)), r.by_level[static_cast<std::size_t>(i)]);
    }
    w.field("errors", r.errors);
    w.field("warnings", r.warnings);
    w.end_object();

    w.begin_object("status_classes");
    for (int cls = 1; cls <= 5; ++cls) {
        w.field(std::to_string(cls) + "xx", r.by_status_class[static_cast<std::size_t>(cls)]);
    }
    w.end_object();

    auto code_array = [&](std::string_view key, const std::vector<CountRow>& rows) {
        w.begin_array(key);
        for (const auto& c : rows) {
            w.element_object();
            w.field_raw("code", c.key); // key is already a decimal integer string
            w.field("count", c.value);
            w.end_object();
        }
        w.end_array();
    };
    code_array("response_codes", r.status_codes);
    code_array("top_status_codes", r.top_status_codes);

    auto count_array = [&](std::string_view key, std::string_view label,
                           const std::vector<CountRow>& rows) {
        w.begin_array(key);
        for (const auto& c : rows) {
            w.element_object();
            w.field(label, c.key);
            w.field("count", c.value);
            w.end_object();
        }
        w.end_array();
    };
    count_array("top_services", "service", r.top_services);
    count_array("top_errors", "message", r.top_errors);
    count_array("failures_by_service", "service", r.top_failure_services);

    w.begin_array("busiest_endpoints");
    for (const auto& e : r.busiest_endpoints) write_endpoint(w, e);
    w.end_array();

    w.begin_array("slowest_endpoints");
    for (const auto& e : r.slowest_endpoints) write_endpoint(w, e);
    w.end_array();

    w.begin_object("timeline");
    w.field("interval_ms", r.interval_ms);
    w.begin_array("buckets");
    for (const auto& t : r.timeline) {
        w.element_object();
        w.field("start", format_timestamp(t.bucket_start_ms));
        w.field("requests", t.requests);
        w.field("errors", t.errors);
        w.end_object();
    }
    w.end_array();
    w.end_object();

    w.begin_array("malformed_samples");
    for (const auto& m : r.malformed_samples) {
        w.element_object();
        w.field("line", m.line);
        w.field("reason", m.reason);
        w.field("text", m.text);
        w.end_object();
    }
    w.end_array();

    w.end_object();
    os << '\n';
}

} // namespace la
