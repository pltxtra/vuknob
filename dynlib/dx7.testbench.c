#include "libtestbench_timer.c"
#include "dx7.c"
#include "libtestbench.c"

#define STEP_LENGTH 240
#define MAX_STEPS  1837 * 4
#define TEST_LENGTH (STEP_LENGTH * MAX_STEPS)

void *input_signal[TEST_LENGTH];
FTYPE output_signal[TEST_LENGTH];	

int main(int argc, char **argv) {
	struct mockery m;

	
	if(prepare_mockery(&m, STEP_LENGTH, "dx7")) {
		printf("Failed to initiate mockery of module.\n");
		exit(1);
	}

	if(create_mock_input_signal(
		   &m, "midi",
		   _MIDI,
		   1,
		   _PTR,
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
		   FTYPE_RESOLUTION,
		   44100,
		   TEST_LENGTH,
		   output_signal
		   )) {
		printf("Failed to create mock output signal.\n");
		exit(3);
	}

	memset(input_signal, 0, sizeof(input_signal));
	memset(output_signal, 0, sizeof(output_signal));

	int *program = get_controller(&m, "program", "general");
	*program = 2;

#define MEVENT(a,b) uint8_t a[8]; MidiEvent *b = (MidiEvent *)a; b->length = 4;
#define MEVENT_SET(T,a,b,c) a->data[0] = T | 0; a->data[1] = b; a->data[2] = c;
	
	MEVENT(on1_event, on1); 
	MEVENT(off1_event, off1); 
	MEVENT(on2_event, on2); 
	MEVENT(off2_event, off2); 
	MEVENT(on3_event, on3); 
	MEVENT(off3_event, off3); 
	MEVENT(on4_event, on4); 
	MEVENT(off4_event, off4); 
	MEVENT(on5_event, on5); 
	MEVENT(off5_event, off5); 

	MEVENT_SET(MIDI_NOTE_ON,   on1, 60, 0x7f);
	MEVENT_SET(MIDI_NOTE_OFF, off1, 60, 0x7f);

	MEVENT_SET(MIDI_NOTE_ON,   on2, 62, 0x7f);
	MEVENT_SET(MIDI_NOTE_OFF, off2, 62, 0x7f);

	MEVENT_SET(MIDI_NOTE_ON,   on3, 64, 0x7f);	
	MEVENT_SET(MIDI_NOTE_OFF, off3, 64, 0x7f);	

	MEVENT_SET(MIDI_NOTE_ON,   on4, 66, 0x7f);	
	MEVENT_SET(MIDI_NOTE_OFF, off4, 66, 0x7f);	
	
	MEVENT_SET(MIDI_NOTE_ON,   on5, 68, 0x7f);	
	MEVENT_SET(MIDI_NOTE_OFF, off5, 68, 0x7f);	
	
	input_signal[ 10000] = on1;
	input_signal[ 20000] = off1;
	input_signal[ 30000] = on2;
	input_signal[ 40000] = off2;
	input_signal[ 50000] = on3;
	input_signal[ 60000] = off3;
	input_signal[ 70000] = on4;
	input_signal[ 80000] = off4;
	input_signal[ 90000] = on5;
	input_signal[100000] = off5;
	
	input_signal[ 10000 + 110000] = on1;
	input_signal[ 20000 + 110000] = off1;
	input_signal[ 30000 + 110000] = on2;
	input_signal[ 40000 + 110000] = off2;
	input_signal[ 50000 + 110000] = on3;
	input_signal[ 60000 + 110000] = off3;
	input_signal[ 70000 + 110000] = on4;
	input_signal[ 80000 + 110000] = off4;
	input_signal[ 90000 + 110000] = on5;
	input_signal[100000 + 110000] = off5;
	
	make_a_mockery(&m, MAX_STEPS);

	write_wav(output_signal, output_signal, TEST_LENGTH, "/tmp/mocking.wav");

	(void) plot_wave("/tmp/mocking.wav", 0, 44100 * 5);

	return 0;
}
