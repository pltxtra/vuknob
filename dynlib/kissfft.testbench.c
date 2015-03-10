#define __SATAN_USES_FXP
//#define __SATAN_USES_FLOATS

#include "kissfft.c"
#include "libtestbench.c"

#define TEST_LENGTH 1024 * 100

int main(int argc, char **argv) {
	struct mockery m;

	FTYPE input_signal[TEST_LENGTH];
	FTYPE output_signal[TEST_LENGTH];	
	
	if(prepare_mockery(&m, TEST_LENGTH, "kissfft")) {
		printf("Failed to initiate mockery of module kissfft.\n");
		exit(1);
	}

	if(create_mock_input_signal(
		   &m, "Mono",
		   _0D,
		   1,
		   44100,
		   TEST_LENGTH,
		   input_signal
		   )) {
		printf("Failed to create mock input signal.\n");
		exit(2);
	}
	if(create_mock_output_signal(
		   &m, "Mono",
		   _0D,
		   1,
		   44100,
		   TEST_LENGTH,
		   output_signal
		   )) {
		printf("Failed to create mock output signal.\n");
		exit(3);
	}

	memset(input_signal, 0, sizeof(input_signal));
	memset(output_signal, 0, sizeof(output_signal));

#if 0
	{
		int k;
		float f;
		for(k = 0; k < TEST_LENGTH; k++) {
			f = (float)k / (44100.0f);
			f *= 8500;
			f = 0.5f * sinf(2.0f * M_PI * f);

			input_signal[k] = ftoFTYPE(f);
		}
	}
#else
	{
		int rd;
		int16_t riffdata[TEST_LENGTH * 2];
		
		RIFF_WAVE_FILE_t rwf;
		
		RIFF_open_file(&rwf, "./whistle.wav");

		rd = RIFF_read_data(&rwf, riffdata, TEST_LENGTH);

		convert_pcm_to_mock_signal(
			input_signal, 1, TEST_LENGTH,

			riffdata,
			rwf.channels, rwf.samples);

		RIFF_close_file(&rwf);
		printf("   read: %d\n", rd);
		printf("data[0] = %d\n", riffdata[0]);
		printf("data[1] = %d\n", riffdata[1]);
		printf("data[2] = %d\n", riffdata[2]);
		printf("data[3] = %d\n", riffdata[3]);
	}
#endif
	
	make_a_mockery(&m);

	write_wav(output_signal, input_signal, TEST_LENGTH, "/tmp/mocking.wav");

	printf("\n\n");
	printf("input: %p\n", input_signal);
	printf("output: %p\n", output_signal);
}

#include "kiss_fft.c"
#include "kiss_fftr.c"
