#pragma once

// A selected edition may have a different runtime from the library item.
// Zero means unknown; never derive a finish estimate from a formatted title.
static inline unsigned detail_runtime_seconds(unsigned item, unsigned facts, unsigned selected) {
    return selected ? selected : item ? item : facts;
}

// Match the details page's Resume threshold. Completed/stale positions start
// from the beginning, and an unknown runtime stays unknown (no end time).
static inline unsigned detail_remaining_seconds(unsigned runtime, unsigned resume, int start_over) {
    if (!runtime) return 0;
    if (!start_over && resume >= 10 && resume < runtime) return runtime - resume;
    return runtime;
}
