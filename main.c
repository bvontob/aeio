#include "userosc.h"
#include "biquad.h"

#define VOWELS   5
#define FORMANTS 2

#define MIN_Q M_SQRT2
#define MAX_Q 16

#define SOFTCLIP_C 0.3f

#define BIQUAD_WC(F_HZ) ((F_HZ) * k_samplerate_recipf)
#define F(X) (M_PI * BIQUAD_WC(X))

static const float ft[VOWELS][FORMANTS] = {
  { F(1000), F(1400) }, /* 0:a <= 0.00f */
  {  F(500), F(2300) }, /* 1:e <= 0.25f */
  {  F(320), F(3200) }, /* 2:i <= 0.50f */
  {  F(500), F(1000) }, /* 3:o <= 0.75f */
  {  F(320),  F(800) }, /* 4:u <= 1.00f */
};

static biquad bq[FORMANTS];
static biquad *bqp[FORMANTS];

static int rand;

static float ff[FORMANTS];
static float q;

static float phi = 0.0f;

void OSC_INIT(uint32_t platform, uint32_t api) {
  (void)platform; (void)api;

  for (int i = 0; i < FORMANTS; i++)
    bqp[i] = &bq[i];
}

void OSC_CYCLE(const user_osc_param_t * const params,
               int32_t *yn,
               const uint32_t frames) {
  const uint8_t note = (params->pitch) >> 8;
  const uint8_t mod  = (params->pitch) & 0x00FF;
  const float   nidx = _osc_bl_saw_idx((float)note);
  const float   w    = osc_w0f_for_note(note, mod);
  const float   gain = 1.0f + (q - M_SQRT2) / (5 * M_SQRT2);

  (void)q31_to_f32(params->shape_lfo); /* TODO Shape LFO */
  
  q31_t * __restrict y = (q31_t *)yn;
  const q31_t * y_e = y + frames;
  
  for (; y != y_e; ) {
    const float sig_osc = (osc_bl2_sawf(phi, nidx)
			   + 0.05f * _osc_white());
    float sig = 0;

    for (int i = 0; i < FORMANTS; i++)
      sig += (1.0f / FORMANTS) * biquad_process_so(bqp[i], sig_osc);
    sig *= gain;
    sig  = osc_softclipf(SOFTCLIP_C, sig) * (1 / (1.0f - SOFTCLIP_C));
    
    phi += w;
    phi -= (uint32_t)phi;
    
    *(y++) = f32_to_q31(sig);
  }
}

void OSC_NOTEON(const user_osc_param_t * const params) {
  (void)params;
  if (rand) {
    const int idx = _osc_rand() % VOWELS;
    for (int i = 0; i < FORMANTS; i++) {
      ff[i] = ft[idx][i];
      biquad_so_bp(bqp[i], fasttanfullf(ff[i]), q);
    }
  }
}

void OSC_NOTEOFF(const user_osc_param_t * const params) {
  (void)params;
}

void OSC_PARAM(uint16_t idx, uint16_t val) { 
  switch (idx) {

  case k_user_osc_param_id1:
    rand = val;
    break;

  case k_user_osc_param_shape:
    {
      const float idf = param_val_to_f32(val) * (VOWELS - 1);
      const int   idx = (int)idf;
      const float off = idf - (float)idx;
      for (int i = 0; i < FORMANTS; i++)
	ff[i] = (1.0f - off) * ft[idx][i] + off * ft[idx + 1][i];
    }
    break;

  case k_user_osc_param_shiftshape:
    q = param_val_to_f32(val) * (MAX_Q - MIN_Q) + MIN_Q;
    break;

  default:
    break;
  }
  
  for (int i = 0; i < FORMANTS; i++)
    biquad_so_bp(bqp[i], fasttanfullf(ff[i]), q);
}
