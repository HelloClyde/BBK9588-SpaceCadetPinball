/* Original WAV playback adapter for the 9588 port. No SDL or audio device I/O.
 * The original engine's Sound interface and oldest-channel arbitration are
 * retained. PCM is supplied to the platform in 22050 Hz signed-16 mono blocks. */
#include "engine/pch.h"
#include "engine/Sound.h"

namespace {
constexpr unsigned MaxVoices = 8;
constexpr unsigned OutputRate = 22050;
constexpr unsigned MaxWaveBytes = 16u * 1024u * 1024u;
struct Voice {
    Mix_Chunk* wave;
    unsigned cursor;
    unsigned fraction;
    unsigned step;
};
Voice voices[MaxVoices]{};
bool output_active = true;
bool output_enabled = false;
bool output_open = false;
int output_volume = 128;
unsigned output_channels = MaxVoices;

unsigned little16(const unsigned char* bytes) {
    return unsigned(bytes[0]) | (unsigned(bytes[1]) << 8);
}
unsigned little32(const unsigned char* bytes) {
    return little16(bytes) | (little16(bytes + 2) << 16);
}
bool tag(const unsigned char* bytes, const char* text) {
    return bytes[0] == text[0] && bytes[1] == text[1] &&
        bytes[2] == text[2] && bytes[3] == text[3];
}
void stop_voices() {
    for (unsigned i = 0; i < MaxVoices; ++i) voices[i].wave = nullptr;
}
int sample(const Mix_Chunk* wave, unsigned frame) {
    const unsigned stride = wave->channels * (wave->bits / 8u);
    const unsigned char* bytes = wave->samples + frame * stride;
    int value = 0;
    for (unsigned channel = 0; channel < wave->channels; ++channel) {
        if (wave->bits == 8)
            value += (int(bytes[channel]) - 128) * 256;
        else {
            unsigned word = little16(bytes + channel * 2);
            value += word < 32768u ? int(word) : int(word) - 65536;
        }
    }
    return value / wave->channels;
}
}

int Sound::num_channels = MaxVoices;
bool Sound::enabled_flag = false;
std::vector<ChannelInfo> Sound::Channels{};
int Sound::Volume = 128;
bool Sound::MixOpen = false;

void Sound::Init(bool mixOpen, int channels, bool enableFlag, int volume) {
    stop_voices();
    MixOpen = output_open = mixOpen;
    output_active = true;
    SetChannels(channels);
    SetVolume(volume);
    Enable(enableFlag);
}
void Sound::Enable(bool enableFlag) {
    enabled_flag = output_enabled = enableFlag;
    if (!enableFlag) stop_voices();
}
void Sound::Activate() { output_active = true; }
void Sound::Deactivate() { output_active = false; }
void Sound::Close() {
    Enable(false);
    Channels.clear();
    MixOpen = output_open = false;
}
void Sound::SetChannels(int channels) {
    if (channels <= 0) channels = MaxVoices;
    if (channels > int(MaxVoices)) channels = MaxVoices;
    num_channels = channels;
    output_channels = unsigned(channels);
    Channels.resize(output_channels);
    for (unsigned i = output_channels; i < MaxVoices; ++i) voices[i].wave = nullptr;
}
void Sound::SetVolume(int volume) {
    if (volume < 0) volume = 0;
    if (volume > 128) volume = 128;
    Volume = output_volume = volume;
}
void Sound::PlaySound(Mix_Chunk* wave, int time, TPinballComponent* source, const char* info) {
    (void)source;
    (void)info;
    if (!MixOpen || !enabled_flag || !wave || !wave->samples || !wave->frames) return;
    unsigned channel = 0;
    for (; channel < output_channels; ++channel) if (!voices[channel].wave) break;
    if (channel == output_channels) {
        /* SDL's original interface steals the voice with the oldest timestamp. */
        channel = 0;
        for (unsigned i = 1; i < output_channels; ++i)
            if (Channels[i].TimeStamp < Channels[channel].TimeStamp) channel = i;
    }
    Voice& voice = voices[channel];
    voice.wave = wave;
    voice.cursor = voice.fraction = 0;
    voice.step = wave->rate * 65536u / OutputRate;
    Channels[channel].TimeStamp = time;
    /* Hardware output is mono. Avoid introducing floating-point work solely
     * for stereo pan metadata which cannot be heard on this output path. */
    Channels[channel].Position = {0.5f, 1.0f};
}

