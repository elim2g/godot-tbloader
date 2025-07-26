#ifndef SCOPED_TIMER_H
#define SCOPED_TIMER_H

#include <godot_cpp/core/object.hpp>
#include <godot_cpp/variant/string.hpp>

#define SCOPED_TIMER(x) turnt::ScopedTimer scoped_timer_##x( #x )

namespace turnt
{

struct ScopeMarker
{
    const char* name;
    uint64_t start_time;
    uint64_t end_time;
    uint64_t depth;
};

class ScopedTimer
{
public:
    ScopedTimer(const char* in_scope_name);
    ~ScopedTimer();

private:
    ScopeMarker* _marker = nullptr;

private:
    static uint64_t stack_depth;
    static godot::Vector<ScopeMarker*> scope_markers;
};

} // namespace turnt

#endif // SCOPED_TIMER_H