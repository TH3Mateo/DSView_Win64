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

#ifndef DSVIEW_PV_DATA_DECODE_METADATA_H
#define DSVIEW_PV_DATA_DECODE_METADATA_H

#include <libsigrokdecode.h>
#include <stdint.h>
#include <vector>
#include <mutex>
#include <QString>

namespace pv {
namespace data {
namespace decode {

// How the gaps between two consecutive meta samples are drawn.
enum MetaInterp {
    MetaInterpStep = 0,   // hold the value until the next sample
    MetaInterpLinear      // straight line between the sample midpoints
};

/**
 * Identifies one SRD_OUTPUT_META stream.
 *
 * The srd_decoder class pointer comes from the global decoder registry and
 * stays valid for the whole application lifetime. The srd_decoder_inst that
 * actually emitted the value must NOT be used as a key: srd_session_destroy()
 * frees every instance at the end of each decode run.
 */
struct MetaId
{
    const srd_decoder *decoder;
    int pdo_id;

    bool operator<(const MetaId &other) const
    {
        if (decoder != other.decoder)
            return decoder < other.decoder;
        return pdo_id < other.pdo_id;
    }
};

struct MetaSample
{
    uint64_t start_sample;
    uint64_t end_sample;
    double   value;
};

/**
 * One numeric measurement stream produced by a decoder, e.g. the per-cycle
 * duty cycle of the PWM decoder.
 *
 * Unlike AnalogSnapshot or MathStack the samples are sparse and unevenly
 * spaced: a decoder emits one value per protocol event, not one per sample.
 * Each value carries the sample span it was measured over.
 */
class MetaData
{
public:
    MetaData(const MetaId &id, const QString &name, const QString &descr);

    inline const MetaId& id() const{
        return _id;
    }

    // Label registered by the decoder, e.g. "Duty cycle".
    inline const QString& name() const{
        return _name;
    }

    inline const QString& descr() const{
        return _descr;
    }

    // Whether this stream is drawn as a waveform row. Survives a re-decode,
    // so the user's choice is not lost when the capture is decoded again.
    inline bool shown() const{
        return _shown;
    }

    inline void set_shown(bool shown){
        _shown = shown;
    }

    inline MetaInterp interp() const{
        return _interp;
    }

    inline void set_interp(MetaInterp interp){
        _interp = interp;
    }

    // Drops the samples but keeps the display settings.
    void clear();

    bool push_sample(uint64_t start_sample, uint64_t end_sample, double value);

    uint64_t get_sample_count();

    /**
     * Collects the samples overlapping [start_sample, end_sample].
     *
     * The last sample starting before the window is included as well, so that
     * a segment entering the view from the left edge is still drawn.
     */
    void get_sample_subset(std::vector<MetaSample> &dest,
        uint64_t start_sample, uint64_t end_sample);

    /**
     * Value range of the given samples, used to auto-fit the vertical axis.
     * Returns false if there is nothing to scale to.
     */
    static bool get_value_range(const std::vector<MetaSample> &samples,
        double &min_value, double &max_value);

private:
    MetaId      _id;
    QString     _name;
    QString     _descr;
    bool        _shown;
    MetaInterp  _interp;

    std::vector<MetaSample> _samples;
    mutable std::mutex      _mutex;
};

} // namespace decode
} // namespace data
} // namespace pv

#endif // DSVIEW_PV_DATA_DECODE_METADATA_H
