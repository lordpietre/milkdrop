#ifndef __AUDIO_CAPTURE_H__
#define __AUDIO_CAPTURE_H__

#include "fft.h"
#include <atomic>

class AudioCapture
{
public:
    AudioCapture();
    ~AudioCapture();

    bool Init(int sample_rate = 44100, int fft_size = 1024);
    void Shutdown();

    // Called each frame: fills spectrum and computes bass/mid/treb
    void Read(float& bass, float& mid, float& treb,
              float& bass_att, float& mid_att, float& treb_att);

    // Get latest waveform samples (mono, call after Read())
    int GetWaveform(float* dest, int max_samples) const;

    int GetSpectrumSize() const { return m_fft_size / 2; }

private:
    friend void pw_thread_fn(AudioCapture*);
    friend void on_process(void*);

    void ProcessAudio(const float* samples, int nframes, int nchannels);

    FFT m_fft;
    int m_sample_rate;
    int m_fft_size;

    // Ring buffer for audio data from PW thread
    static const int RING_SIZE = 16384;
    float m_ring[RING_SIZE];
    std::atomic<int> m_ring_write;
    std::atomic<int> m_ring_read;

    // PipeWire state (opaque pointers)
    void* m_pw_stream;
    void* m_pw_main_loop;
    void* m_pw_thread;
    std::atomic<bool> m_running;

    // FFT work buffers
    float* m_fft_window;
    float* m_fft_output;

    // Latest waveform samples (for wave rendering)
    float* m_waveform;
    int m_waveform_len;

    // Smoothed outputs
    float m_bass, m_mid, m_treb;
    float m_bass_att, m_mid_att, m_treb_att;
};

#endif
