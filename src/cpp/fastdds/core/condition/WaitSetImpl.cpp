// Copyright 2021 Proyectos y Sistemas de Mantenimiento SL (eProsima).
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/**
 * @file WaitSetImpl.cpp
 */

#include "WaitSetImpl.hpp"

#include <condition_variable>
#include <mutex>

#include <fastdds/dds/core/condition/Condition.hpp>
#include <fastdds/rtps/common/Time_t.h>
#include <fastrtps/types/TypesBase.h>

#include <fastdds/core/condition/ConditionNotifier.hpp>

using eprosima::fastrtps::types::ReturnCode_t;

namespace eprosima {
namespace fastdds {
namespace dds {
namespace detail {

WaitSetImpl::~WaitSetImpl()
{
    std::lock_guard<std::mutex> guard(mutex_);
    for (const Condition* c : entries_)
    {
        c->get_notifier()->detach_from(this);
    }
}

ReturnCode_t WaitSetImpl::attach_condition(
        const Condition& condition)
{
    bool was_there = false;

    {
        // We only need to protect access to the collection.
        std::lock_guard<std::mutex> guard(mutex_);

        was_there = entries_.remove(&condition);
        entries_.emplace_back(&condition);
    }

    if (!was_there)
    {
        // This is a new condition. Inform the notifier of our interest.
        condition.get_notifier()->attach_to(this);

        {
            // Might happen that a wait changes is_waiting_'s status. Protect it.
            std::lock_guard<std::mutex> guard(mutex_);

            // Should wake_up when adding a new triggered condition
            if (is_waiting_ && condition.get_trigger_value())
            {
                wake_up();
            }
        }
    }

    return ReturnCode_t::RETCODE_OK;
}

ReturnCode_t WaitSetImpl::detach_condition(
        const Condition& condition)
{
    bool was_there = false;

    {
        // We only need to protect access to the collection.
        std::lock_guard<std::mutex> guard(mutex_);
        was_there = entries_.remove(&condition);
    }

    if (was_there)
    {
        // Inform the notifier we are not interested anymore.
        condition.get_notifier()->detach_from(this);
        return ReturnCode_t::RETCODE_OK;
    }

    // Condition not found
    return ReturnCode_t::RETCODE_PRECONDITION_NOT_MET;
}

ReturnCode_t WaitSetImpl::wait(
        ConditionSeq& active_conditions,
        const fastrtps::Duration_t& timeout)
{
    std::unique_lock<std::mutex> lock(mutex_);
    // last notification processed
    unsigned int old_counter = notifications_ - 1;

    if (is_waiting_)
    {
        return ReturnCode_t::RETCODE_PRECONDITION_NOT_MET;
    }

    auto fill_active_conditions = [&]()
            {
                bool ret_val;

                if ( old_counter == notifications_ )
                {
                    // spurious wakeup
                    return false;
                }

                // Loop if predicate may be outdated
                do
                {
                    ret_val = false;
                    old_counter = notifications_;
                    active_conditions.clear();

                    for (const Condition* c : entries_)
                    {
                        if (c->get_trigger_value())
                        {
                            ret_val = true;
                            active_conditions.push_back(const_cast<Condition*>(c));
                        }
                    }
                }
                while (old_counter != notifications_
                && active_conditions.size() != entries_.size());

                return ret_val;
            };

    bool condition_value = false;
    is_waiting_ = true;
    auto missing_notification_outage = std::chrono::milliseconds(500);
    auto now = std::chrono::steady_clock::now();
    auto deadline = fastrtps::c_TimeInfinite == timeout ?
            std::chrono::steady_clock::time_point::max() :
            now + std::chrono::nanoseconds(timeout.to_ns());

    do
    {
        now = std::chrono::steady_clock::now();
        auto next_outage_timeout = now + missing_notification_outage;
        auto ctimeout = std::min(next_outage_timeout, deadline);

        condition_value = cond_.wait_until(lock, ctimeout, fill_active_conditions);
    }
    while (!condition_value && ( old_counter != notifications_ || deadline > now));

    is_waiting_ = false;

    return condition_value ? ReturnCode_t::RETCODE_OK : ReturnCode_t::RETCODE_TIMEOUT;
}

ReturnCode_t WaitSetImpl::get_conditions(
        ConditionSeq& attached_conditions) const
{
    std::lock_guard<std::mutex> guard(mutex_);
    attached_conditions.reserve(entries_.size());
    attached_conditions.clear();
    for (const Condition* c : entries_)
    {
        attached_conditions.push_back(const_cast<Condition*>(c));
    }
    return ReturnCode_t::RETCODE_OK;
}

void WaitSetImpl::wake_up()
{
    ++notifications_;
    cond_.notify_one();
}

void WaitSetImpl::will_be_deleted (
        const Condition& condition)
{
    std::lock_guard<std::mutex> guard(mutex_);
    entries_.remove(&condition);
}

}  // namespace detail
}  // namespace dds
}  // namespace fastdds
}  // namespace eprosima
