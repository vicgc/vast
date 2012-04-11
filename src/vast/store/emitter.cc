#include "vast/store/emitter.h"

#include <ze/event.h>
#include "vast/store/exception.h"
#include "vast/store/segment.h"
#include "vast/util/logger.h"

namespace vast {
namespace store {

emitter::emitter(ze::component& c,
                 std::shared_ptr<segment_cache> cache,
                 std::vector<ze::uuid> ids)
  : ze::core_source<ze::event>(c)
  , cache_(cache)
  , ids_(std::move(ids))
  , current_(ids_.begin())
{
}

void emitter::start()
{
    paused_ = false;
    io_.service().post(std::bind(&emitter::emit, shared_from_this()));
}

void emitter::pause()
{
    paused_ = true;
}

void emitter::emit()
{
    if (paused_ || current_ == ids_.end())
        return;

    try
    {
        auto id = *current_++;
        std::shared_ptr<isegment> segment = cache_->retrieve(id);
        LOG(verbose, store) << "emitting segment " << id;
        segment->get([&](ze::event_ptr&& e) { send(e); });
    }
    catch (segment_exception const& e)
    {
        LOG(error, store) << e.what();
    }
    catch (ze::serialization::exception const& e)
    {
        LOG(error, store) << e.what();
    }

    io_.service().post(std::bind(&emitter::emit, shared_from_this()));
}

} // namespace store
} // namespace vast