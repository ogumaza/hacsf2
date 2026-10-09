// SPDX-License-Identifier: MIT

// Sound format readers for nn::atk, the Switch's sound library: BFSAR archives and their FSEQ, FBNK, FWAR, FWAV and
// FGRP files. They're the Wii U's formats. Those are the 3DS's CTR formats with new names, laid out the same way. Each
// file is in the byte order that its byte order mark gives: little-endian on the Switch. Layouts were checked against
// FSAR 2.2.0 and 2.5.0 archives.

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace hacsf2
{

enum class WaveEncoding : uint8_t
{
    kPcm8 = 0,
    kPcm16 = 1,
    kDspAdpcm = 2
};

// DSP-ADPCM predictor coefficients (8 pairs) and initial sample history.
struct DspAdpcmInfo
{
    std::array<int16_t, 16> coefs{};
    int16_t yn1 = 0, yn2 = 0;
};

// One wave channel's sample data and initial decoder state.
struct WaveChannel
{
    std::span<const uint8_t> data; // sample data from the channel's start to the end of DATA
    std::optional<DspAdpcmInfo> adpcm;
};

// FWAV encoding, byte order, sample rate, loop bounds and channels.
struct Wave
{
    static Wave Parse(std::span<const uint8_t> file);

    WaveEncoding encoding = WaveEncoding::kPcm16;
    bool big_endian = false; // the byte order of PCM16 samples
    bool loop = false;
    uint32_t sample_rate = 32000;
    uint32_t loop_start = 0;
    uint32_t loop_end = 0; // sample count; exclusive end of playback for both looping and one-shot waves
    std::vector<WaveChannel> channels;
};

// FWAV files in a wave archive (FWAR).
struct WaveArchive
{
    static WaveArchive Parse(std::span<const uint8_t> file);

    std::vector<std::span<const uint8_t>> waves;
};

// A region's envelope as MML values: attack, decay, sustain, hold and release.
struct Adshr
{
    // Default when a region has no ADSHR: five 127s. nn::atk's BankFile sets them up when it starts.
    uint8_t attack = 127, decay = 127, sustain = 127, hold = 127, release = 127;
};

// Wave and playback settings for a key/velocity region.
struct VelocityRegion
{
    uint32_t wave_id_index = 0;
    uint8_t original_key = 60;
    uint8_t volume = 127;
    uint8_t pan = 64;
    float pitch = 1.0f;
    bool ignore_note_off = false;
    uint8_t key_group = 0;
    Adshr adshr;
};

// A key range, subdivided into velocity ranges.
struct KeyRegion
{
    // A velocity range and its optional region. A velocity can pass 127, and a direct table's range has no upper bound.
    struct Vel
    {
        int lo = 0, hi = 127;
        std::optional<VelocityRegion> region;
    };

    uint8_t lo = 0, hi = 127;
    std::vector<Vel> velocities;
};

// A bank instrument's key regions.
struct Instrument
{
    // Finds the velocity region for a key and velocity: the first key region that holds the key, and the first of its
    // velocity regions that holds the velocity.
    const VelocityRegion* Find(int key, int velocity) const;

    std::vector<KeyRegion> keys;
};

// A bank wave reference: wave archive ID and index within it.
struct WaveId
{
    uint32_t wave_archive_item = 0; // 0x05xxxxxx item id
    uint32_t index = 0;
};

// FBNK instruments and wave references.
struct Bank
{
    static Bank Parse(std::span<const uint8_t> file);

    std::vector<WaveId> waves;
    std::vector<std::optional<Instrument>> instruments;
};

// FSEQ bytecode.
struct Sequence
{
    static Sequence Parse(std::span<const uint8_t> file);

    std::span<const uint8_t> data; // DATA block payload
    bool big_endian = false;       // the byte order of the commands' 16- and 24-bit arguments: the file's
};

enum class SoundType
{
    kSequence,
    kStream,
    kWaveSound,
    kUnknown
};

// Sound metadata. Missing names are generated from type and index, e.g. SEQ_12. As in other item names, control
// characters, line breaks and invalid UTF-8 bytes become '?'.
struct SoundInfo
{
    std::string name;
    uint32_t file_id = 0;
    uint8_t volume = 127;
    uint8_t pan_mode = 0;  // nn::atk mode: 0 dual (pan stereo channels separately), 1 balance
    uint8_t pan_curve = 0; // nn::atk curve: 0 is the default square-root law
    SoundType type = SoundType::kUnknown;

    // Sequence sounds only.
    std::vector<uint32_t> banks; // up to four bank item IDs (0x03xxxxxx), or 0xFFFFFFFF for an empty slot
    uint32_t allocate_track_flags = 1;
    uint32_t start_offset = 0;
};

// Bank name and file ID. Unnamed banks use BANK_<index>.
struct BankInfo
{
    std::string name;
    uint32_t file_id = 0;
};

// Wave archive name and file ID. Unnamed wave archives use WARC_<index>.
struct WaveArchiveInfo
{
    std::string name;
    uint32_t file_id = 0;
};

class Reader;

// FSAR sound, bank and wave archive tables. Files may be stored directly in the archive, in FGRP groups inside it, or
// in group files that the game keeps beside it.
class SoundArchive
{
public:
    // Read archive tables and locate files stored in groups. Throw FormatError if the tables cannot be read.
    static SoundArchive Load(std::vector<uint8_t> bytes);

    const std::vector<SoundInfo>& Sounds() const
    {
        return sounds_;
    }

    const std::vector<BankInfo>& Banks() const
    {
        return banks_;
    }

    const std::vector<WaveArchiveInfo>& WaveArchives() const
    {
        return wave_archives_;
    }

    // The names of the groups that the archive lists with no file: the game keeps them outside it and loads them
    // itself. Unnamed groups use GROUP_<index>.
    const std::vector<std::string>& ExternalGroups() const
    {
        return external_groups_;
    }

    // Add an external group's file (see ExternalGroups). The files it holds that the archive lacks can be read with
    // FileData afterwards. Throw FormatError if `bytes` isn't a group file.
    void AddGroupFile(std::vector<uint8_t> bytes);

    // Return file contents, or an empty span for external files and files absent from all readable groups.
    std::span<const uint8_t> FileData(uint32_t file_id) const;

    // True if any part of the file extends past the archive. For files stored only in groups, true if no readable group
    // contains the file and some groups extend past the archive.
    bool FileMissing(uint32_t file_id) const
    {
        return file_id < files_.size() && files_[file_id].missing;
    }

    // True if the archive is shorter than the size in its header.
    bool Truncated() const
    {
        return truncated_;
    }

    // True if the archive's name table is damaged. Its sounds and banks then go by number.
    bool NamesDamaged() const
    {
        return names_damaged_;
    }

    // Errors reading groups. Files stored only in unreadable groups are unavailable.
    const std::vector<std::string>& GroupErrors() const
    {
        return group_errors_;
    }

private:
    // A file's location within the archive or a group file added to it.
    struct FileEntry
    {
        std::size_t offset = 0;
        uint32_t size = 0;
        uint32_t source = 0;   // 0 for the archive, n for the nth group file added
        bool internal = false; // stored in the FILE block, directly or in a group, or in a group file added
        bool missing = false;  // lies past the end of the archive
        bool grouped = false;  // no location in the file table; may be stored in groups
    };

    // Read tables through `r` and locate files stored in groups. Throw FormatError if the tables cannot be read.
    void ReadTables(const Reader& r);

    // Locate files stored only in groups using FGRP group `name`. Keep locations already found in earlier groups.
    // `group` is the FGRP file's table entry. Append truncated files to `cut_short` with the group name, and the error
    // to `unreadable` if the group can't be read.
    void ReadGroup(const std::string& name, const FileEntry& group,
                   std::vector<std::pair<std::string, uint32_t>>& cut_short, std::vector<std::string>& unreadable);

    std::vector<uint8_t> bytes_;
    std::vector<std::vector<uint8_t>> group_files_;
    std::vector<SoundInfo> sounds_;
    std::vector<BankInfo> banks_;
    std::vector<WaveArchiveInfo> wave_archives_;
    std::vector<std::string> external_groups_;
    std::vector<FileEntry> files_;
    bool truncated_ = false;
    bool names_damaged_ = false;
    bool groups_cut_ = false; // some groups lie past the end of the archive
    std::vector<std::string> group_errors_;
};

} // namespace hacsf2
