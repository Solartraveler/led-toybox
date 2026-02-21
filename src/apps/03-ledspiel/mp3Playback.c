#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "mp3Playback.h"

#include "ledspiellib/timer32Bit.h"
#include "audioOut.h"
#include "ff.h"
#include "mad.h"
#include "utility.h"


#define SD_BLOCKSIZE 512

/*The behaviour of libmad is mostly undocumented.
  According to https://lists.mars.org/hyperkitty/list/mad-dev@lists.mars.org/message/23ACZCLN3DMTR62GDAQNBGNUUMXORWYR/
  the maximum mp3 frame size is 2881 bytes, then libmad needs 8 additional guard bytes.
  Then we round this up to 2 additional SD card block size -> 3584, so we can always read
  at least one frame into the buffer without needing to read a partial block from the source.
*/
#define IN_BUFFER_SIZE (SD_BLOCKSIZE * 7)

/*Should buffer 0.2s at 16bit, 44100Hz, mono -> 17640 bytes needed.
*/
#define OUTPUT_BUFFER_ELEMENTS 8820

//Reverse engineered by looking into steam and mad.h:
//22652 byte for 32-bit ARM, 22732 byte for 64-bit x64
#define SYNC_BUFFER_SIZE (sizeof(struct mad_stream) + sizeof(struct mad_frame) + sizeof(struct mad_synth))

//See layer3.c function mad_layer_III
#define OVERLAP_BUFFER_SIZE (2 * 32 * 18 * sizeof(mad_fixed_t))

//Should be accessible by DMA!
uint16_t g_dacFifoBuffer[OUTPUT_BUFFER_ELEMENTS];

typedef struct {
	FIL f; //file to read the input data from
	bool opened; //if true, f is valid
	float volume; //scaling factor
	uint8_t inBuffer[IN_BUFFER_SIZE]; //file buffer to be used by libmad
	size_t inBufferUsed; //until which position inBuffer contains valid data
	uint32_t bytesRead; //number of bytes read from the input file
	uint32_t bytesGenerated; //number of decoded samples generated (converted to 8bit, so equal to output bytes)
	bool play; //if true, the file is not at the end yet (or the user did not select stop)
	bool needData; //if true, decoding function should refill inBuffer
	uint32_t sampleRate; //in [Hz]
	uint32_t bitRate; //in [Hz]
	uint32_t channels; //1 = mono, 2 = stereo
	uint64_t ticksTotal; //[CPU ticks] for performance measurement
	uint64_t ticksInput; //[CPU ticks] for performance measurement
	uint64_t ticksOutput; //[CPU ticks] for performance measurement
	uint64_t ticksWaiting; //[CPU ticks] for performance measurement
	struct mad_decoder madDecoder; //the libmad state struct
	//Internal buffers for libmad:
	uint8_t syncBuffer[SYNC_BUFFER_SIZE];
	unsigned char main_data[MAD_BUFFER_MDLEN];
	uint8_t overlap[OVERLAP_BUFFER_SIZE];
} playerState_t;

//Currently ccmram is not initialized
__attribute__((section(".ccmram"), aligned(4))) playerState_t g_playerState;

//A big thank you to https://stackoverflow.com/questions/39803572/libmad-playback-too-fast-if-read-in-chunks
enum mad_flow MadInput(void *data, struct mad_stream *stream) {
	(void)data;
	if ((g_playerState.opened == false) || (g_playerState.play == false)) {
		g_playerState.play = false;
		return MAD_FLOW_STOP;
	}
	uint32_t tStart = Timer32BitGet();
	size_t maxRead = IN_BUFFER_SIZE;
	//Did the decoder processed some data?
	size_t leftBefore = 0;
	size_t consumed = 0;
	if (stream->error == MAD_ERROR_BUFLEN) {
		if (stream->next_frame) {
			consumed = stream->next_frame - stream->buffer;
			leftBefore = g_playerState.inBufferUsed - consumed;
		}
	}
	if (leftBefore < maxRead) {
		if (leftBefore) {
			memmove(g_playerState.inBuffer, g_playerState.inBuffer + consumed, leftBefore);
		}
		maxRead -= leftBefore;
	} else {
		leftBefore = 0; //if no data were processed - skip the data alltogehter - they seem to contain no frame at all
	}
	//round it down to the next block size
	maxRead -= maxRead % SD_BLOCKSIZE;
	//printf("Consumed: %u, left before %u, will read %u\r\n", (unsigned int)consumed, (unsigned int)leftBefore, (unsigned int)maxRead);
	//read data and give the buffer back
	UINT r = 0;
	enum mad_flow result = MAD_FLOW_STOP;
	FRESULT rRead = f_read(&(g_playerState.f), g_playerState.inBuffer + leftBefore, maxRead, &r);
	if ((rRead == FR_OK) && (r > 0)) {
		//printf("Input read %u bytes\r\n", (unsigned int)r);
		mad_stream_buffer(stream, g_playerState.inBuffer, r + leftBefore);
		g_playerState.bytesRead += r;
		g_playerState.inBufferUsed = r + leftBefore;
		//printf("Input processed %u bytes\r\n", (unsigned int)r);
		result = MAD_FLOW_CONTINUE;
	} else {
		printf("Error, read returned with code %u\r\n", rRead);
		g_playerState.play = false;
	}
	uint32_t tStop = Timer32BitGet();
	g_playerState.ticksInput += tStop - tStart;
	return result;
}

