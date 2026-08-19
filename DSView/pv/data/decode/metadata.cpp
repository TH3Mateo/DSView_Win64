/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
 *
 * Copyright (C) 2014 DreamSourceLab <support@dreamsourcelab.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include "metadata.h"

#include <algorithm>
#include <new>

using namespace std;

namespace pv {
namespace data {
namespace decode {

MetaData::MetaData(const MetaId &id, const QString &name, const QString &descr) :
    _id(id),
    _name(name),
    _descr(descr),
    _shown(false),
    _interp(MetaInterpStep)
{
}

void MetaData::clear()
{
    std::lock_guard<std::mutex> lock(_mutex);
    _samples.clear();
}

bool MetaData::push_sample(uint64_t start_sample, uint64_t end_sample, double value)
{
    std::lock_guard<std::mutex> lock(_mutex);

    try {
        MetaSample s;
        s.start_sample = start_sample;
        s.end_sample = end_sample;
        s.value = value;
        _samples.push_back(s);
        return true;

    } catch (const std::bad_alloc&) {
        return false;
    }
}

uint64_t MetaData::get_sample_count()
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _samples.size();
}

void MetaData::get_sample_subset(std::vector<MetaSample> &dest,
    uint64_t start_sample, uint64_t end_sample)
{
    std::lock_guard<std::mutex> lock(_mutex);

    // A decoder emits its values in increasing sample order, so _samples is
    // already sorted and can be searched instead of scanned.
    MetaSample key;
    key.start_sample = start_sample;
    key.end_sample = 0;
    key.value = 0;

    auto begin = lower_bound(_samples.begin(), _samples.end(), key,
        [](const MetaSample &a, const MetaSample &b){
            return a.start_sample < b.start_sample;
        });

    // Step back one, so a segment already in progress at the left edge of the
    // view is drawn instead of the row starting blank.
    if (begin != _samples.begin())
        begin--;

    for (auto i = begin; i != _samples.end(); i++)
    {
        if ((*i).start_sample > end_sample)
            break;
        dest.push_back(*i);
    }
}

bool MetaData::get_value_range(const std::vector<MetaSample> &samples,
    double &min_value, double &max_value)
{
    if (samples.empty())
        return false;

    min_value = max_value = samples.front().value;

    for (const MetaSample &s : samples)
    {
        min_value = min(min_value, s.value);
        max_value = max(max_value, s.value);
    }

    return true;
}

} // namespace decode
} // namespace data
} // namespace pv
