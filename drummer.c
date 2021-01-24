#include <stdint.h>
#include <stdio.h>
#include <memory.h>

#include "samples/drum_clap.h"
#include "samples/drum_hihat.h"
#include "samples/drum_kick.h"
#include "samples/drum_perc.h"
#include "samples/drum_snare.h"

#define N_VOICES 5

#define BPM           155
#define SAMPLE_RATE 44100
#define BEATS_PER_BAR   4
#define BAR_LEN        72
#define LOOP_BARS       8

const struct voice {
    const int     pan;
    const int     volume;
    const int16_t *samples;
    const size_t  len;
} voices[N_VOICES] = {
   { 16,  256, drum_kick,  sizeof(drum_kick)/sizeof(int16_t)},
   {  8,   32, drum_clap,  sizeof(drum_clap)/sizeof(int16_t)},
   { 16,  128, drum_snare, sizeof(drum_snare)/sizeof(int16_t)},
   { 24,   64, drum_hihat, sizeof(drum_hihat)/sizeof(int16_t)},
   { 16,   32, drum_perc,  sizeof(drum_perc)/sizeof(int16_t)}
};

char patterns[N_VOICES][BAR_LEN] = {
//"012345678901234567890123456789012345678901234567890123456789012345678901"
  "1                                   1                                   ",
  "         1                                   1                 1        ",
  "1                 1                 1                 1                 ",
  "         1                 1                                            ",
  "                                                                        "
};


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

int voicestate[N_VOICES];

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

static int bar = 0;
static int tickOfBar = 0;
static int sampleOfTick = 0;
static int samplesPerBeat;
static int samplesPerTick;
static int samplesToGenerate;

void generate_sample(FILE *f) {
   int32_t samples[N_VOICES];
   int32_t output[2];
   int16_t o[2];

   // Start of new tick?
   if(sampleOfTick == 0) {
      for(int i = 0; i < N_VOICES; i++) {
         if(patterns[i][tickOfBar] != ' ') {
            voicestate[i] = 1;
         }
      }
   }

   for(int i = 0; i < N_VOICES; i++) {
      samples[i] = voices[i].samples[voicestate[i]];
      if(voicestate[i] != 0) {
         voicestate[i]++;
         if(voicestate[i] >= voices[i].len) {
            voicestate[i] = 0;
         } 
      }
   }

   output[0] = 0;
   output[1] = 0;
   for(int i = 0; i < N_VOICES; i++) {
      output[0] += voices[i].volume * samples[i]*(voices[i].pan);
      output[1] += voices[i].volume * samples[i]*(32-voices[i].pan);
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
   fwrite(o, sizeof(o), 1, f);
}

int main(int argc, char *argv[]) {
   FILE *f;
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
       generate_sample(f);
   }
   fclose(f);
   printf("sampleOfTick %i\n",sampleOfTick);
   printf("tickOfBar    %i\n",tickOfBar);
   printf("bar          %i\n",bar);
   return 0;
}
