#include "audio_capture.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <thread>

#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>

AudioCapture::AudioCapture()
    : m_sample_rate(44100)
    , m_fft_size(1024)
    , m_ring_write(0)
    , m_ring_read(0)
    , m_pw_stream(nullptr)
    , m_pw_main_loop(nullptr)
    , m_pw_thread(nullptr)
    , m_running(false)
    , m_fft_window(nullptr)
    , m_fft_output(nullptr)
    , m_waveform(nullptr)
    , m_waveform_len(0)
    , m_bass(0.0f), m_mid(0.0f), m_treb(0.0f)
    , m_bass_att(0.0f), m_mid_att(0.0f), m_treb_att(0.0f)
{
    memset(m_ring, 0, sizeof(m_ring));
}

AudioCapture::~AudioCapture()
{
    Shutdown();
}

void on_process(void* userdata)
{
    AudioCapture* ac = static_cast<AudioCapture*>(userdata);
    if (!ac->m_pw_stream) return;

    pw_stream* stream = static_cast<pw_stream*>(ac->m_pw_stream);
    pw_buffer* b = pw_stream_dequeue_buffer(stream);
    if (!b) return;

    spa_buffer* buf = b->buffer;
    if (!buf || !buf->datas[0].data || buf->datas[0].chunk->size == 0)
    {
        pw_stream_queue_buffer(stream, b);
        return;
    }

    float* samples = static_cast<float*>(buf->datas[0].data);
    int size = buf->datas[0].chunk->size;
    int nframes = size / (2 * sizeof(float));

    int write_pos = ac->m_ring_write.load(std::memory_order_relaxed);
    float* ring = ac->m_ring;
    int RING_SIZE = ac->RING_SIZE;

    for (int i = 0; i < nframes && i < RING_SIZE; i++)
    {
        float mono = (samples[i * 2] + samples[i * 2 + 1]) * 0.5f;
        ring[write_pos] = mono;
        write_pos = (write_pos + 1) % RING_SIZE;
    }
    ac->m_ring_write.store(write_pos, std::memory_order_release);

    pw_stream_queue_buffer(stream, b);
}

void pw_thread_fn(AudioCapture* ac)
{
    pw_init(nullptr, nullptr);

    pw_main_loop* loop = pw_main_loop_new(nullptr);
    if (!loop) {
        fprintf(stderr, "AudioCapture: failed to create main loop\n");
        pw_deinit();
        return;
    }

    pw_context* context = pw_context_new(pw_main_loop_get_loop(loop), nullptr, 0);
    if (!context) {
        fprintf(stderr, "AudioCapture: failed to create context\n");
        pw_main_loop_destroy(loop);
        pw_deinit();
        return;
    }

    pw_core* core = pw_context_connect(context, nullptr, 0);
    if (!core) {
        fprintf(stderr, "AudioCapture: failed to connect (PipeWire not running?)\n");
        pw_context_destroy(context);
        pw_main_loop_destroy(loop);
        pw_deinit();
        return;
    }

    pw_properties* props = pw_properties_new(
        PW_KEY_MEDIA_TYPE, "Audio",
        PW_KEY_MEDIA_CATEGORY, "Capture",
        PW_KEY_MEDIA_ROLE, "Music",
        NULL);

    pw_stream* stream = pw_stream_new(core, "MilkDrop3 Capture", props);
    if (!stream) {
        fprintf(stderr, "AudioCapture: failed to create stream\n");
        pw_core_disconnect(core);
        pw_context_destroy(context);
        pw_main_loop_destroy(loop);
        pw_deinit();
        return;
    }

    spa_hook stream_listener = {};
    pw_stream_events events = {};
    events.version = PW_VERSION_STREAM_EVENTS;
    events.process = on_process;
    pw_stream_add_listener(stream, &stream_listener, &events, ac);

    uint8_t pod_buf[1024];
    spa_pod_builder bld = SPA_POD_BUILDER_INIT(pod_buf, sizeof(pod_buf));

    struct spa_audio_info_raw info;
    memset(&info, 0, sizeof(info));
    info.format = SPA_AUDIO_FORMAT_F32;
    info.rate = 44100;
    info.channels = 2;

    const spa_pod* params[1];
    params[0] = spa_format_audio_raw_build(&bld, SPA_PARAM_EnumFormat, &info);

    if (pw_stream_connect(stream, PW_DIRECTION_INPUT, PW_ID_ANY,
                         static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT |
                                                      PW_STREAM_FLAG_MAP_BUFFERS),
                         params, 1) < 0) {
        fprintf(stderr, "AudioCapture: failed to connect stream\n");
        pw_stream_destroy(stream);
        pw_core_disconnect(core);
        pw_context_destroy(context);
        pw_main_loop_destroy(loop);
        pw_deinit();
        return;
    }

    ac->m_pw_stream = stream;
    ac->m_pw_main_loop = loop;
    ac->m_running.store(true, std::memory_order_release);

    pw_main_loop_run(loop);

    pw_stream_destroy(stream);
    pw_core_disconnect(core);
    pw_context_destroy(context);
    pw_main_loop_destroy(loop);
    pw_deinit();

    ac->m_pw_stream = nullptr;
    ac->m_pw_main_loop = nullptr;
    ac->m_running.store(false, std::memory_order_release);
}