//Function mostly copied from minimad.c found in libmad
enum mad_flow MadError(void *data, struct mad_stream *stream, struct mad_frame *frame) {
	(void)data;
	(void)frame;
	printf("Error, decoding 0x%04x (%s) at byte offset %u\r\n", stream->error, mad_stream_errorstr(stream), (unsigned int)g_playerState.bytesRead);
	if (stream->error == MAD_ERROR_NONE) {
		return MAD_FLOW_CONTINUE;
	}
	return MAD_FLOW_IGNORE;
}

enum mad_flow MadHeader(void *data, struct mad_header const * header) {
	(void)data;
	//printf("Mad header\r\n");
	if ((g_playerState.sampleRate == 0) && (header->samplerate)) {
		printf("Mad header with sample rate %u\r\n", header->samplerate);
		g_playerState.sampleRate = header->samplerate;
		if (!AudioInit(g_dacFifoBuffer, OUTPUT_BUFFER_ELEMENTS, g_playerState.sampleRate)) {
			return MAD_FLOW_STOP;
		}
	}
	g_playerState.bitRate = header->bitrate;
	if (header->mode == MAD_MODE_SINGLE_CHANNEL) {
		g_playerState.channels = 1;
	} else {
		g_playerState.channels = 2;
	}
	//printf("Mad header callback, bitrate %u\r\n", (unsigned int)g_playerState.bitRate);
	return MAD_FLOW_CONTINUE;
}

//Function mostly copied from minimad.c found in libmad
int32_t MadScale(mad_fixed_t sample) {
	/* round */
	sample += (1L << (MAD_F_FRACBITS - 16));
	/* clip */
	if (sample >= MAD_F_ONE) {
		sample = MAD_F_ONE - 1;
	} else if (sample < -MAD_F_ONE) {
		sample = -MAD_F_ONE;
	}
	/* quantize */
	return sample >> (MAD_F_FRACBITS + 1 - 16);
}