Mix_Chunk* Sound::LoadWaveFile(const std::string& path) {
    if (!MixOpen) return nullptr;
    FILE* file = fopenu(path.c_str(), "rb");
    if (!file) return nullptr;
    unsigned char header[16];
    unsigned format = 0, channels = 0, rate = 0, bits = 0, alignment = 0;
    unsigned data_offset = 0, data_bytes = 0;
    Mix_Chunk* wave = nullptr;
    if (fread(header, 1, 12, file) != 12 || !tag(header, "RIFF") || !tag(header + 8, "WAVE")) {
        fclose(file);
        return nullptr;
    }
    const unsigned declared_size = little32(header + 4);
    if (declared_size < 4 || declared_size > MaxWaveBytes + 65536u ||
        fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return nullptr;
    }
    const long actual_size = ftell(file);
    const unsigned end = declared_size + 8u;
    if (actual_size < 0 || unsigned(actual_size) < end) {
        fclose(file);
        return nullptr;
    }
    unsigned position = 12;
    while (position <= end - 8u) {
        if (fseek(file, long(position), SEEK_SET) != 0 || fread(header, 1, 8, file) != 8) break;
        const unsigned size = little32(header + 4);
        const unsigned content = position + 8u;
        if (size > end - content) break;
        if (tag(header, "fmt ") && size >= 16) {
            if (fread(header, 1, 16, file) != 16) break;
            format = little16(header);
            channels = little16(header + 2);
            rate = little32(header + 4);
            alignment = little16(header + 12);
            bits = little16(header + 14);
        } else if (tag(header, "data") && !data_offset) {
            data_offset = content;
            data_bytes = size;
        }
        if (size == end - content) break;
        position = content + size + (size & 1u);
    }
    if (format == 1 && (channels == 1 || channels == 2) &&
        (bits == 8 || bits == 16) && rate >= 4000 && rate <= 48000 &&
        alignment == channels * (bits / 8u) && data_offset &&
        data_bytes && data_bytes <= MaxWaveBytes && data_bytes % alignment == 0) {
        wave = static_cast<Mix_Chunk*>(calloc(1, sizeof(Mix_Chunk)));
        if (wave) {
            wave->samples = static_cast<unsigned char*>(malloc(data_bytes));
            if (!wave->samples || fseek(file, long(data_offset), SEEK_SET) != 0 ||
                fread(wave->samples, 1, data_bytes, file) != data_bytes) {
                free(wave->samples);
                free(wave);
                wave = nullptr;
            } else {
                wave->frames = data_bytes / alignment;
                wave->rate = rate;
                wave->channels = static_cast<unsigned short>(channels);
                wave->bits = static_cast<unsigned short>(bits);
            }
        }
    }
    fclose(file);
    return wave;
}

void Sound::FreeSound(Mix_Chunk* wave) {
    if (!wave) return;
    for (unsigned i = 0; i < MaxVoices; ++i)
        if (voices[i].wave == wave) voices[i].wave = nullptr;
    free(wave->samples);
    free(wave);
}

extern "C" unsigned scp_audio_active_voices() {
    unsigned active = 0;
    for (unsigned i = 0; i < output_channels; ++i) if (voices[i].wave) ++active;
    return active;
}
extern "C" void scp_audio_mix(short* output, unsigned frames) {
    if (!output) return;
    for (unsigned frame = 0; frame < frames; ++frame) {
        int mixed = 0;
        if (output_open && output_enabled && output_active) {
            for (unsigned channel = 0; channel < output_channels; ++channel) {
                Voice& voice = voices[channel];
                if (!voice.wave) continue;
                const Mix_Chunk* wave = voice.wave;
                int first = sample(wave, voice.cursor);
                int second = voice.cursor + 1 < wave->frames ? sample(wave, voice.cursor + 1) : first;
                mixed += (first * int(65536u - voice.fraction) + second * int(voice.fraction)) / 65536;
                voice.fraction += voice.step;
                voice.cursor += voice.fraction >> 16;
                voice.fraction &= 65535u;
                if (voice.cursor >= wave->frames) voice.wave = nullptr;
            }
            mixed = mixed * output_volume / 128;
        }
        if (mixed > 32767) mixed = 32767;
        if (mixed < -32768) mixed = -32768;
        output[frame] = static_cast<short>(mixed);
    }
}
