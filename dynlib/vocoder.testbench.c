/*#define __SATAN_USES_FXP*/
#define __SATAN_USES_FLOATS

#ifdef __SATAN_USES_FXP
#define OUTPUT_FNAME "/tmp/mocking_fxp.wav"
#define OUTPUT_B_FNAME "/tmp/mocking_b_fxp.wav"
#else
#define OUTPUT_FNAME "/tmp/mocking_float.wav"
#define OUTPUT_B_FNAME "/tmp/mocking_b_float.wav"
#endif

#include "vocoder.c"
#include "libtestbench.c"

#define TEST_LENGTH 44100 * 10
#define STEP_LENGTH 370
#define MAX_STEPS (TEST_LENGTH / STEP_LENGTH)

FTYPE modulator_signal[TEST_LENGTH];
FTYPE a_signal[TEST_LENGTH];	
FTYPE carrier_signal[TEST_LENGTH];
FTYPE output_signal[TEST_LENGTH];	

int main(int argc, char **argv) {
	struct mockery m;
	
	if(prepare_mockery(&m, STEP_LENGTH, "vocoder")) {
		printf("Failed to initiate mockery of module rewerb.\n");
		exit(1);
	}

	if(create_mock_input_signal(
		   &m, "Modulator",
		   _0D,
		   1,
		   FTYPE_RESOLUTION,
		   44100,
		   TEST_LENGTH,
		   modulator_signal
		   )) {
		printf("Failed to create mock modulator signal.\n");
		exit(2);
	}
	if(create_mock_input_signal(
		   &m, "Carrier",
		   _0D,
		   1,
		   FTYPE_RESOLUTION,
		   44100,
		   TEST_LENGTH,
		   carrier_signal
		   )) {
		printf("Failed to create mock carrier signal.\n");
		exit(2);
	}
	if(create_mock_output_signal(
		   &m, "Mono",
		   _0D,
		   1,
		   FTYPE_RESOLUTION,
		   44100,
		   TEST_LENGTH,
		   output_signal
		   )) {
		printf("Failed to create mock output signal.\n");
		exit(3);
	}

	if(create_mock_intermediate_signal(
		   &m, "A",
		   _0D,
		   1,
		   FTYPE_RESOLUTION,
		   44100,
		   TEST_LENGTH,
		   a_signal
		   )) {
		printf("Failed to create mock output signal.\n");
		exit(3);
	}


	memset(modulator_signal, 0, sizeof(modulator_signal));
	memset(carrier_signal, 0, sizeof(carrier_signal));
	memset(output_signal, 0, sizeof(output_signal));
	memset(a_signal, 0, sizeof(output_signal));

	{
		int rd;
		int16_t *riffdata = (int16_t *)calloc(TEST_LENGTH * 2, sizeof(int16_t));
		
		RIFF_WAVE_FILE_t rwf;
		
		RIFF_open_file(&rwf, "carrier44.wav");

		rd = RIFF_read_data(&rwf, riffdata, TEST_LENGTH);

		convert_pcm_to_mock_signal(
			carrier_signal, 1, TEST_LENGTH,

			riffdata,
			rwf.channels, rwf.samples);

		RIFF_close_file(&rwf);
		printf("   read: %d\n", rd);
		printf("data[0] = %d\n", riffdata[0]);
		printf("data[1] = %d\n", riffdata[1]);
		printf("data[2] = %d\n", riffdata[2]);
		printf("data[3] = %d\n", riffdata[3]);
	}
	{
		int rd;
		int16_t *riffdata = (int16_t *)calloc(TEST_LENGTH * 2, sizeof(int16_t));
		
		RIFF_WAVE_FILE_t rwf;
		
		RIFF_open_file(&rwf, "modulator44.wav");

		rd = RIFF_read_data(&rwf, riffdata, TEST_LENGTH);
		convert_pcm_to_mock_signal(
			modulator_signal, 1, TEST_LENGTH,

			riffdata,
			rwf.channels, rwf.samples);

		RIFF_close_file(&rwf);
		printf("   read: %d\n", rd);
		printf("data[0] = %d\n", riffdata[0]);
		printf("data[1] = %d\n", riffdata[1]);
		printf("data[2] = %d\n", riffdata[2]);
		printf("data[3] = %d\n", riffdata[3]);
	}

	make_a_mockery(&m, MAX_STEPS);
	
	write_wav(output_signal, output_signal, TEST_LENGTH, OUTPUT_FNAME);
	write_wav(carrier_signal, modulator_signal, TEST_LENGTH, OUTPUT_B_FNAME);

	printf("\n\n");

	return 0;
}