//Function mostly copied from minimad.c found in libmad
enum mad_flow MadOutput(void *data, struct mad_header const *header, struct mad_pcm *pcm) {
	(void)data;
	(void)header;
	uint32_t nchannels, nsamples;
	mad_fixed_t const *left_ch, *right_ch;
	uint32_t tStart = Timer32BitGet();
	uint32_t tWaitTicks = 0;
	/* pcm->samplerate contains the sampling frequency */
	nchannels = pcm->channels;
	nsamples  = pcm->length;
	left_ch   = pcm->samples[0];
	right_ch  = pcm->samples[1];
	//size_t freeStart = SeqFifoFree();
	//printf("Output has %u samples ready\r\n", (unsigned int)nsamples);
	if (g_playerState.sampleRate == 0) {
		printf("No output sample rate defined\r\n");
		//audio out has not started, therefore stop?
		return MAD_FLOW_STOP;
	}
	for (uint32_t i = 0; i < nsamples; i++) {
		float sample;
		/* output sample(s) in 16-bit signed little-endian PCM */
		sample = MadScale(*left_ch++);
		if (nchannels == 2) {
			sample += MadScale(*right_ch++);
			sample /= 2.0; //smash left and right channel together
		}
		sample *= g_playerState.volume;
		//signed 16bit to unsigned 12bit PCM
		sample /= 16;
		sample += 2048;
		sample = MAX(sample, 0);
		sample = MIN(sample, AUIDO_VALUE_MAX);
		uint16_t sample16 = sample;
		if (AudioBufferFreeGet() == 0) {
			uint32_t tWaitStart = Timer32BitGet();
			while (AudioBufferFreeGet() == 0) {
			//This will happen because the next call to PlaybackProcess does not care about the FIFO state
			//printf("Waiting for FIFO, need to put %u\r\n", (unsigned int)nsamples - i );
			}
			uint32_t tWaitStop = Timer32BitGet();
			tWaitTicks += tWaitStop - tWaitStart;
		}
		AudioBufferPut(sample16);
	}
	g_playerState.bytesGenerated += nsamples;
	uint32_t tStop = Timer32BitGet();
	g_playerState.ticksOutput += (tStop - tStart) - tWaitTicks;
	g_playerState.ticksWaiting += tWaitTicks;
	if (AudioBufferFreeGet() < nsamples) {
		//We simply assume we get the same amount of data next time, so it should fit into the buffer without waiting
		//printf("Only %u bytes free in FIFO\r\n", (unsigned int)SeqFifoFree());
		return MAD_FLOW_STOP;
	}
	//printf("Completed output\r\n");
	return MAD_FLOW_CONTINUE;
}

bool PlaybackStart(const char * filename, float volume) {
	PlaybackStop();
	memset(&g_playerState, 0, sizeof(g_playerState)); //because the memory is not initialized by the startup code

	if (f_open(&g_playerState.f, filename, FA_READ) != FR_OK) {
		printf("Error, could not open file\r\n");
		return false;
	}
	g_playerState.inBufferUsed = 0;
	g_playerState.sampleRate = 0;
	g_playerState.bitRate = 0;
	g_playerState.ticksInput = 0;
	g_playerState.ticksOutput = 0;
	g_playerState.ticksTotal = 0;
	g_playerState.opened = true;
	g_playerState.play = true;
	g_playerState.needData = true;
	g_playerState.volume = volume;
	mad_decoder_init(&g_playerState.madDecoder, NULL, MadInput, MadHeader, NULL, MadOutput, MadError, 0 /* message */);
	//This would play, but would block until the mp3 is finished (or the watchdog bites us):
	//mad_decoder_run(&g_playerState.madDecoder, MAD_DECODER_MODE_SYNC);
	//instead we run the internal loop manually, and this is the init code:
	struct mad_decoder * pMad = &(g_playerState.madDecoder);
	pMad->sync = (void *)g_playerState.syncBuffer;
	struct mad_stream * pStream = &pMad->sync->stream;
	struct mad_frame * pFrame = &pMad->sync->frame;
	struct mad_synth * pSynth = &pMad->sync->synth;
	mad_stream_init(pStream);
	mad_frame_init(pFrame);
	mad_synth_init(pSynth);
	mad_stream_options(pStream, pMad->options);
	//source analysis show we can simply set a fixed buffer, so malloc and calloc will never be called
	//this however forbids to call the deinit functions, as otherwise they would free the static buffers
	pStream->main_data = (void *)g_playerState.main_data;
	memset(g_playerState.overlap, 0, OVERLAP_BUFFER_SIZE); //cause original code uses calloc - clear data from previous play
	pFrame->overlap = (void *)g_playerState.overlap;
	return true;
}