bool AudioCapture::Init(int sample_rate, int fft_size)
{
    m_sample_rate = sample_rate;
    m_fft_size = fft_size;

    m_fft.Init(fft_size, fft_size, 1, 1.0f);
    m_fft_window = new float[fft_size]();
    m_fft_output = new float[fft_size / 2]();
    m_waveform = new float[fft_size]();
    m_waveform_len = fft_size;

    memset(m_ring, 0, sizeof(m_ring));
    m_ring_write.store(0, std::memory_order_relaxed);
    m_ring_read.store(0, std::memory_order_relaxed);

    try {
        m_pw_thread = new std::thread(pw_thread_fn, this);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        if (!m_running.load())
        {
            fprintf(stderr, "AudioCapture: PipeWire unavailable (running without audio)\n");
            Shutdown();
            return false;
        }
    }
    catch (...)
    {
        fprintf(stderr, "AudioCapture: Failed to create PipeWire thread\n");
        return false;
    }

    printf("AudioCapture: PipeWire initialized (%d Hz, FFT %d)\n",
           sample_rate, fft_size);
    return true;
}

void AudioCapture::Shutdown()
{
    if (m_pw_thread)
    {
        if (m_running.load() && m_pw_main_loop)
            pw_main_loop_quit(static_cast<pw_main_loop*>(m_pw_main_loop));
        static_cast<std::thread*>(m_pw_thread)->join();
        delete static_cast<std::thread*>(m_pw_thread);
        m_pw_thread = nullptr;
    }
    delete[] m_fft_window;
    delete[] m_fft_output;
    delete[] m_waveform;
    m_fft_window = nullptr;
    m_fft_output = nullptr;
    m_waveform = nullptr;
    m_fft.CleanUp();
}

void AudioCapture::Read(float& bass, float& mid, float& treb,
                         float& bass_att, float& mid_att, float& treb_att)
{
    int write_pos = m_ring_write.load(std::memory_order_acquire);
    int read_pos = m_ring_read.load(std::memory_order_relaxed);
    int available = (write_pos - read_pos + RING_SIZE) % RING_SIZE;

    if (available >= m_fft_size)
    {
        for (int i = 0; i < m_fft_size; i++)
            m_fft_window[i] = m_ring[(read_pos + i) % RING_SIZE];
        m_ring_read.store((read_pos + m_fft_size) % RING_SIZE, std::memory_order_release);

        // Save waveform for wave rendering
        memcpy(m_waveform, m_fft_window, m_fft_size * sizeof(float));

        m_fft.time_to_frequency_domain(m_fft_window, m_fft_output);

        int numFreq = m_fft_size / 2;
        float bin_freq = (float)m_sample_rate * 0.5f / (float)numFreq;

        int bass_end   = (int)(761.0f / bin_freq);
        int mid_end    = (int)(2897.0f / bin_freq);
        if (bass_end > numFreq) bass_end = numFreq;
        if (mid_end > numFreq) mid_end = numFreq;

        float sum_bass = 0.0f, sum_mid = 0.0f, sum_treb = 0.0f;
        for (int i = 0; i < bass_end; i++) sum_bass += m_fft_output[i];
        for (int i = bass_end; i < mid_end; i++) sum_mid += m_fft_output[i];
        for (int i = mid_end; i < numFreq; i++) sum_treb += m_fft_output[i];

        int n_bass = bass_end;
        int n_mid = mid_end - bass_end;
        int n_treb = numFreq - mid_end;
        if (n_bass > 0) sum_bass /= (float)n_bass;
        if (n_mid > 0) sum_mid /= (float)n_mid;
        if (n_treb > 0) sum_treb /= (float)n_treb;

        float attack = 0.9f, release = 0.15f;
        auto smooth = [&](float& out, float in) {
            if (in > out) out += (in - out) * attack;
            else          out += (in - out) * release;
        };

        smooth(m_bass, sum_bass);
        smooth(m_mid, sum_mid);
        smooth(m_treb, sum_treb);

        auto peak = [](float& out, float in) {
            if (in > out) out = in;
            else          out *= 0.995f;
        };
        peak(m_bass_att, sum_bass);
        peak(m_mid_att, sum_mid);
        peak(m_treb_att, sum_treb);
    }

    bass = m_bass;
    mid = m_mid;
    treb = m_treb;
    bass_att = m_bass_att;
    mid_att = m_mid_att;
    treb_att = m_treb_att;
}

int AudioCapture::GetWaveform(float* dest, int max_samples) const
{
    int n = m_waveform_len;
    if (n > max_samples) n = max_samples;
    memcpy(dest, m_waveform, n * sizeof(float));
    return n;
}
