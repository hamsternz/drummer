#include <stdint.h>
#include <stdio.h>
#include <memory.h>

#include "samples/drum_clap.h"
#include "samples/drum_hihat.h"
#include "samples/drum_kick.h"
#include "samples/drum_perc.h"
#include "samples/drum_snare.h"

#define N_VOICES 6

#define BPM           155
#define SAMPLE_RATE 44100
#define BEATS_PER_BAR   4
#define BAR_LEN        72
#define LOOP_BARS      10

static int bar = 0;
static int tickOfBar = 0;
static int sampleOfTick = 0;
static int samplesPerBeat;
static int samplesPerTick;
static int samplesToGenerate;

const struct Sounds {
    const int16_t *samples;
    const size_t  len;
} sounds[] = {
   {drum_kick,  sizeof(drum_kick)/sizeof(int16_t)},
   {drum_clap,  sizeof(drum_clap)/sizeof(int16_t)},
   {drum_snare, sizeof(drum_snare)/sizeof(int16_t)},
   {drum_hihat, sizeof(drum_hihat)/sizeof(int16_t)},
   {drum_perc,  sizeof(drum_perc)/sizeof(int16_t)}
};

static struct voice {
    int pan;
    int volume;
    int sample;
    int pos;
    int emph;
} voices[N_VOICES] = {
   { 16,  192, 0, 0, 0},
   {  8,    0, 1, 0, 0},
   { 16,   40, 2, 0, 0},
   { 18,   80, 3, 0, 0},
   { 28,   30, 4, 0, 0},
   {  4,   30, 0, 0, 0}
};

struct Pattern {
   char pattern[N_VOICES][BAR_LEN];
};

static struct Pattern pattern0 = {{
//"012345678901234567890123456789012345678901234567890123456789012345678901"
  "1                                                                       ",
  "                                                                        ",
  "1                                                                       ",
  "                                                                        ",
  "                                                                        ",
  "                                                                        "
}};

static struct Pattern pattern1 = {{
//"012345678901234567890123456789012345678901234567890123456789012345678901"
  "9                                   1                                   ",
  "                                                                        ",
  "4        1        1        1        3        1        1        1        ",
  "                                                                        ",
  "                                                                        ",
  "                                                                        "
}};

static struct Pattern pattern2 = {{
//"012345678901234567890123456789012345678901234567890123456789012345678901"
  "9                                   1                                   ",
  "         1                 1                 1                 1        ",
  "1                 1                 1                 1                 ",
  "5        1        1        1        1        1                 1        ",
  "5                          1                 1                          ",
  "         1                          4                          1        "
}};

struct Pattern *patterns[LOOP_BARS] = {
   &pattern0,
   &pattern1,
   &pattern2
};

int bars[LOOP_BARS] = { 1,1,1,2,2,2,2,2,1,0};



void generate_sample(int16_t o[2]) {
   int32_t samples[N_VOICES];
   int32_t output[2];

   // Start of new tick?
   if(sampleOfTick == 0) {
      for(int i = 0; i < N_VOICES; i++) {
         if(patterns[bars[bar]]->pattern[i][tickOfBar] != ' ') {
            voices[i].pos  = 1;
            voices[i].emph = (patterns[bars[bar]]->pattern[i][tickOfBar]-'1') * 12;
         }
      }
   }

   for(int i = 0; i < N_VOICES; i++) {
      samples[i] = 0;
      if(voices[i].sample >= 0) {
         samples[i] = sounds[voices[i].sample].samples[voices[i].pos ];
         if(voices[i].pos != 0) {
            voices[i].pos++;
            if(voices[i].pos  >= sounds[voices[i].sample].len) {
               voices[i].pos  = 0;
            } 
         }
      }
   }

   output[0] = 0;
   output[1] = 0;
   for(int i = 0; i < N_VOICES; i++) {
      int vol = voices[i].emph + voices[i].volume;
      output[0] += vol * samples[i]*(voices[i].pan);
      output[1] += vol * samples[i]*(32-voices[i].pan);
   }

   sampleOfTick++;
   if(sampleOfTick == samplesPerTick) {
      sampleOfTick = 0;
      tickOfBar++;
      if(tickOfBar == BAR_LEN) {
         tickOfBar = 0;
         bar++;
      }
   }

   // Convert back to 16 bit
   o[0] = output[0] / (32 *4 * 256);
   o[1] = output[1] / (32 *4 * 256);
}

////////////////////////////////////////////////////////////////////////

struct WaveFileHeader {
  uint8_t  chunkID[4];
  uint8_t  chunkSize[4];
  uint8_t  chunkFormat[4];

  uint8_t  subchunk1ID[4];
  uint8_t  subchunk1Size[4];
  uint8_t  audioFormat[2];
  uint8_t  numChannels[2];
  uint8_t  sampleRate[4];
  uint8_t  byteRate[4];
  uint8_t  blockAlign[2];
  uint8_t  bitsPerSample[2];

  uint8_t  subchunk2ID[4];
  uint8_t  subchunk2Size[4];
};


void intToU4(uint8_t *d, int val) {
   d[0] = val;
   d[1] = val>>8;
   d[2] = val>>16;
   d[3] = val>>24;
}

void intToU2(uint8_t *d, int val) {
   d[0] = val&0xFF;
   d[1] = val>>8;
}

int output_header(FILE *f, int samples_to_generate) {
   struct WaveFileHeader header;

   memcpy(header.chunkID,       "RIFF",4);
   intToU4(header.chunkSize,     36 + samples_to_generate*4);
   memcpy(header.chunkFormat,    "WAVE",4);

   memcpy(header.subchunk1ID,   "fmt ",4);
   intToU4(header.subchunk1Size, 16);
   intToU2(header.audioFormat,   1);
   intToU2(header.numChannels,   2);
   intToU4(header.sampleRate,    44100);
   intToU4(header.byteRate,      44100*4);
   intToU2(header.blockAlign,    4);
   intToU2(header.bitsPerSample, 16);
   memcpy(header.subchunk2ID,   "data", 4);
   intToU4(header.subchunk2Size, samples_to_generate*4);
   if(fwrite(&header, sizeof(header), 1, f) != 1) {
      return 0;
   }
   return 1;
}
int write_sample(FILE *f, int16_t samples[2]) {
   uint8_t o[4];
   o[0] = samples[0];
   o[1] = samples[0]>>8;
   o[2] = samples[1];
   o[3] = samples[1]>>8;
   if(fwrite(o, sizeof(o), 1, f) != 1) 
     return 0;
   return 1;
}

////////////////////////////////////////////////////////////////////////
int main(int argc, char *argv[]) {
   FILE *f;
   int16_t samples[2];
   samplesPerBeat = SAMPLE_RATE*60/BPM;
   samplesPerTick = samplesPerBeat * BEATS_PER_BAR / BAR_LEN;
   samplesToGenerate = samplesPerTick * BAR_LEN * LOOP_BARS;

   printf("Samples per beet is %i\n", samplesPerBeat);
   printf("samples_per tick is %i\n", samplesPerTick);
   printf("samples_to_generate is %i\n", samplesToGenerate);
  
   f = fopen("out.wav","w");
   if(f == NULL) {
     fprintf(stderr, "Unable to open output file\n");
     return 0;
   } 
   output_header(f, samplesToGenerate);
   for(int i = 0; i < samplesToGenerate; i++) {
       generate_sample(samples);
       write_sample(f, samples);
   }
   fclose(f);
   printf("sampleOfTick %i\n",sampleOfTick);
   printf("tickOfBar    %i\n",tickOfBar);
   printf("bar          %i\n",bar);
   return 0;
}