//reverse engineed form decoder.c
bool PlaybackProcess(void) {
	if (!g_playerState.play) {
		return false;
	}
	Timer32BitInit(0);
	Timer32BitStart();
	void *error_data = NULL;
	struct mad_decoder * pMad = &(g_playerState.madDecoder);
	struct mad_stream * stream = &pMad->sync->stream;
	struct mad_frame * frame = &pMad->sync->frame;
	struct mad_synth * synth = &pMad->sync->synth;
	if (g_playerState.needData) {
		if (pMad->input_func(pMad->cb_data, stream) != MAD_FLOW_CONTINUE) {
			uint32_t stamp = Timer32BitGet();
			g_playerState.ticksTotal += stamp;
			Timer32BitStop();
			PlaybackEvaluatePerformance();
			return true;
		}
	}
	g_playerState.needData = true;
	while (1) {
		if (pMad->header_func) {
			if (mad_header_decode(&frame->header, stream) == -1) {
				if (!MAD_RECOVERABLE(stream->error)) {
					break;
				}
				pMad->error_func(error_data, stream, frame);
			}
			enum mad_flow headerResult = pMad->header_func(pMad->cb_data, &frame->header);
			if ((headerResult == MAD_FLOW_STOP) || ((headerResult == MAD_FLOW_BREAK))) {
				break;
			}
			if (headerResult == MAD_FLOW_IGNORE) {
				continue;
			}
		}
		if (mad_frame_decode(frame, stream) == -1) {
			if (!MAD_RECOVERABLE(stream->error)) {
				break;
			}
			if (pMad->error_func(error_data, stream, frame) != MAD_FLOW_CONTINUE) {
				continue;
			}
		}
		//We have no filter func...
		mad_synth_frame(synth, frame);
		if (pMad->output_func(pMad->cb_data, &frame->header, &synth->pcm) == MAD_FLOW_STOP) {
			g_playerState.needData = false;
			break;
		}
	}
	uint32_t stamp = Timer32BitGet();
	g_playerState.ticksTotal += stamp;
	Timer32BitStop();
	//printf("Read: %u\r\n", (unsigned int)g_playerState.bytesRead);
	return true;
}

bool PlaybackStop(void) {
	AudioStop();
	return true;
}

void PlaybackEvaluatePerformance(void) {
	uint32_t bytesPerSecond = g_playerState.sampleRate;
	if (!bytesPerSecond) {
		return;
	}
	uint32_t secondsPlayed = g_playerState.bytesGenerated / bytesPerSecond;
	uint64_t cycles = (uint64_t)secondsPlayed * (uint64_t)F_CPU;
	if (!cycles) {
		return;
	}
#if (F_CPU > 84000000)
	//timer counts no longer with F_CPU
	g_playerState.ticksTotal *= 2;
	g_playerState.ticksInput *= 2;
	g_playerState.ticksOutput *= 2;
	g_playerState.ticksWaiting *= 2;
#endif
	uint64_t percentageTotal = g_playerState.ticksTotal * (uint64_t)100 / cycles;
	uint64_t ticksComputing = g_playerState.ticksTotal - g_playerState.ticksInput - g_playerState.ticksOutput - g_playerState.ticksWaiting;
	uint64_t percentageInput = g_playerState.ticksInput * (uint64_t)100 / cycles;
	uint64_t percentageComputing = ticksComputing * (uint64_t)100 / cycles;
	uint64_t percentageOutput = g_playerState.ticksOutput * (uint64_t)100 / cycles;
	uint64_t percentageWaiting = g_playerState.ticksWaiting * (uint64_t)100 / cycles;
	printf("CPU Ticks for playing %9uM - %02u%c\r\n", (unsigned int)(g_playerState.ticksTotal / 1000000ULL), (unsigned int)percentageTotal, '%');
	printf("  Input               %9uM - %02u%c\r\n", (unsigned int)(g_playerState.ticksInput / 1000000ULL), (unsigned int)percentageInput, '%');
	printf("  Computing           %9uM - %02u%c\r\n", (unsigned int)(ticksComputing / 1000000ULL), (unsigned int)percentageComputing, '%');
	printf("  Output              %9uM - %02u%c\r\n", (unsigned int)(g_playerState.ticksOutput / 1000000ULL), (unsigned int)percentageOutput, '%');
	printf("  Output waiting      %9uM - %02u%c\r\n", (unsigned int)(g_playerState.ticksWaiting / 1000000ULL), (unsigned int)percentageWaiting, '%');
}

void PlaybackVolume(float scale) {
	g_playerState.volume = scale;
}

/*Dummy functions to save memory. They are referenced by libmad, but are not called
  in the setup this app is providing for the lib. The compiler does a #define malloc mallocIncept
  for all files.
*/
void * mallocIncept(size_t size) {
	printf("Error, malloc disabled - want %u bytes\r\n", (unsigned int)size);
	return NULL;
}

void * callocIncept(size_t nmem, size_t size) {
	printf("Error, calloc disabled - want %u bytes\r\n", (unsigned int)(nmem * size));
	return NULL;
}

void freeIncept(void * ptr) {
	printf("Error, free disabled - 0x%x\r\n", (unsigned int)(uintptr_t)ptr);
}

void abortIncept(void) {
	printf("Error, abort called\r\n");
	while(1);
}
