#include "filter/record_filter.hpp"

#include <string_view>
#include <utility>

namespace la {

RecordFilter::RecordFilter(FilterSpec spec) : spec_(std::move(spec)) {
    if (!spec_.level_only.empty()) {
        use_level_mask_ = true;
        for (const Level lv : spec_.level_only) {
            level_mask_[level_index(lv)] = true;
        }
    }
}

bool RecordFilter::matches(const LogRecord& r) const {
    if (spec_.from_ms && r.epoch_ms < *spec_.from_ms) return false;
    if (spec_.to_ms && r.epoch_ms >= *spec_.to_ms) return false;

    if (use_level_mask_) {
        if (!level_mask_[level_index(r.level)]) return false;
    } else if (spec_.min_level) {
        if (level_index(r.level) < level_index(*spec_.min_level)) return false;
    }

    if (!spec_.services.empty()) {
        bool hit = false;
        for (const std::string& s : spec_.services) {
            if (r.service == s) {
                hit = true;
                break;
            }
        }
        if (!hit) return false;
    }

    if (!spec_.status_classes.empty()) {
        if (r.status == 0) return false;
        const int cls = r.status / 100;
        bool hit = false;
        for (const int c : spec_.status_classes) {
            if (c == cls) {
                hit = true;
                break;
            }
        }
        if (!hit) return false;
    }

    if (spec_.path_prefix) {
        const std::string& p = *spec_.path_prefix;
        if (r.path.size() < p.size() || r.path.substr(0, p.size()) != p) return false;
    }

    if (spec_.path_contains) {
        if (r.path.find(*spec_.path_contains) == std::string_view::npos) return false;
    }

    return true;
}

bool RecordFilter::is_pass_through() const {
    return !spec_.from_ms && !spec_.to_ms && !spec_.min_level &&
           spec_.level_only.empty() && spec_.services.empty() &&
           spec_.status_classes.empty() && !spec_.path_prefix && !spec_.path_contains;
}

} // namespace la
