#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include "scoped_timer.h"

using namespace turnt;

/*static*/ uint64_t ScopedTimer::stack_depth = 0;
/*static*/ godot::Vector<ScopeMarker*> ScopedTimer::scope_markers;

ScopedTimer::ScopedTimer(godot::String in_scope_name)
{
    _marker = new ScopeMarker 
    { 
        /* name */          in_scope_name, 
        /* start_time */    godot::Time::get_singleton()->get_ticks_msec(), 
        /* end_time */      0u, 
        /* depth */         ScopedTimer::stack_depth 
    };
    ScopedTimer::scope_markers.append(_marker);
    ++ScopedTimer::stack_depth;
}

ScopedTimer::~ScopedTimer()
{
    _marker->end_time = godot::Time::get_singleton()->get_ticks_msec();
    --ScopedTimer::stack_depth;

    if (unlikely(ScopedTimer::stack_depth <= 0))
    {
        godot::String output("----------\n");
        for (const ScopeMarker* sm : ScopedTimer::scope_markers)
        {
            output += godot::String("-").repeat(sm->depth);
            output += godot::String("[" + sm->name + "] ");
            output += godot::String::num_uint64(sm->end_time - sm->start_time) + "ms";
            output += '\n';
        }
        output += "----------";

        godot::UtilityFunctions::print(output);

        for (ScopeMarker* sm : ScopedTimer::scope_markers)
        {
            delete sm;
        }
        ScopedTimer::scope_markers.clear();
    }
}